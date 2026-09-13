// Stage 7 — PresetManager implementation.
#include "PresetManager.h"
#include "PedalRegistry.h"
#include "FxRegistry.h"
#include "AmpRegistry.h"
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
    // Le varianti B, C e D. La A resta <Parameters>, cosi' i file di prima si
    // leggono senza conversioni e restano leggibili anche da una versione
    // vecchia del plugin.
    constexpr const char* kChildBanks = "Banks";
    constexpr const char* kBank       = "Bank";
    constexpr const char* kAttrBankId = "id";
    // I modelli scelti nelle sezioni si salvano per NOME e non per numero.
    // L'indice nel registro cambia ogni volta che si aggiunge un pedale in
    // mezzo all'elenco, e un preset salvato prima si ritroverebbe in quella
    // sezione un effetto diverso senza accorgersene. Il nome invece non si
    // muove: e' per questo che ogni modello ne ha uno.
    constexpr const char* kChildModels = "Models";
    constexpr const char* kModelEntry  = "M";

    enum class Reg { Pedal, Fx, Amp };
    struct ModelParam { const char* param; Reg reg; };
    const ModelParam kModelParams[] = {
        { "od_model",    Reg::Pedal }, { "dist_model",  Reg::Pedal },
        { "ng_model",    Reg::Pedal }, { "gate_model",  Reg::Pedal },
        { "comp_model",  Reg::Pedal }, { "eq_model",    Reg::Pedal },
        { "fxdel_model", Reg::Fx },    { "fxch_model",  Reg::Fx },
        { "fxfl_model",  Reg::Fx },    { "fxrv_model",  Reg::Fx },
        { "fxtr_model",  Reg::Fx },    { "amp_model",   Reg::Amp },
    };

    // I primi tre amplificatori non stanno nel registro: hanno parametri propri
    // e vanno nominati qui.
    const char* const kFixedAmps[3] = { "gearsx", "marchellow", "rectifier" };

    juce::String modelIdFor (Reg r, int index)
    {
        switch (r) {
            case Reg::Pedal: return pedal::at (index).id;
            case Reg::Fx:    return fxpedal::at (index).id;
            case Reg::Amp:
                if (index < 3) return kFixedAmps[index < 0 ? 0 : index];
                return ampmodel::at (index - 3).id;
        }
        return {};
    }

    int modelIndexFor (Reg r, const juce::String& id, int fallback)
    {
        if (id.isEmpty()) return fallback;
        switch (r) {
            case Reg::Pedal:
                for (int i = 0; i < pedal::count(); ++i)
                    if (id == pedal::at (i).id) return i;
                break;
            case Reg::Fx:
                for (int i = 0; i < fxpedal::count(); ++i)
                    if (id == fxpedal::at (i).id) return i;
                break;
            case Reg::Amp:
                for (int i = 0; i < 3; ++i) if (id == kFixedAmps[i]) return i;
                for (int i = 0; i < ampmodel::count(); ++i)
                    if (id == ampmodel::at (i).id) return i + 3;
                break;
        }
        return fallback;   // modello sparito: si tiene quello che c'era
    }

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

int PresetManager::scanBankMask (const juce::XmlElement& root)
{
    int mask = 1;                                   // il banco A c'e' sempre
    if (auto* banks = root.getChildByName (kChildBanks))
        for (auto* b : banks->getChildWithTagNameIterator (kBank)) {
            const auto id = b->getStringAttribute (kAttrBankId);
            if (id.isNotEmpty()) {
                const int i = id[0] - 'A';
                if (i > 0 && i < 4) mask |= (1 << i);
            }
        }
    return mask;
}

