// PedalRegistry.h — elenco condiviso dei pedali emulabili.
//
// Ogni sezione che espone il menu a tendina pesca da QUESTO elenco, che e'
// unico per tutte. Un pedale dichiara i propri controlli: la sezione si
// popola di conseguenza, e la sua larghezza segue il numero di controlli.
//
// Perche' i controlli sono descrittori e non parametri veri: i parametri di un
// AudioProcessorValueTreeState si fissano alla costruzione e non si possono
// creare a runtime. Ogni sezione ha quindi una riserva fissa di parametri
// generici normalizzati (sei continui e due a scatti); il pedale scelto decide
// quanti usarne, come chiamarli e in quale intervallo reale mapparli.
// L'automazione dell'host resta stabile cambiando pedale: cambia il significato
// dei parametri, non la loro identita'.
//
// Nomi: evocativi ma non coincidenti con i marchi originali, nello stesso
// spirito di MARCHELLOW per il JCM800. Le topologie sono ingegneria pubblica
// (clipping a diodi, stadi op-amp, reti di tono passive); i nomi commerciali
// restano dei rispettivi proprietari e non vengono usati.
#pragma once

#include <array>
#include <vector>
#include <cstddef>

namespace pedal {

// Otto e non sei: l'equalizzatore grafico a sette bande piu' il livello e'
// il modello con piu' controlli dell'elenco, e detta la dimensione.
constexpr int kMaxKnobs  = 8;   // riserva di parametri continui per sezione
// Uno solo: nessun pedale dell'elenco ne usa due, e ogni posto in piu' e' un
// parametro per sezione che resta vuoto nei preset e nella lista
// dell'automazione dell'host.
constexpr int kMaxSwitch = 1;   // riserva di parametri a scatti per sezione

enum class Category { Overdrive, Distortion, HighGain, Fuzz, Booster, Gate, Equalizer, Compressor };

// Quale algoritmo usa il modello. Dichiarata esplicitamente invece di dedurla
// dall'id: dedurla dai caratteri faceva collidere od_screamer con od_super, e
// avrebbe reso asimmetrico un circuito che non lo e'.
enum class Topology {
    Clean,          // solo livello
    Boost,          // guadagno pulito con taglio dei bassi
    OdSoftSym,      // clipping morbido simmetrico nell'anello di reazione
    OdSoftAsym,     // clipping morbido asimmetrico, due diodi contro uno
    OdJfet,         // risposta a JFET, meno compressione statica
    DistHard,       // clipping duro verso massa
    DistTurbo,      // secondo stadio di clipping in cascata sul modo II
    DistBody,       // clipping duro con rientro controllato delle basse
    HgColor,        // due filtri di colore indipendenti sommati
    HgZone,         // doppio stadio con tre bande e medio parametrico
    FuzzGate,       // asimmetria forte con componente continua
    GateSuppress,   // soppressore con soglia e decadimento, modi Reduction/Mute
    GateHard,       // cancello secco: sopra soglia passa, sotto chiude
    EqGraphic7,     // sette bande fisse a 100/200/400/800/1.6k/3.2k/6.4k piu' livello
    EqNative,       // la torre di tono dell'amplificatore, gia' nella catena
    EqParametric,   // due campane spazzolabili piu' livello
    CompSustain,    // compressore con sustain, attacco e tono
    CompSimple,     // il predecessore, senza controllo di tono
    CompLimiter     // limitatore con rapporto e soglia espliciti
};

inline const char* categoryName (Category c)
{
    switch (c) {
        case Category::Overdrive:  return "Overdrive";
        case Category::Distortion: return "Distortion";
        case Category::HighGain:   return "High Gain";
        case Category::Fuzz:       return "Fuzz";
        case Category::Booster:    return "Booster";
        case Category::Gate:       return "Gate / Noise";
        case Category::Equalizer:  return "Equalizer";
        case Category::Compressor: return "Compressor";
    }
    return "?";
}

// Un controllo continuo: etichetta sul pomello e intervallo reale in cui
// mappare il parametro normalizzato.
struct Knob
{
    const char* label;
    float       min;
    float       max;
    float       def;     // predefinito, nell'intervallo reale
    const char* suffix;  // unita' mostrata, puo' essere vuota
};

// Un controllo a scatti: due o piu' posizioni.
struct Switch
{
    const char*                label;
    const char* options[4];
    int                        numOptions;
    int                        def;
};

struct Model
{
    const char* id;        // stabile, finisce nello stato salvato
    const char* name;      // mostrato nel menu
    Category    cat;
    Topology    topo;
    const char* blurb;     // suggerimento a comparsa

