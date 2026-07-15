// Stage 7 — PresetManager implementation.
#include "PresetManager.h"
#include "PluginProcessor.h"
#include "NAMPresetData.h"


namespace
{
    constexpr const char* kExt        = ".nampreset";
    constexpr const char* kRoot       = "NAMPreset";
    constexpr const char* kAttrName   = "name";
    constexpr const char* kAttrVer    = "version";
    constexpr const char* kAttrLock   = "lockModel";
    constexpr const char* kAttrCategory = "category";
    constexpr const char* kChildModel = "ModelPath";
    constexpr const char* kChildIR    = "IRPath";
    constexpr const char* kChildParams= "Parameters";

    static juce::String stem (const juce::File& f) { return f.getFileNameWithoutExtension(); }

    // H3 fix (2026-07-15 audit): allowlist for preset-referenced file paths.
    // A user-crafted .nampreset used to carry arbitrary <ModelPath>/<IRPath>
    // strings that PresetManager would blindly pass to juce::File(...) +
    // loadModelAsync. That enabled info-disclosure (probing existence of
    // /etc/passwd, C:\Windows\System32\config\SAM, ...) and DoS via
    // over-sized file opens. We now require the resolved path to sit under
    // one of the standard user/system data roots where NAM assets legitimately
    // live. isAChildOf works on canonicalised paths, so ../.. tricks that
    // land outside the allowed roots are rejected automatically.
    static bool isPresetPathAllowed (const juce::File& f)
    {
        if (f == juce::File{}) return false;
        if (f.getFullPathName().isEmpty()) return false;
        static const juce::File roots[] = {
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory),
            juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory),
            juce::File::getSpecialLocation (juce::File::commonDocumentsDirectory),
            juce::File::getSpecialLocation (juce::File::tempDirectory),
        };
        for (const auto& r : roots)
            if (r != juce::File{} && (f == r || f.isAChildOf (r))) return true;
        return false;
    }
}

PresetManager::PresetManager (NAMAudioProcessor& proc,
                              juce::AudioProcessorValueTreeState& state)
    : processor_ (proc), apvts_ (state)
{
    // Register listener on every parameter so any UI/host edit marks dirty.
    for (auto* p : processor_.getParameters())
    {
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            apvts_.addParameterListener (withID->paramID, this);
    }

    refresh();
}

PresetManager::~PresetManager()
{
    for (auto* p : processor_.getParameters())
    {
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            apvts_.removeParameterListener (withID->paramID, this);
    }
}

juce::File PresetManager::userPresetDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("NAMCustom")
                .getChildFile ("Presets");
    if (! d.exists()) d.createDirectory();
    return d;
}

void PresetManager::refresh()
{
    presets_.clear();

    // Factory from NAMPresetData.
    for (int i = 0; i < NAMPresetData::namedResourceListSize; ++i)
    {
        const auto* originalName = NAMPresetData::originalFilenames[i];
        const juce::String orig (originalName);
        if (! orig.endsWithIgnoreCase (kExt)) continue;
        PresetRef ref;
        ref.name         = juce::File (orig).getFileNameWithoutExtension();
        ref.isFactory    = true;
        ref.factoryIndex = i;
        {
            int sz = 0;
            if (auto* bytes = NAMPresetData::getNamedResource (NAMPresetData::namedResourceList[i], sz))
                if (auto x = juce::XmlDocument::parse (juce::String::fromUTF8 (bytes, sz)))
                    ref.category = x->getStringAttribute (kAttrCategory, ref.category);
        }
        presets_.push_back (ref);
    }

    // User from disk.
    auto dir = userPresetDir();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, juce::String ("*") + kExt);
    for (auto& f : files)
    {
        PresetRef ref;
        ref.name      = stem (f);
        ref.isFactory = false;
        ref.userFile  = f;
        if (auto x = juce::XmlDocument::parse (f))
            ref.category = x->getStringAttribute (kAttrCategory, ref.category);
        presets_.push_back (ref);
    }

    // Re-locate currentIndex_ by name if still present.
    currentIndex_ = -1;
    for (size_t i = 0; i < presets_.size(); ++i)
        if (presets_[i].name == currentName_) { currentIndex_ = (int) i; break; }

    notify();
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! loading_) markDirty();
}

void PresetManager::markDirty()
{
    if (! dirty_) { dirty_ = true; notify(); }
}

void PresetManager::notify()
{
    if (onChanged) juce::MessageManager::callAsync ([cb = onChanged]() { cb(); });
}

juce::String PresetManager::serialize (const juce::String& name) const
{
    juce::XmlElement root (kRoot);
    root.setAttribute (kAttrName, name);
    root.setAttribute (kAttrVer, 1);
    root.setAttribute (kAttrLock, lockModel_ ? 1 : 0);
    root.setAttribute (kAttrCategory, currentCategory_);

    auto* mp = root.createNewChildElement (kChildModel);
    mp->addTextElement (processor_.getCurrentModelPath());

    auto* ip = root.createNewChildElement (kChildIR);
    ip->addTextElement (processor_.getCurrentIRPath());

    auto* params = root.createNewChildElement (kChildParams);
    if (auto state = apvts_.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            params->addChildElement (xml.release());
    }

    return root.toString();
}