juce::String PresetManager::bankLetters (int mask)
{
    juce::String out;
    for (int i = 0; i < 4; ++i)
        if (mask & (1 << i)) { if (out.isNotEmpty()) out << " "; out << (char) ('A' + i); }
    return out;
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
                if (auto x = juce::XmlDocument::parse (juce::String::fromUTF8 (bytes, sz))) {
                    ref.category = x->getStringAttribute (kAttrCategory, ref.category);
                    ref.bankMask = scanBankMask (*x);
                }
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
        if (auto x = juce::XmlDocument::parse (f)) {
            ref.category = x->getStringAttribute (kAttrCategory, ref.category);
            ref.bankMask = scanBankMask (*x);
        }
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

void PresetManager::writeModelIds (juce::XmlElement& dest) const
{
    auto* models = dest.createNewChildElement (kChildModels);
    for (const auto& mp : kModelParams)
        if (auto* v = apvts_.getRawParameterValue (mp.param)) {
            auto* e = models->createNewChildElement (kModelEntry);
            e->setAttribute ("p", mp.param);
            e->setAttribute ("id", modelIdFor (mp.reg, (int) v->load()));
        }
}

void PresetManager::applyModelIds (const juce::XmlElement& src)
{
    auto* models = src.getChildByName (kChildModels);
    if (models == nullptr) return;          // preset vecchio: resta com'e'
    for (auto* e : models->getChildWithTagNameIterator (kModelEntry)) {
        const auto pid = e->getStringAttribute ("p");
        const auto mid = e->getStringAttribute ("id");
        const ModelParam* mp = nullptr;
        for (const auto& c : kModelParams) if (pid == c.param) { mp = &c; break; }
        if (mp == nullptr) continue;
        auto* par = apvts_.getParameter (pid);
        if (par == nullptr) continue;
        const int cur = (int) par->convertFrom0to1 (par->getValue());
        const int idx = modelIndexFor (mp->reg, mid, cur);
        par->setValueNotifyingHost (par->convertTo0to1 ((float) idx));
    }
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
    writeModelIds (root);

    return root.toString();
}

bool PresetManager::applyXml (const juce::XmlElement& root, int bank)
{
    if (! root.hasTagName (kRoot)) return false;

    const bool fileLockModel = root.getIntAttribute (kAttrLock, 0) != 0;
    const bool effectiveLock = lockModel_ || fileLockModel;

    currentCategory_ = root.getStringAttribute (kAttrCategory, "Uncategorized");

    loading_ = true;

    // I parametri vengono dal banco richiesto. Se quella variante non c'e' si
    // ripiega sul banco A, che esiste sempre: meglio richiamare il preset nella
    // sua versione base che non richiamarlo affatto.
    const juce::XmlElement* paramXml = nullptr;
    const juce::XmlElement* bankNode = nullptr;
    if (bank > 0) {
        if (auto* banks = root.getChildByName (kChildBanks)) {
            const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + bank));
            for (auto* b : banks->getChildWithTagNameIterator (kBank))
                if (b->getStringAttribute (kAttrBankId) == letter) {
                    bankNode = b;
                    paramXml = b->getChildByName (kChildParams) != nullptr
                             ? b->getChildByName (kChildParams)->getFirstChildElement()
                             : b->getFirstChildElement();
                    break;
                }
        }
    }
    if (paramXml == nullptr)
        if (auto* params = root.getChildByName (kChildParams))
            paramXml = params->getFirstChildElement();

    if (paramXml != nullptr)
    {
        auto vt = juce::ValueTree::fromXml (*paramXml);
        if (vt.isValid())
            apvts_.replaceState (vt);
    }

    // I numeri appena caricati vanno riletti per nome: e' l'unica cosa che
    // sopravvive all'aggiunta di modelli in mezzo agli elenchi. La variante ha
    // i suoi, se li ha; altrimenti valgono quelli del preset.
    if (bankNode != nullptr && bankNode->getChildByName (kChildModels) != nullptr)
        applyModelIds (*bankNode);
    else
        applyModelIds (root);

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
bool PresetManager::isSupportedPresetFile (const juce::File& f)
{
    // Solo i nostri formati: .prs (singolo), .prstl (elenco) e .nampreset
    // (nativo). Qualunque altra estensione viene rifiutata senza aprirla.
    return f.hasFileExtension ("prs") || f.hasFileExtension ("prstl")
        || f.hasFileExtension ("nampreset");
}

