// AmpRegistry.h — l'elenco degli amplificatori a riserva condivisa.
//
// I primi tre amplificatori della catena (GEAR SX, MARCHELLOW, MAUSE RECTIFIER)
// hanno parametri propri e una pagina disegnata su misura. Da qui in poi si
// usa lo stesso schema dei pedali: un elenco di modelli che dichiarano i loro
// comandi, e una riserva sola di parametri che tutti condividono. Sei teste
// con parametri dedicati ne avrebbero voluti quasi novanta; cosi' ne bastano
// ventinove, e aggiungerne una settima non ne costa nessuno.
//
// Ogni comando dichiara anche a quale canale appartiene, cosi' l'interfaccia
// puo' mettere una fila per canale e attenuare quelli che non stanno suonando,
// come fa gia' il Rectifier.
#pragma once

#include <vector>
#include <cstring>

namespace ampmodel {

// Ventiquattro perche' la testa a quattro canali, con tre bande, guadagno e
// volume per canale piu' presenza e profondita', arriva a ventidue; quella coi
// cursori dell'equalizzatore grafico a ventitre'.
constexpr int kMaxKnobs  = 24;
constexpr int kMaxSwitch = 4;

struct Knob
{
    const char* label;
    float       min, max, def;
    const char* suffix;
    int         group;      // 0..3 il canale a cui appartiene, -1 se globale
};

struct Switch
{
    const char* label;
    const char* options[4];
    int         numOptions;
    int         def;
};

enum class Topology {
    Slo,        // due canali, torre di tono condivisa, presenza e profondita'
    Ecstasy,    // tre canali, struttura alta o bassa, pre-equalizzazione
    Vh4,        // quattro canali completamente separati
    Trinity,    // tre canali, il primo con la sua torre, gli altri in comune
    Revo,       // tre canali con messa a fuoco propria
    Mark5       // tre canali piu' l'equalizzatore grafico a cinque bande
};

struct Model
{
    const char* id;
    const char* name;
    Topology    topo;
    // A quale testa si ispira, scritto per esteso: il nome di fantasia dice a
    // cosa somiglia solo a chi lo sa gia'.
    const char* reference;
    const char* blurb;
    Knob   knobs[kMaxKnobs];
    int    numKnobs;
    Switch switches[kMaxSwitch];
    int    numSwitches;
    int    channelSwitch;   // quale interruttore sceglie il canale, -1 se nessuno
    int    numChannels;
};

namespace detail {
    inline Knob gain  (int ch, float d = 0.5f)  { return { "GAIN",     0.f, 1.f, d, "", ch }; }
    inline Knob vol   (int ch, float d = -12.f) { return { "VOLUME", -40.f, 6.f, d, " dB", ch }; }
    inline Knob bass  (int ch, float d = 0.f)   { return { "BASS",   -15.f, 15.f, d, " dB", ch }; }
    inline Knob mid   (int ch, float d = 0.f)   { return { "MIDDLE", -15.f, 15.f, d, " dB", ch }; }
    inline Knob treb  (int ch, float d = 0.f)   { return { "TREBLE", -15.f, 15.f, d, " dB", ch }; }
    inline Knob pres  (float d = 0.f)           { return { "PRESENCE", -12.f, 12.f, d, " dB", -1 }; }
    inline Knob depth (float d = 0.f)           { return { "DEPTH",    -12.f, 12.f, d, " dB", -1 }; }
    inline Switch none() { return { "", { "", "", "", "" }, 0, 0 }; }
}

inline const std::vector<Model>& models()
{
    using namespace detail;
    static const std::vector<Model> list = {

    { "amp_slo", "SOLDERANO", Topology::Slo,
      "Soldano SLO-100",
      "Due canali con guadagno e volume propri e una torre di tono sola per "
      "tutti e due. La profondita' lavora sul fondo del finale insieme alla "
      "presenza: insieme fanno molto piu' di una torre di tono normale. Il "
      "canale pulito ha anche il modo Crunch, che aggiunge uno stadio.",
      { gain (0, 0.35f), vol (0, -10.f),
        gain (1, 0.6f),  vol (1, -14.f),
        bass (-1), mid (-1), treb (-1), pres (0.f), depth (0.f) }, 9,
      { { "CHANNEL", { "Normal", "Overdrive", "", "" }, 2, 1 },
        { "NORMAL",  { "Clean", "Crunch", "", "" }, 2, 0 }, none(), none() }, 2,
      0, 2 },

    { "amp_ecstasy", "BUGNER", Topology::Ecstasy,
      "Bogner Ecstasy 101B",
      "Tre canali: il primo con la sua torre di tono, gli altri due che se ne "
      "dividono una. La struttura sceglie fra una cascata piu' bassa e una piu' "
      "alta, e la pre-equalizzazione decide quanto fondo arriva al guadagno. "
      "L'interruttore di classe cambia il modo di lavorare del finale.",
      { gain (0, 0.3f), vol (0, -10.f),
        gain (1, 0.55f), vol (1, -12.f),
        gain (2, 0.75f), vol (2, -14.f),
        bass (-1), mid (-1), treb (-1), pres (0.f), { "MASTER", -40.f, 6.f, -8.f, " dB", -1 } }, 11,
      { { "CHANNEL",   { "Clean", "Blue", "Red", "" }, 3, 1 },
        { "STRUCTURE", { "Low", "High", "", "" }, 2, 0 },
        { "PRE-EQ",    { "Normal", "B1", "B2", "" }, 3, 0 },
        { "CLASS",     { "A/B", "A", "", "" }, 2, 0 } }, 4,
      0, 3 },

    { "amp_vh4", "DizBenZA", Topology::Vh4,
      "Diezel VH4",
      "Quattro canali davvero separati: ognuno ha guadagno, tre bande e volume "
      "suoi, e non condividono niente. Presenza e profondita' sono le uniche "
      "due cose in comune, e stanno nel finale.",
      { gain (0, 0.2f),  bass (0), mid (0), treb (0), vol (0, -10.f),
        gain (1, 0.45f), bass (1), mid (1), treb (1), vol (1, -12.f),
        gain (2, 0.7f),  bass (2), mid (2, -3.f), treb (2), vol (2, -14.f),
        gain (3, 0.85f), bass (3), mid (3, -5.f), treb (3), vol (3, -14.f),
        pres (0.f), depth (0.f) }, 22,
      { { "CHANNEL", { "Clean", "Crunch", "Mega", "Lead" }, 4, 2 }, none(), none(), none() }, 1,
      0, 4 },

    { "amp_trinity", "MAZZALARGA", Topology::Trinity,
      "Mezzabarba Trinity",
      "Tre canali: il pulito ha la sua torre di tono, gli altri due se ne "
      "dividono una seconda. Nel finale, oltre a presenza e profondita', c'e' "
      "la controreazione: abbassandola il finale si allenta e diventa piu' "
      "morbido sotto le dita.",
      { gain (0, 0.3f), bass (0), mid (0), treb (0), vol (0, -10.f),
        gain (1, 0.5f), gain (2, 0.75f),
        bass (1), mid (1), treb (1),
        vol (1, -12.f), vol (2, -14.f),
        { "MASTER", -40.f, 6.f, -8.f, " dB", -1 }, pres (0.f),
        { "FEEDBACK", 0.f, 1.f, 0.6f, "", -1 }, depth (0.f) }, 16,
      { { "CHANNEL", { "Clean", "Drive", "Overdrive", "" }, 3, 1 }, none(), none(), none() }, 1,
      0, 3 },

    { "amp_revo", "BRUNTELLI", Topology::Revo,
      "Brunetti XL R-Evo",
      "Tre canali, ognuno con guadagno, tre bande, volume e una messa a fuoco "
      "che stringe o allarga la banda prima del guadagno: e' quella a decidere "
      "se il canale resta largo o diventa chirurgico. In coda un volume "
      "generale e la profondita'.",
      { gain (0, 0.3f), bass (0), mid (0), treb (0), vol (0, -10.f), { "FOCUS", 0.f, 1.f, 0.4f, "", 0 },
        gain (1, 0.55f), bass (1), mid (1), treb (1), vol (1, -12.f), { "FOCUS", 0.f, 1.f, 0.5f, "", 1 },
        gain (2, 0.8f), bass (2), mid (2, -3.f), treb (2), vol (2, -14.f), { "FOCUS", 0.f, 1.f, 0.65f, "", 2 },
        { "MASTER", -40.f, 6.f, -8.f, " dB", -1 }, depth (0.f) }, 20,
      { { "CHANNEL", { "Clean", "Boost", "XLead", "" }, 3, 2 }, none(), none(), none() }, 1,
      0, 3 },

    { "amp_mark5", "Mause Mark 5", Topology::Mark5,
      "Mesa/Boogie Mark V",
      "Tre canali con i loro comandi, e soprattutto l'equalizzatore grafico a "
      "cinque cursori fissi — 80, 240, 750 Hz, 2,2 e 6,6 kHz — che si puo' "
      "assegnare o togliere. La curva a V su quei punti e' meta' del suono di "
      "questa famiglia. Il finale si commuta in potenza, e a potenza ridotta "
      "cede prima.",
      { gain (0, 0.3f), treb (0), mid (0), bass (0), { "PRESENCE", -12.f, 12.f, 0.f, " dB", 0 }, vol (0, -10.f),
        gain (1, 0.55f), treb (1), mid (1, -2.f), bass (1), { "PRESENCE", -12.f, 12.f, 0.f, " dB", 1 }, vol (1, -12.f),
        gain (2, 0.8f), treb (2), mid (2, -6.f), bass (2), { "PRESENCE", -12.f, 12.f, 0.f, " dB", 2 }, vol (2, -14.f),
        { "80", -12.f, 12.f, 3.f, " dB", -1 },   { "240", -12.f, 12.f, 1.f, " dB", -1 },
        { "750", -12.f, 12.f, -6.f, " dB", -1 }, { "2.2k", -12.f, 12.f, 2.f, " dB", -1 },
        { "6.6k", -12.f, 12.f, 4.f, " dB", -1 } }, 23,
      { { "CHANNEL", { "Clean", "Crunch", "Extreme", "" }, 3, 2 },
        { "MODE",    { "I", "II", "III", "" }, 3, 1 },
        { "GRAPHIC", { "Off", "On", "", "" }, 2, 1 },
        { "POWER",   { "10 W", "45 W", "90 W", "" }, 3, 2 } }, 4,
      0, 3 },
    };
    return list;
}

inline int count() { return (int) models().size(); }
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

} // namespace ampmodel