    // Array C e non std::array: con l'inizializzazione aggregata annidata
    // std::array pretenderebbe un secondo livello di graffe su ogni modello.
    Knob   knobs[kMaxKnobs];
    int    numKnobs;
    Switch switches[kMaxSwitch];
    int    numSwitches;
};

namespace detail {
    inline Knob drive (float d = 0.4f) { return { "DRIVE", 0.f, 1.f, d, "" }; }
    inline Knob tone  (float d = 0.5f) { return { "TONE",  0.f, 1.f, d, "" }; }
    inline Knob level (float d = 0.5f) { return { "LEVEL", 0.f, 1.f, d, "" }; }
    inline Switch none() { return { "", { "", "", "", "" }, 0, 0 }; }
    inline Switch mode() { return { "MODE", { "Standard", "Custom", "", "" }, 2, 0 }; }
}

// L'elenco. L'ordine conta: il menu raggruppa per categoria mantenendolo.
inline const std::vector<Model>& models()
{
    using namespace detail;
    static const std::vector<Model> list = {

    // --- Overdrive ---------------------------------------------------------
    { "clean", "Bypass / Clean", Category::Overdrive, Topology::Clean,
      "Nessuna colorazione: lo stadio passa il segnale invariato.",
      { level(1.0f) }, 1, { none() }, 0 },

    { "od_asym", "OD-ONE Asymmetric", Category::Overdrive, Topology::OdSoftAsym,
      "Clipping morbido asimmetrico nell'anello di reazione: due diodi da un lato "
      "e uno dall'altro, e la seconda armonica resta in evidenza.",
      { drive(0.35f), tone(), level() }, 3, { none() }, 0 },

    { "od_screamer", "GREEN SCREAM 808", Category::Overdrive, Topology::OdSoftSym,
      "Clipping morbido simmetrico con forte taglio dei bassi dentro l'anello: la "
      "gobba sui medi che spinge l'amplificatore senza impastare.",
      { drive(), tone(), level() }, 3, { none() }, 0 },

    { "od_super", "SUPER DRIVE W", Category::Overdrive, Topology::OdSoftAsym,
      "L'asimmetrico a due modi: Standard come l'originale, Custom con piu' "
      "guadagno e i bassi meno tagliati.",
      { drive(), tone(), level() }, 3, { mode() }, 1 },

    { "od_blues", "BLUE DRIVER W", Category::Overdrive, Topology::OdJfet,
      "Risposta a JFET, piu' aperta e dinamica: pulisce abbassando il volume della "
      "chitarra. Custom spinge il fronte senza sgranare.",
      { drive(), tone(), level() }, 3, { mode() }, 1 },

    { "od_natural", "NATURAL OD", Category::Overdrive, Topology::OdSoftSym,
      "Compressione dolce e tono aperto, pensato per restare trasparente sul "
      "carattere dell'amplificatore.",
      { drive(), tone(), level() }, 3, { none() }, 0 },

    // --- Distortion --------------------------------------------------------
    { "ds_classic", "DS-ONE Classic", Category::Distortion, Topology::DistHard,
      "Clipping duro simmetrico a diodi verso massa dopo uno stadio op-amp ad alto "
      "guadagno, con la rete di tono che scava i medi.",
      { drive(0.5f), tone(), level() }, 3, { none() }, 0 },

    { "ds_w", "DS-ONE W", Category::Distortion, Topology::DistHard,
      "Il classico con il modo Custom: piu' corpo sui bassi e clipping meno "
      "compresso.",
      { drive(0.5f), tone(), level() }, 3, { mode() }, 1 },

    { "ds_turbo", "TURBO DS II", Category::Distortion, Topology::DistTurbo,
      "Due modi: I e' il classico, II aggiunge un secondo stadio di clipping per un "
      "attacco piu' spesso e sostenuto.",
      { drive(0.5f), tone(), level() }, 3,
      { { "TURBO", { "I", "II", "", "" }, 2, 0 } }, 1 },

    { "ds_mega", "MEGA DS", Category::Distortion, Topology::DistBody,
      "Guadagno alto con un controllo di corpo dedicato alle basse, che tiene saldo "
      "il fondo anche a distorsione estrema.",
      { drive(0.55f), tone(), level(), { "BOTTOM", 0.f, 1.f, 0.5f, "" } }, 4,
      { none() }, 0 },

    // --- High Gain ---------------------------------------------------------
    { "hg_metal", "HEAVY M-2 Chainsaw", Category::HighGain, Topology::HgColor,
      "Due filtri di colore indipendenti su bassi e alti: il timbro a motosega. Con "
      "entrambi al massimo e' il suono svedese.",
      { drive(0.7f), level(),
        { "COLOR LOW", 0.f, 1.f, 0.5f, "" }, { "COLOR HIGH", 0.f, 1.f, 0.5f, "" } }, 4,
      { mode() }, 1 },

    { "hg_zone", "METAL ZONE W", Category::HighGain, Topology::HgZone,
      "Doppio stadio di guadagno con equalizzatore a tre bande e medio parametrico "
      "spazzolabile: la voce piu' completa dell'elenco.",
      { drive(0.6f), level(),
        { "LOW", -15.f, 15.f, 0.f, " dB" }, { "HIGH", -15.f, 15.f, 0.f, " dB" },
        { "MID", -15.f, 15.f, 0.f, " dB" }, { "MID FREQ", 200.f, 5000.f, 800.f, " Hz" } }, 6,
      { mode() }, 1 },

    // --- Fuzz --------------------------------------------------------------
    { "fz_w", "FUZZ ONE W", Category::Fuzz, Topology::FuzzGate,
      "Fuzz a transistor con taglio ruvido e coda lunga; Custom apre i bassi e rende "
      "il decadimento piu' aperto.",
      { { "FUZZ", 0.f, 1.f, 0.6f, "" }, tone(), level() }, 3, { mode() }, 1 },

    // --- Booster -----------------------------------------------------------
    { "bs_clean", "CLEAN BOOST", Category::Booster, Topology::Boost,
      "Guadagno pulito con passa-alto regolabile all'ingresso: serve a spingere lo "
      "stadio successivo, non a distorcere.",
      { { "BOOST", 0.f, 24.f, 6.f, " dB" }, { "LOW CUT", 20.f, 800.f, 80.f, " Hz" } }, 2,
      { none() }, 0 },

    // --- Gate / Noise ------------------------------------------------------
    { "ng_suppress", "NS-TWO Suppressor", Category::Gate, Topology::GateSuppress,
      "Soppressore con soglia e decadimento. In Reduction attenua il fondo lasciando "
      "passare la coda; in Mute chiude del tutto sotto la soglia.",
      { { "THRESHOLD", -80.f, 0.f, -55.f, " dB" }, { "DECAY", 5.f, 800.f, 120.f, " ms" } }, 2,
      { { "MODE", { "Reduction", "Mute", "", "" }, 2, 0 } }, 1 },

    { "ng_hard", "NF-ONE Noise Gate", Category::Gate, Topology::GateHard,
      "Cancello secco: sopra la soglia passa, sotto chiude. Il piu' semplice e il "
      "piu' deciso, adatto al metal a canale chiuso.",
      { { "THRESHOLD", -80.f, 0.f, -60.f, " dB" }, { "RELEASE", 5.f, 500.f, 80.f, " ms" } }, 2,
      { none() }, 0 },

    // --- Equalizer ---------------------------------------------------------
    { "eq_tonestack", "AMP Tone Stack", Category::Equalizer, Topology::EqNative,
      "La torre di tono dell'amplificatore: bassi, medio spazzolabile con Q, "
      "presenza, acuti e aria. E' quella gia' presente nella catena.",
      { { "", 0.f, 1.f, 0.f, "" } }, 0,
      { none() }, 0 },

    { "eq_graphic", "GE-SEVEN Graphic", Category::Equalizer, Topology::EqGraphic7,
      "Sette bande fisse a 100, 200, 400 e 800 Hz, 1.6, 3.2 e 6.4 kHz, piu' il "
      "livello: l'equalizzatore grafico classico da pedaliera.",
      { { "100", -15.f, 15.f, 0.f, " dB" }, { "200", -15.f, 15.f, 0.f, " dB" },
        { "400", -15.f, 15.f, 0.f, " dB" }, { "800", -15.f, 15.f, 0.f, " dB" },
        { "1.6k", -15.f, 15.f, 0.f, " dB" }, { "3.2k", -15.f, 15.f, 0.f, " dB" },
        { "6.4k", -15.f, 15.f, 0.f, " dB" }, { "LEVEL", -15.f, 15.f, 0.f, " dB" } }, 8,
      { none() }, 0 },

    { "eq_param", "EQ-TWENTY Parametric", Category::Equalizer, Topology::EqParametric,
      "Due campane spazzolabili con guadagno e frequenza indipendenti, piu' il "
      "livello: si interviene dove serve invece che su bande fisse.",
      { { "LOW GAIN", -15.f, 15.f, 0.f, " dB" }, { "LOW FREQ", 40.f, 1000.f, 200.f, " Hz" },
        { "HI GAIN", -15.f, 15.f, 0.f, " dB" },  { "HI FREQ", 500.f, 8000.f, 2500.f, " Hz" },
        { "LEVEL", -15.f, 15.f, 0.f, " dB" } }, 5,
      { none() }, 0 },

    // --- Compressor --------------------------------------------------------
    { "cp_sustain", "CS-THREE Sustainer", Category::Compressor, Topology::CompSustain,
      "Comprime i picchi e solleva il debole, con il tono che apre il click della "
      "pennata. L'attacco in senso orario lascia passare il transiente.",
      { { "SUSTAIN", 0.f, 1.f, 0.4f, "" }, { "ATTACK", 1.f, 100.f, 15.f, " ms" },
        { "TONE", -12.f, 12.f, 0.f, " dB" }, { "LEVEL", -12.f, 12.f, 0.f, " dB" } }, 4,
      { none() }, 0 },

    { "cp_simple", "CS-TWO Compressor", Category::Compressor, Topology::CompSimple,
      "Il predecessore senza controllo di tono: solo sustain, attacco e livello. "
      "Piu' schietto e meno colorato.",
      { { "SUSTAIN", 0.f, 1.f, 0.4f, "" }, { "ATTACK", 1.f, 100.f, 20.f, " ms" },
        { "LEVEL", -12.f, 12.f, 0.f, " dB" } }, 3,
      { none() }, 0 },

    { "cp_limit", "LM-THREE Limiter", Category::Compressor, Topology::CompLimiter,
      "Limitatore con rapporto e soglia espliciti: tiene il livello sotto controllo "
      "invece di dare sustain, utile in coda alla catena.",
      { { "THRESHOLD", -40.f, 0.f, -18.f, " dB" }, { "RATIO", 1.5f, 20.f, 4.f, ":1" },
        { "RELEASE", 10.f, 800.f, 150.f, " ms" }, { "LEVEL", -12.f, 12.f, 0.f, " dB" } }, 4,
      { none() }, 0 },

    };
    return list;
}

// Indice per id, 0 (clean) se non trovato.
inline int indexOf (const char* id)
{
    const auto& l = models();
    for (std::size_t i = 0; i < l.size(); ++i) {
        const char* a = l[i].id; const char* b = id;
        while (*a && *a == *b) { ++a; ++b; }
        if (*a == *b) return (int) i;
    }
    return 0;
}

inline const Model& at (int i)
{
    const auto& l = models();
    if (i < 0 || i >= (int) l.size()) return l[0];
    return l[(std::size_t) i];
}

inline int count() { return (int) models().size(); }

} // namespace pedal