int PresetManager::importFrom (const juce::File& f)
{
    if (! f.existsAsFile()) return 0;
    if (! isSupportedPresetFile (f)) return 0;
    auto xml = juce::parseXML (f);
    // Anche con l'estensione giusta il contenuto deve essere nostro: la radice
    // dev'essere un <NAMPreset> o un contenitore che ne racchiude.
    if (! xml) return 0;
    if (! xml->hasTagName (kRoot) && xml->getChildByName (kRoot) == nullptr) return 0;

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

bool PresetManager::saveOver (int index)
{
    if (index < 0 || index >= (int) presets_.size()) return false;
    auto& ref = presets_[(size_t) index];
    if (ref.isFactory) return false;                 // i preset di fabbrica non si toccano

    // Sovrascrivendo il preset caricato si aggiorna solo il banco corrente e si
    // lasciano intatte le altre varianti; sovrascrivendone un altro si riscrive
    // il suo banco A, perche' e' quello il punto di partenza.
    if (index == currentIndex_) return saveBank (ref.name, currentBank_);

    const auto prevName = currentName_;
    const auto prevBank = currentBank_;
    currentName_ = ref.name;
    const bool ok = saveBank (ref.name, 0);
    if (! ok) { currentName_ = prevName; currentBank_ = prevBank; }
    return ok;
}

bool PresetManager::deleteAt (int index)
{
    if (index < 0 || index >= (int) presets_.size()) return false;
    auto& ref = presets_[(size_t) index];
    if (ref.isFactory) return false;
    if (! ref.userFile.deleteFile()) return false;
    if (index == currentIndex_) { currentName_ = {}; currentIndex_ = -1; dirty_ = false; }
    refresh();
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

// Scrive una sola variante dentro il preset, lasciando intatte le altre. Se il
// file non c'e' ancora lo crea, e quel che si sta salvando diventa il banco A
// oltre che quello richiesto: un preset senza banco A non avrebbe un punto di
// partenza da cui caricare.
bool PresetManager::saveBank (const juce::String& name, int bank)
{
    if (name.trim().isEmpty() || isReservedName (name)) return false;
    bank = juce::jlimit (0, 3, bank);

    const auto safe = juce::File::createLegalFileName (name.trim());
    auto file = userPresetDir().getChildFile (safe + kExt);

    // Lo stato di adesso, gia' confezionato: da qui si prendono i parametri.
    auto fresh = juce::parseXML (serialize (safe));
    if (! fresh) return false;
    auto* freshParams = fresh->getChildByName (kChildParams);
    if (freshParams == nullptr) return false;

    std::unique_ptr<juce::XmlElement> root;
    if (file.existsAsFile()) root = juce::parseXML (file);
    if (! root || ! root->hasTagName (kRoot)) {
        // Preset nuovo: parte dallo stato corrente, che diventa anche il banco A.
        root = std::move (fresh);
        if (bank == 0) { /* gia' scritto in <Parameters> e <Models> */ }
        else {
            auto* banks = root->createNewChildElement (kChildBanks);
            auto* b = banks->createNewChildElement (kBank);
            b->setAttribute (kAttrBankId, juce::String::charToString ((juce::juce_wchar) ('A' + bank)));
            b->addChildElement (new juce::XmlElement (*freshParams));
            writeModelIds (*b);
        }
    } else {
        // Preset esistente: si sostituisce solo il banco richiesto, e si
        // aggiornano i percorsi di modello e IR, che sono del preset e non
        // della variante.
        for (const char* tag : { kChildModel, kChildIR, kChildIR2 }) {
            root->removeChildElement (root->getChildByName (tag), true);
            if (auto* src = fresh->getChildByName (tag))
                root->addChildElement (new juce::XmlElement (*src));
        }
        root->setAttribute (kAttrCategory, currentCategory_);
        root->setAttribute (kAttrLock, lockModel_ ? 1 : 0);

        if (bank == 0) {
            root->removeChildElement (root->getChildByName (kChildParams), true);
            root->addChildElement (new juce::XmlElement (*freshParams));
            // I nomi dei modelli vanno riscritti insieme ai parametri, o
            // resterebbero quelli della versione precedente del banco.
            root->removeChildElement (root->getChildByName (kChildModels), true);
            if (auto* fm = fresh->getChildByName (kChildModels))
                root->addChildElement (new juce::XmlElement (*fm));
        } else {
            auto* banks = root->getChildByName (kChildBanks);
            if (banks == nullptr) banks = root->createNewChildElement (kChildBanks);
            const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + bank));
            juce::XmlElement* target = nullptr;
            for (auto* b : banks->getChildWithTagNameIterator (kBank))
                if (b->getStringAttribute (kAttrBankId) == letter) { target = b; break; }
            if (target != nullptr) banks->removeChildElement (target, true);
            auto* b = banks->createNewChildElement (kBank);
            b->setAttribute (kAttrBankId, letter);
            b->addChildElement (new juce::XmlElement (*freshParams));
            writeModelIds (*b);
        }
    }

    if (! file.replaceWithText (root->toString())) return false;

    currentName_ = safe;
    currentBank_ = bank;
    refresh();
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

bool PresetManager::load (int index, int bank)
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

    // Richiamando dalla lista si tiene il banco selezionato, se quel preset ce
    // l'ha; altrimenti si torna al banco A. Dal display il banco arriva invece
    // esplicito, ed e' quello che si carica.
    const int mask = scanBankMask (*xml);
    int wanted = (bank < 0) ? currentBank_ : juce::jlimit (0, 3, bank);
    if ((mask & (1 << wanted)) == 0) wanted = 0;

    if (! applyXml (*xml, wanted)) return false;

    // Il banco selezionato e' anch'esso un parametro, quindi replaceState lo
    // riporta a quello scritto nel file: senza questo, richiamando la variante
    // B il display tornerebbe a dire A un istante dopo.
    if (auto* pb = apvts_.getParameter ("preset_bank"))
        pb->setValueNotifyingHost (pb->convertTo0to1 ((float) wanted));

    currentIndex_ = index;
    currentName_  = ref.name;
    currentBank_  = wanted;
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
