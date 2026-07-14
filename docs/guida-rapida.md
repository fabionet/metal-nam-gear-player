# METAL NAM GEAR PLAYER — Guida Rapida

Guida di partenza per suonare in pochi minuti.

## 1. Installazione

- Copia `NAM Custom.vst3` in `~/.vst3/` (Linux). Per LV2, copia `NAM Custom.lv2` in `~/.lv2/`.
- Su Windows/macOS il porting è a parte (repo `nam-custom-win`).
- Il plugin viene visto dagli host come **NAM Custom**; il titolo nella UI è **..::METAL NAM GEAR PLAYER::..**

## 2. Prima sessione

1. Fai partire il DAW a **48 kHz** (i modelli NAM sono generalmente allenati a 48 kHz).
2. Inserisci **NAM Custom** su una traccia con una chitarra DI (segnale pulito direct-in).
3. Clicca sul loader **MODEL** e apri un file `.nam` (V1 o A2). Trovi modelli gratuiti su [Tone3000](https://www.tone3000.com/).
4. Clicca sul loader **IR** e apri una risposta impulsiva `.wav` del cabinet (opzionale ma consigliato per modelli "amp-only").
5. Regola **INPUT** e **OUTPUT** e suona.

## 3. Modalità Output (Steve-style)

Sotto il pomello OUT trovi il toggle **NORMAL** e il bottone **CAL** (accanto al banner preset):

- **Raw** — nessuna correzione, esci al livello del modello.
- **Normalized** *(default)* — se il modello ha metadata `loudness`, il plugin compensa per allineare il volume ai modelli di riferimento (~14 dBu di headroom).
- **Calibrated** — se il modello ha metadata `input_level_dbu` e `output_level_dbu`, corregge sia l'ingresso sia l'uscita al livello dichiarato. Usalo per confrontare modelli come se fossero pedali reali.

Se un modello **non ha metadata reali** la normalizzazione è **no-op** (evita saturazione dell'IR o compensazioni sballate). Il bottone **CAL** apre un popup dove leggi i metadata del modello caricato e imposti il livello di calibrazione input in **dBu**.

## 4. Slim (solo modelli A2)

Sotto il loader NAM c'è lo slider **SLIM** e il pomello **QUAL**: attivi solo con modelli **A2 SlimmableContainer**. Riduce il numero di canali della rete in tempo reale → meno CPU al costo di un po' di fedeltà. Sui modelli V1 lo slider è grigio.

## 5. Catena effetti

Ordine di segnale:

```
IN → Smart Gate → OverDrive → Distortion
   → NAM Model
   → 5-band EQ + Depth + Resonance → Noise Gate → HP → Loudness Normalization
   → IR (con HP/LP + trim + phase invert)
   → Delay → Chorus → Flanger → Reverb → Tremolo
   → OUT
```

I toggle di bypass sono nel footer (checked = **attivo**, unchecked = bypass).

## 6. Preset

- Menu **PRESETS** in alto a destra: preset di fabbrica per categoria (Clean / Rock / Metal / Extreme Metal) + preset utente.
- **Get more presets** → apre il browser Tone3000.
- **Save / Save As** → salva le tue impostazioni come `.nampreset`.

## 7. Controlli extra nell'header

- **CPU N%** — meter di carico (lime <40%, giallo <75%, arancione oltre)
- **OS 2x** — oversampling 2x (aumenta qualità e CPU; latenza dichiarata al DAW)
- **i** — popup informazioni, crediti e licenze

## 8. Troubleshooting

- **Audio a scatti** → assicurati di avere una build **Release** (`-DCMAKE_BUILD_TYPE=Release`) e che il DAW sia a 48 kHz.
- **Modello non carica** → potrebbe essere corrotto o di formato non supportato; controlla la console dell'host. Dalla v0.1.x un `.nam` invalido non fa più crashare l'host.
- **IR troppo lunga o header sospetto** → la lettura è protetta con un cap di 4 s a partire dalla v0.1.x.

Per approfondimenti tecnici (sicurezza, RT-safety, calibrazione dettagliata) vedi la **Guida Tecnica**.