bool PresetManager::applyXml (const juce::XmlElement& root)
{
    if (! root.hasTagName (kRoot)) return false;

    const bool fileLockModel = root.getIntAttribute (kAttrLock, 0) != 0;
    const bool effectiveLock = lockModel_ || fileLockModel;

    currentCategory_ = root.getStringAttribute (kAttrCategory, "Uncategorized");

    loading_ = true;

    if (auto* params = root.getChildByName (kChildParams))
    {
        if (auto* paramXml = params->getFirstChildElement())
        {
            auto vt = juce::ValueTree::fromXml (*paramXml);
            if (vt.isValid())
                apvts_.replaceState (vt);
        }
    }

    if (! effectiveLock)
    {
        const auto mp = root.getChildByName (kChildModel);
        const auto ip = root.getChildByName (kChildIR);
        const juce::String mpath = mp ? mp->getAllSubText().trim() : juce::String();
        const juce::String ipath = ip ? ip->getAllSubText().trim() : juce::String();

        auto resolveBundled = [] (const juce::String& name) -> juce::File
        {
            // H3 fix: bundled name must be a bare filename. Reject any path
            // separator (both '/' and '\\' — the original code only blocked
            // '/', letting ..\..\evil.nam through on Windows) and any ".."
            // component so a hostile preset can't reach outside the bundled
            // temp dir.
            if (name.isEmpty()
                || name.containsChar ('/')
                || name.containsChar ('\\')
                || name.contains ("..")) return {};
            int sz = 0;
            const char* bytes = nullptr;
            for (int i = 0; i < NAMPresetData::namedResourceListSize; ++i)
            {
                if (name.equalsIgnoreCase (NAMPresetData::originalFilenames[i]))
                {
                    bytes = NAMPresetData::getNamedResource (NAMPresetData::namedResourceList[i], sz);
                    break;
                }
            }
            if (bytes == nullptr || sz <= 0) return {};
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("NAMCustom").getChildFile ("bundled");
            if (! dir.exists()) dir.createDirectory();
            auto out = dir.getChildFile (name);
            if (! out.existsAsFile() || out.getSize() != (juce::int64) sz)
                out.replaceWithData (bytes, (size_t) sz);
            return out;
        };

        // H3 fix: gate absolute paths from the preset behind the allowlist.
        // A path that resolves outside the standard user/system data roots is
        // treated as if the file did not exist; we still try the bundled
        // fallback so factory presets keep working, then clear on miss.
        const juce::File mfile (mpath);
        if (mpath.isNotEmpty() && isPresetPathAllowed (mfile) && mfile.existsAsFile())
            processor_.loadModelAsync (mfile);
        else if (auto bundled = resolveBundled (mpath); bundled.existsAsFile())
            processor_.loadModelAsync (bundled);
        else
            processor_.clearModel();

        const juce::File ifile (ipath);
        if (ipath.isNotEmpty() && isPresetPathAllowed (ifile) && ifile.existsAsFile())
            processor_.loadIRAsync (ifile);
        else
            processor_.clearIR();
    }

    loading_ = false;
    dirty_   = false;
    return true;
}

bool PresetManager::save()
{
    if (currentIndex_ < 0) return false;
    auto& ref = presets_[(size_t) currentIndex_];
    if (ref.isFactory) return false; // can't overwrite factory
    ref.userFile.replaceWithText (serialize (ref.name));
    dirty_ = false;
    notify();
    return true;
}

bool PresetManager::saveAs (const juce::String& name)
{
    if (name.trim().isEmpty()) return false;
    auto safe = juce::File::createLegalFileName (name.trim());
    auto file = userPresetDir().getChildFile (safe + kExt);
    if (! file.replaceWithText (serialize (safe))) return false;

    currentName_ = safe;
    refresh(); // rebuilds list, locates currentIndex_ by name
    dirty_ = false;
    notify();
    return true;
}

bool PresetManager::deleteCurrent()
{
    if (currentIndex_ < 0) return false;
    auto& ref = presets_[(size_t) currentIndex_];
    if (ref.isFactory) return false;
    if (! ref.userFile.deleteFile()) return false;
    currentName_  = {};
    currentIndex_ = -1;
    dirty_        = false;
    refresh();
    return true;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) presets_.size()) return false;
    const auto& ref = presets_[(size_t) index];

    std::unique_ptr<juce::XmlElement> xml;
    if (ref.isFactory)
    {
        int size = 0;
        const char* data = NAMPresetData::getNamedResource (
            NAMPresetData::namedResourceList[ref.factoryIndex], size);
        if (data == nullptr || size <= 0) return false;
        xml = juce::parseXML (juce::String::fromUTF8 (data, size));
    }
    else
    {
        xml = juce::parseXML (ref.userFile);
    }

    if (! xml) return false;
    if (! applyXml (*xml)) return false;

    currentIndex_ = index;
    currentName_  = ref.name;
    notify();
    return true;
}

void PresetManager::next()
{
    if (presets_.empty()) return;
    int i = (currentIndex_ + 1) % (int) presets_.size();
    load (i);
}

void PresetManager::prev()
{
    if (presets_.empty()) return;
    int i = currentIndex_ <= 0 ? (int) presets_.size() - 1 : currentIndex_ - 1;
    load (i);
}
