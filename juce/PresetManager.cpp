// Stage 7 — PresetManager implementation.
#include "PresetManager.h"
#include <map>
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
    constexpr const char* kChildIR2   = "IR2Path";
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

    auto* ip2 = root.createNewChildElement (kChildIR2);
    ip2->addTextElement (processor_.getCurrentIR2Path());

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
        const auto ip2 = root.getChildByName (kChildIR2);
        const juce::String ipath2 = ip2 ? ip2->getAllSubText().trim() : juce::String();

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

        const juce::File ifile2 (ipath2);
        if (ipath2.isNotEmpty() && isPresetPathAllowed (ifile2) && ifile2.existsAsFile())
            processor_.loadIR2Async (ifile2);
        else
            processor_.clearIR2();
    }

    loading_ = false;
    dirty_   = false;
    return true;
}

// .prs e .prstl condividono il formato XML dei preset nativi: il primo
// contiene un solo <NAMPreset>, il secondo un contenitore con piu' di uno.
// Si accetta anche .nampreset, che e' lo stesso contenuto con altro nome.
// I nomi gia' presenti vengono resi univoci con un suffisso numerico, cosi'
// un'importazione non sovrascrive mai il lavoro dell'utente.
int PresetManager::importFrom (const juce::File& f)
{
    if (! f.existsAsFile()) return 0;
    auto xml = juce::parseXML (f);
    if (! xml) return 0;

    std::vector<const juce::XmlElement*> items;
    if (xml->hasTagName (kRoot))
        items.push_back (xml.get());
    else
        for (auto* c : xml->getChildIterator())
            if (c->hasTagName (kRoot)) items.push_back (c);

    // Un .prstl di backup porta con se' i file .nam e .wav in base64: si
    // scrivono accanto ai preset e si tiene la mappa nome -> percorso, con cui
    // i riferimenti dentro i preset vengono riscritti in percorsi validi qui.
    std::map<juce::String, juce::String> restored;
    if (auto* assets = xml->getChildByName ("Assets"))
    {
        auto dir = userPresetDir().getParentDirectory().getChildFile ("Assets");
        dir.createDirectory();
        for (auto* a : assets->getChildIterator())
        {
            const auto name = a->getStringAttribute ("name").trim();
            // Solo nomi nudi: un separatore o un ".." permetterebbe a un file
            // ostile di scrivere fuori dalla cartella degli asset.
            if (name.isEmpty() || name.containsChar ('/') || name.containsChar ('\\')
                || name.contains ("..")) continue;
            juce::MemoryOutputStream raw;
            if (! juce::Base64::convertFromBase64 (raw, a->getAllSubText().trim())) continue;
            auto dest = dir.getChildFile (name);
            if (dest.replaceWithData (raw.getData(), raw.getDataSize()))
                restored[name] = dest.getFullPathName();
        }
    }

    int written = 0;
    for (auto* el : items)
    {
        auto name = el->getStringAttribute (kAttrName, "Imported").trim();
        if (name.isEmpty()) name = "Imported";
        if (isReservedName (name)) name += " (importato)";

        auto safe = juce::File::createLegalFileName (name);
        auto dest = userPresetDir().getChildFile (safe + kExt);
        for (int i = 2; dest.existsAsFile() && i < 1000; ++i)
            dest = userPresetDir().getChildFile (safe + " (" + juce::String (i) + ")" + kExt);

        // Riscrive i riferimenti agli asset ripristinati prima di salvare.
        juce::XmlElement copy (*el);
        for (auto* tag : { kChildModel, kChildIR, kChildIR2 })
            if (auto* pe = copy.getChildByName (tag)) {
                auto it = restored.find (pe->getAllSubText().trim());
                if (it != restored.end()) {
                    pe->deleteAllTextElements();
                    pe->addTextElement (it->second);
                }
            }

        if (dest.replaceWithText (copy.toString())) ++written;
    }

    if (written > 0) refresh();
    return written;
}

bool PresetManager::exportCurrent (const juce::File& dest)
{
    if (currentIndex_ < 0) return false;
    const auto& ref = presets_[(size_t) currentIndex_];
    return dest.replaceWithText (serialize (ref.name));
}

// Gli asset vengono incorporati una sola volta ciascuno anche se piu' preset
// puntano allo stesso file. Nel preset scritto dentro il backup il percorso
// assoluto viene sostituito dal solo nome del file, che e' la chiave con cui
// l'asset viene ritrovato in fase di ripristino.
int PresetManager::exportAll (const juce::File& dest)
{
    juce::XmlElement root ("NAMPresetList");
    root.setAttribute ("version", 1);

    auto* assets = new juce::XmlElement ("Assets");
    juce::StringArray embedded;

    auto embed = [&] (const juce::String& path, const char* kind) -> juce::String
    {
        if (path.isEmpty()) return {};
        juce::File f (path);
        // Un percorso che non e' un file su disco (per esempio il nome di un
        // asset gia' incorporato nel binario) si lascia com'e'.
        if (! f.existsAsFile()) return path;
        const auto name = f.getFileName();
        if (! embedded.contains (name)) {
            juce::MemoryBlock mb;
            if (f.loadFileAsData (mb)) {
                auto* a = assets->createNewChildElement ("Asset");
                a->setAttribute ("name", name);
                a->setAttribute ("kind", kind);
                a->addTextElement (juce::Base64::toBase64 (mb.getData(), mb.getSize()));
                embedded.add (name);
            }
        }
        return name;
    };

    int count = 0;
    for (const auto& ref : presets_)
    {
        std::unique_ptr<juce::XmlElement> xml;
        if (ref.isFactory) {
            int size = 0;
            const char* data = NAMPresetData::getNamedResource (
                NAMPresetData::namedResourceList[ref.factoryIndex], size);
            if (data == nullptr || size <= 0) continue;
            xml = juce::parseXML (juce::String::fromUTF8 (data, size));
        } else {
            xml = juce::parseXML (ref.userFile);
        }
        if (! xml || ! xml->hasTagName (kRoot)) continue;

        for (auto* tag : { kChildModel, kChildIR, kChildIR2 })
            if (auto* el = xml->getChildByName (tag)) {
                const auto repl = embed (el->getAllSubText().trim(),
                                         juce::String (tag) == kChildModel ? "nam" : "ir");
                el->deleteAllTextElements();
                if (repl.isNotEmpty()) el->addTextElement (repl);
            }

        root.addChildElement (new juce::XmlElement (*xml));
        ++count;
    }

    root.addChildElement (assets);   // il contenitore passa a root, che lo libera
    return dest.replaceWithText (root.toString()) ? count : 0;
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
    // "Default" e' il preset neutro di fabbrica: modificando i parametri si e'
    // obbligati a salvare sotto un altro nome, cosi' il punto di partenza
    // resta intatto.
    if (isReservedName (name)) return false;
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
