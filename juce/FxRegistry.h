// FxRegistry.h — l'elenco dei pedali per le sezioni della scheda FX.
//
// Stesso meccanismo del registro principale (PedalRegistry.h) ma elenco a
// parte: nella scheda FX hanno senso ritardi, modulazioni e riverberi, non
// distorsori. L'elenco e' unico per tutte le sezioni della scheda, raggruppato
// per categoria, e ogni categoria si apre con l'effetto originario della
// catena, cosi' scegliendolo si torna esattamente al suono di prima.
//
// I nomi sono evocativi ma distinti dai marchi reali; le topologie invece
// seguono i circuiti pubblici e i comandi che quei pedali hanno davvero.
#pragma once

#include "PedalRegistry.h"
#include <vector>
#include <cstring>

namespace fxpedal {

// Cinque comandi continui bastano al modello piu' ricco dell'elenco
// (l'ensemble con livello, velocita', profondita' e i due filtri).
constexpr int kMaxKnobs  = 5;
constexpr int kMaxSwitch = 1;

using Knob   = pedal::Knob;
using Switch = pedal::Switch;

enum class Category { Delay, Chorus, Flanger, Reverb, Tremolo };

enum class Topology {
    DelayStd,       // il ritardo della catena, ripetizioni pulite
    DelayAnalog,    // secchi a BBD: ripetizioni che si scuriscono a ogni giro
    DelayDigital,   // ripetizioni fedeli, con il selettore della portata
    DelayMod,       // ripetizioni appena ondeggianti, sapore di nastro
    ChorusStd,      // il coro della catena
    ChorusCe2,      // una voce sola, dolce e scura: velocita' e profondita'
    ChorusCe5,      // due voci in quadratura piu' i filtri acuti e bassi
    Vibrato,        // solo segnale modulato, senza il diretto: vibrato vero
    FlangerStd,     // il flanger della catena
    FlangerBf2,     // manuale, profondita', velocita' e risonanza
    FlangerBf3,     // come sopra, con il modo Ultra che allarga tutto
    ReverbStd,      // il riverbero della catena
    ReverbSix,      // stanza, sala, piastra o molla, a scelta
    ReverbMod,      // la coda respira, modulata piano
    TremStd,        // il tremolo della catena
    TremTr2,        // onda regolabile dal triangolo all'onda quadra
    TremSlicer      // tremolo a passi, con motivi ritmici
};

struct Model
{
    const char* id;
    const char* name;
    Category    cat;
    Topology    topo;
    const char* blurb;
    Knob   knobs[kMaxKnobs];
    int    numKnobs;
    Switch switches[kMaxSwitch];
    int    numSwitches;
};

namespace detail {
    inline Knob   mix   (float d = 0.25f) { return { "MIX",   0.f, 1.f, d, "" }; }
    inline Knob   rate  (float d = 0.8f)  { return { "RATE",  0.05f, 10.f, d, " Hz" }; }
    inline Knob   depth (float d = 0.4f)  { return { "DEPTH", 0.f, 1.f, d, "" }; }
    inline Switch none()                  { return { "", { "", "", "", "" }, 0, 0 }; }
}

inline const std::vector<Model>& models()
{
    using namespace detail;
    static const std::vector<Model> list = {

    // --- Delay -------------------------------------------------------------
    { "dl_std", "STANDARD Delay", Category::Delay, Topology::DelayStd,
      "Il ritardo che la catena ha sempre avuto: ripetizioni pulite, tempo, "
      "reazione e dosaggio.",
      { { "TIME", 20.f, 1200.f, 320.f, " ms" }, { "FEEDBACK", 0.f, 0.9f, 0.35f, "" },
        mix (0.25f) }, 3,
      { none() }, 0 },

    { "dl_analog", "DM-TWO Analog", Category::Delay, Topology::DelayAnalog,
      "Secchi analogici: ogni ripetizione torna piu' scura e piu' morbida della "
      "precedente, e spinta si impasta invece di sgranare. Tempo corto, come "
      "vuole il circuito a secchi.",
      { { "REPEAT RATE", 20.f, 300.f, 180.f, " ms" }, { "INTENSITY", 0.f, 0.9f, 0.4f, "" },
        { "ECHO", 0.f, 1.f, 0.3f, "" } }, 3,
      { none() }, 0 },

    { "dl_digital", "DD-THREE Digital", Category::Delay, Topology::DelayDigital,
      "Ripetizioni fedeli all'originale, senza colore. Il selettore sceglie la "
      "portata del tempo, come sul pedale.",
      { { "D.TIME", 0.f, 1.f, 0.45f, "" }, { "F.BACK", 0.f, 0.9f, 0.3f, "" },
        { "E.LEVEL", 0.f, 1.f, 0.3f, "" } }, 3,
      { { "MODE", { "50 ms", "200 ms", "800 ms", "" }, 3, 2 } }, 1 },

    { "dl_mod", "DD-TWENTY Modulate", Category::Delay, Topology::DelayMod,
      "Le ripetizioni ondeggiano appena, e la coda si allarga invece di ripetere "
      "identica: il sapore del nastro senza il suo rumore.",
      { { "TIME", 20.f, 1200.f, 420.f, " ms" }, { "FEEDBACK", 0.f, 0.9f, 0.45f, "" },
        { "E.LEVEL", 0.f, 1.f, 0.3f, "" }, { "MOD", 0.f, 1.f, 0.35f, "" } }, 4,
      { none() }, 0 },

    // --- Chorus ------------------------------------------------------------
    { "ch_std", "STANDARD Chorus", Category::Chorus, Topology::ChorusStd,
      "Il coro che la catena ha sempre avuto: una voce modulata mescolata al "
      "diretto.",
      { rate (0.8f), depth (0.4f), mix (0.3f) }, 3,
      { none() }, 0 },

    { "ch_ce2", "CE-TWO Chorus", Category::Chorus, Topology::ChorusCe2,
      "Una voce sola a secchi, dolce e scura, col dosaggio fisso come "
      "sull'originale: bastano velocita' e profondita'.",
      { rate (0.5f), depth (0.5f) }, 2,
      { none() }, 0 },

    { "ch_ce5", "CE-FIVE Ensemble", Category::Chorus, Topology::ChorusCe5,
      "Due voci sfasate di un quarto di giro, che allargano invece di ondeggiare, "
      "piu' i due filtri per togliere acuti o bassi al solo effetto.",
      { { "E.LEVEL", 0.f, 1.f, 0.4f, "" }, rate (0.4f), depth (0.5f),
        { "FILTER HI", 1000.f, 12000.f, 8000.f, " Hz" },
        { "FILTER LO", 20.f, 800.f, 80.f, " Hz" } }, 5,
      { none() }, 0 },

    { "ch_vibrato", "VB-TWO Vibrato", Category::Chorus, Topology::Vibrato,
      "Niente segnale diretto: resta solo quello modulato, quindi si sente "
      "l'intonazione ondeggiare e non il battimento del coro. Il tempo di salita "
      "dice quanto ci mette ad arrivare a fondo.",
      { rate (5.f), depth (0.35f), { "RISE", 0.f, 2000.f, 300.f, " ms" } }, 3,
      { none() }, 0 },

    // --- Flanger -----------------------------------------------------------
    { "fl_std", "STANDARD Flanger", Category::Flanger, Topology::FlangerStd,
      "Il flanger che la catena ha sempre avuto: ritardo cortissimo, reazione e "
      "dosaggio.",
      { rate (0.3f), depth (0.5f), { "FEEDBACK", 0.f, 0.9f, 0.4f, "" }, mix (0.25f) }, 4,
      { none() }, 0 },

    { "fl_bf2", "BF-TWO Flanger", Category::Flanger, Topology::FlangerBf2,
      "I quattro comandi dell'originale: il manuale sposta il centro della "
      "spazzolata, la risonanza decide quanto stride. Col manuale basso e la "
      "risonanza alta si ottiene il getto d'aria.",
      { { "MANUAL", 0.3f, 10.f, 2.f, " ms" }, depth (0.6f), rate (0.3f),
        { "RESONANCE", 0.f, 0.95f, 0.5f, "" } }, 4,
      { none() }, 0 },

    { "fl_bf3", "BF-THREE Flanger", Category::Flanger, Topology::FlangerBf3,
      "Gli stessi comandi con il modo Ultra, che allarga la spazzolata e spinge "
      "la reazione fino al fischio.",
      { { "MANUAL", 0.3f, 10.f, 2.f, " ms" }, depth (0.6f), rate (0.3f),
        { "RESONANCE", 0.f, 0.95f, 0.5f, "" } }, 4,
      { { "MODE", { "Normal", "Ultra", "", "" }, 2, 0 } }, 1 },

    // --- Reverb ------------------------------------------------------------
    { "rv_std", "STANDARD Reverb", Category::Reverb, Topology::ReverbStd,
      "Il riverbero che la catena ha sempre avuto: dimensione, smorzamento e "
      "dosaggio.",
      { { "ROOM", 0.f, 1.f, 0.5f, "" }, { "DAMP", 0.f, 1.f, 0.5f, "" }, mix (0.25f) }, 3,
      { none() }, 0 },

    { "rv_six", "RV-SIX Reverb", Category::Reverb, Topology::ReverbSix,
      "Quattro ambienti a scelta: stanza asciutta, sala lunga, piastra densa e "
      "brillante, molla con il suo tonfo. Il tono chiude la coda senza toccare "
      "il diretto.",
      { { "E.LEVEL", 0.f, 1.f, 0.3f, "" }, { "TONE", -12.f, 12.f, 0.f, " dB" },
        { "TIME", 0.f, 1.f, 0.5f, "" } }, 3,
      { { "MODE", { "Room", "Hall", "Plate", "Spring" }, 4, 0 } }, 1 },

    { "rv_mod", "RV-SIX Modulate", Category::Reverb, Topology::ReverbMod,
      "La coda respira: una modulazione lenta la tiene in movimento, e non si "
      "sente mai ferma sulle note tenute.",
      { { "E.LEVEL", 0.f, 1.f, 0.3f, "" }, { "TONE", -12.f, 12.f, 0.f, " dB" },
        { "TIME", 0.f, 1.f, 0.6f, "" }, { "MOD", 0.f, 1.f, 0.4f, "" } }, 4,
      { none() }, 0 },

    // --- Tremolo -----------------------------------------------------------
    { "tr_std", "STANDARD Tremolo", Category::Tremolo, Topology::TremStd,
      "Il tremolo che la catena ha sempre avuto: velocita', profondita' e forma "
      "d'onda.",
      { rate (4.f), depth (0.5f), { "SHAPE", 0.f, 1.f, 0.f, "" } }, 3,
      { none() }, 0 },

    { "tr_tr2", "TR-TWO Tremolo", Category::Tremolo, Topology::TremTr2,
      "L'onda passa senza scatti dal triangolo all'onda quadra: da un'ondata "
      "morbida a un battito netto.",
      { rate (5.f), depth (0.6f), { "WAVE", 0.f, 1.f, 0.3f, "" } }, 3,
      { none() }, 0 },

    { "tr_slicer", "SL-TWENTY Slicer", Category::Tremolo, Topology::TremSlicer,
      "Non un'onda ma un motivo a passi: il volume viene tagliato a tempo "
      "secondo lo schema scelto.",
      { rate (2.f), depth (0.9f), { "SMOOTH", 0.f, 1.f, 0.25f, "" } }, 3,
      { { "PATTERN", { "Otto", "Terzine", "Salto", "Sincope" }, 4, 0 } }, 1 },
    };
    return list;
}

inline int   count()          { return (int) models().size(); }
inline const Model& at (int i)
{
    const auto& l = models();
    if (i < 0) i = 0;
    if (i >= (int) l.size()) i = (int) l.size() - 1;
    return l[(size_t) i];
}
inline int indexOf (const char* id)
{
    const auto& l = models();
    for (int i = 0; i < (int) l.size(); ++i)
        if (std::strcmp (l[(size_t) i].id, id) == 0) return i;
    return 0;
}

inline const char* categoryName (Category c)
{
    switch (c) {
        case Category::Delay:   return "Delay";
        case Category::Chorus:  return "Chorus";
        case Category::Flanger: return "Flanger";
        case Category::Reverb:  return "Reverb";
        case Category::Tremolo: return "Tremolo";
    }
    return "?";
}

} // namespace fxpedal
