# Metal NAM Gear Players — Neo Edition — Guida Rapida

*Versione 0.2.0*

Guida di partenza per suonare in pochi minuti.

## 1. Installazione

**Linux, via pacchetto** — il modo consigliato, mette i tre formati dove gli host li cercano:

```bash
sudo dpkg -i metal-nam-gear-players-neo_0.2.0_amd64.deb
```

**Linux, a mano** — copia `Metal NAM Gear Players Neo.vst3` in `~/.vst3/` e `Metal NAM Gear Players Neo.lv2` in `~/.lv2/`.

**Windows** — usa l'installer `MetalNAMGearPlayersNeo-windows-x64-setup.exe`, che ti fa scegliere fra standalone, VST3 e LV2. In alternativa scompatta il pacchetto portabile dove preferisci.

Gli host vedono il plugin come **Metal NAM Gear Players Neo**; il titolo nell'interfaccia è **..:: Metal NAM Gear Players :: ^Neo Edition^ ::..**

> **Convive con l'edizione Full.** Nomi, codice VST3 e URI LV2 sono diversi, quindi puoi tenere installate entrambe e caricarle insieme nella stessa sessione.

## 2. Prima sessione

1. Fai partire il DAW a **48 kHz** (i modelli NAM sono generalmente allenati a 48 kHz).
2. Inserisci **Metal NAM Gear Players Neo** su una traccia con una chitarra DI (segnale pulito direct-in).
3. Clicca sul loader **MODEL** e apri un file `.nam` (V1 o A2). Trovi modelli gratuiti su [Tone3000](https://www.tone3000.com/).
4. Clicca sul loader **IR 1** e apri una risposta impulsiva `.wav` del cabinet (opzionale ma consigliato per modelli "amp-only").
   Se il modello contiene già la cassa, il primo loader si spegne da solo: vedi §5.
5. Regola **INPUT** e **OUTPUT** e suona.

## 3. Modalità Output (Steve-style)

Nella colonna **AMP**, sotto il knob `OUTPUT`, trovi il toggle **NORMAL / RAW / CALIBRATED**. Il bottone **CAL** (per aprire il popup di calibrazione) è nell'header in alto a destra, insieme a PRESETS, i, OS 2x, CPU e Zoom.

- **Raw** — nessuna correzione, esci al livello del modello.
- **Normalized** *(default)* — se il modello ha metadata `loudness`, il plugin compensa per allineare il volume ai modelli di riferimento (~14 dBu di headroom).
- **Calibrated** — se il modello ha metadata `input_level_dbu` e `output_level_dbu`, corregge sia l'ingresso sia l'uscita al livello dichiarato. Usalo per confrontare modelli come se fossero pedali reali.

Se un modello **non ha metadata reali** la normalizzazione è **no-op** (evita saturazione dell'IR o compensazioni sballate). Il bottone **CAL** apre un popup dove leggi i metadata del modello caricato e imposti il livello di calibrazione input in **dBu**.

## 4. Slim (solo modelli con sottomodelli)

Lo slider **SLIM** sotto il loader NAM e il pomello **QUAL** nella colonna **CAB** sono legati allo stesso parametro e sono attivi **solo sui modelli che portano davvero più sottomodelli** (SlimmableContainer o WaveNet slimmabile, versione 0.7.0). Scelgono quale sottomodello usare: **0 è il più leggero**, 1 il più completo. Meno CPU al costo di un po' di fedeltà.

Sui modelli V1 — e su un A2 che non sia slimmabile — entrambi i controlli sono **spenti e attenuati**, perché non c'è nulla da scegliere. Il suggerimento a comparsa te lo dice.

> **Attenzione:** non tutti gli A2 sono slimmabili. Conta cosa c'è dentro il file, non la sigla.

## 4-bis. Due IR e bilanciamento

Sotto al primo loader ce n'è un **secondo**, con volume e meter propri. Il pomello **IR BAL** nella sezione **IR TOOLS** incrocia le due risposte: tutto a sinistra senti solo IR 1, al centro una miscela paritaria, tutto a destra solo IR 2.

Il secondo si accende da solo passando a **Dual-Mono** o **Stereo**, dove serve davvero. In **Mono** lo accendi tu col tasto **ON** accanto all'etichetta IR 2. Tornando in Mono non viene spento: resta una tua scelta.

**Esclusione automatica.** Se carichi un modello che contiene già la cassa — i metadati dicono `gear_type` `amp_cab` oppure `full-rig` — il **primo** loader va in bypass vero e si spegne graficamente, così non senti due casse in fila. Il secondo resta disponibile per stereo e dual-mono.

## 4-ter. Il menu dei pedali

Sotto al titolo delle sezioni **NGATE**, **GATE**, **COMP**, **OVERDRIVE**, **DISTORTION** ed **EQ** c'è una tendina con l'elenco dei pedali emulabili. L'elenco è **lo stesso in tutte**, raggruppato per categoria: Overdrive, Distortion, High Gain, Fuzz, Booster, Gate / Noise, Equalizer, Compressor.

Anche le cinque sezioni della scheda **FX** hanno la loro tendina, con un **elenco a parte**: lì hanno senso ritardi, modulazioni e riverberi, non distorsori. Le categorie sono Delay, Chorus, Flanger, Reverb, Tremolo, e ognuna si apre con l'effetto che quella sezione ha sempre avuto — scegliendolo si torna esattamente al suono di prima.

Scegliendo un pedale la sezione si **ripopola**: restano solo i pomelli e gli interruttori che quel pedale ha davvero, con i nomi, le unità e i valori di fabbrica suoi. Un limitatore mostra soglia, rapporto, rilascio e livello; un sustainer mostra sustain, attacco, tono e livello; un grafico a sette bande mostra sette cursori più il livello. La larghezza del pannello si adatta da sola.

**Anche il nome della sezione segue il pedale**: titolo e tasto di attivazione diventano EQUALIZER, COMPRESSOR, HIGH GAIN e così via. La posizione nella catena non cambia, e si legge dall'ordine dei pannelli.

Sull'acceso/spento valgono due regole, e non si contraddicono:

- scegli un pedale **mai usato finora**: la sezione tiene lo stato che aveva. Se era spenta resta spenta.
- richiami un pedale **già presente o già usato** in un'altra sezione: arriva com'era — stessi valori dei pomelli, stesso interruttore, stessa accensione. È lo stesso effetto spostato o duplicato, non un altro che comincia da zero.

**Lo stesso effetto non si può mettere in due sezioni.** Se lo scegli dove ce n'è già uno uguale, la scelta viene rifiutata e compare un avviso che dice dove sta. Per spostarlo, prima cambia pedale alla sezione che lo ospita, poi richiamalo dove vuoi: ci arriverà com'era.

La sola **torre di tono dell'amplificatore** non si duplica: è una e sta nella sezione EQ. Sceglierla altrove lascia la sezione senza comandi, e te lo dice.

Con l'equalizzatore viaggia il suo **analizzatore di spettro**, con il compressore il suo **misuratore di riduzione**: compaiono nella sezione dove li hai messi e spariscono da quella che hai svuotato.

Niente vieta di mettere un distorsore nella sezione dell'equalizzatore o un equalizzatore in quella dell'overdrive: l'elenco è unico e ogni sezione è un posto nella catena, non un vincolo sul tipo di pedale.

Le sezioni che gestiscono **IR**, **AMP** e **MASTER** non hanno la tendina: non sono posti da pedale.

## 4-quater. Analizzatore dell'equalizzatore

Dove metti un equalizzatore, fra i comandi e il tasto di attivazione compare un analizzatore di spettro con sopra la sua curva. Il segnale è prelevato **subito dopo quella sezione**, quindi vedi l'effetto della curva anche se hai spostato l'equalizzatore all'inizio della catena. Sostituendo l'equalizzatore con un altro pedale l'analizzatore sparisce insieme a lui.

Ogni banda ha una maniglia colorata: trascinala in verticale per il guadagno, e compare la lettura del valore. Quali maniglie vedi dipende dall'equalizzatore scelto:

- **AMP Tone Stack** — la torre di tono dell'amplificatore, quella di sempre: cinque maniglie, e la sola **MID** ha anche la frequenza sull'asse orizzontale e il **Q sulla rotellina** del mouse.
- **GE-SEVEN Graphic** — sette maniglie a frequenza fissa (100, 200, 400, 800 Hz, 1.6, 3.2, 6.4 kHz). Trascinandone una si muove anche il cursore corrispondente della sezione, e viceversa: sono lo stesso comando visto da due parti.
- **EQ-TWENTY Parametric** — due maniglie spazzolabili in frequenza.

> Con il guadagno MID a zero il Q non cambia nulla, né nel disegno né nel suono: un filtro a campana senza guadagno è un passa-tutto. Alza il MID di qualche dB e vedrai la campana stringersi.

## 5. Catena effetti

Ordine di segnale:

```
IN → NGATE → GATE → Compressore → OverDrive → Distortion
   → NAM Model (con volume e tone stack propri)
   → Amp Sim (GEAR SX oppure MARCHELLOW, opzionale)
   → EQ (torre di tono nativa oppure pedale) + Depth + Resonance → HP → Loudness Normalization
   → IR 1 <-> IR 2  (bilanciamento, poi HP/LP + trim + phase invert)
   → Delay → Chorus → Flanger → Reverb → Tremolo
   → Widener → Metronomo → OUT
```

Sei di questi stadi — NGATE, GATE, COMP, OVERDRIVE, DISTORTION, EQ — sono **posti** occupati dal pedale scelto nella tendina della sezione, non effetti fissi. Il compressore si può spostare nella catena con il selettore sotto alla sua tendina (Front, Post-Gate, Post-IR).

I toggle di bypass sono nel footer (checked = **attivo**, unchecked = bypass).

## 6. Preset

Il pannello **PRESETS** entra da destra con uno scorrimento. Dentro trovi i preset di fabbrica per categoria (Clean / Rock / Metal / Extreme Metal), un **Default** neutro non sovrascrivibile, e i tuoi.

- **Doppio clic** richiama un preset. Il **clic singolo** lo seleziona soltanto, per esportarlo o eliminarlo senza cambiare il suono sotto le dita.
- **Save / Save As** salvano in `.nampreset`. Il nome *Default* è riservato: modificando i parametri sei obbligato a salvare sotto un altro nome, così il punto di partenza resta pulito.
- **Esporta .prs** scrive il preset corrente come file singolo, da passare a qualcun altro.
- **Backup lista .prstl** salva l'intera libreria in un file solo, **con dentro i file `.nam` e le IR** che i preset usano: si ripristina anche su un'altra macchina.
- **Importa preset...** legge `.prs`, `.prstl` e `.nampreset`. Altre estensioni vengono rifiutate senza nemmeno aprirle. I nomi già presenti vengono resi univoci con un suffisso, quindi un'importazione non sovrascrive mai il tuo lavoro.

### Il display LCD e i banchi

Nell'intestazione c'è un display a matrice di punti che mostra il preset corrente e il banco. Se il nome è lungo scorre da solo; riducendo lo zoom il display scende su una riga propria sotto le schede.

- **A B C D** — quattro banchi per tenere varianti dello stesso preset
- **SAVE** — salva la variante nel banco scelto, sovrascrivendo se esiste
- **TAP** — batti il tempo, tre tocchi bastano. Lampeggia a tempo anche a metronomo spento.
- **METRO** — metronomo con suono da campanaccio, accento sul primo movimento, metriche 4/4 3/4 2/4 6/8 5/4 7/8 e **volume indipendente** dall'uscita

## 7. Controlli extra nell'header

Dall'alto a destra della UI:

- **CAL** — apre il popup di calibrazione (vedi §3)
- **PRESETS** — menu preset (vedi §6)
- **i** — popup informazioni, crediti e licenze
- **OS 2x** — oversampling 2x (aumenta qualità e CPU; latenza dichiarata al DAW)
- **CPU N%** — meter di carico (lime <40%, giallo <75%, arancione oltre)
- **Zoom** — scala la finestra del plugin (utile su schermi HiDPI)

## 8. Troubleshooting

- **Audio a scatti** → assicurati di avere una build **Release** (`-DCMAKE_BUILD_TYPE=Release`) e che il DAW sia a 48 kHz.
- **Modello non carica** → dalla v0.2.0 l'interfaccia **te lo dice**, con una riga rossa sotto il meter del lettore: *MODELLO NON TROVATO* se il file non c'è più, *MODELLO ILLEGGIBILE* se c'è ma non si apre. Prima falliva in silenzio e sembrava un difetto del plugin. Vale anche per gli IR.
- **La lista modelli è vuota** → la cartella ricordata non esiste più. Capita coi dischi esterni, che il sistema può rimontare su un percorso diverso. Usa **Browse** per ripuntarla: la scansione è ricorsiva, quindi puoi puntare la radice di un pack e vedrai anche i file nelle sottocartelle.
- **SLIM sempre spento** → il modello caricato non ha sottomodelli. È corretto, vedi §4.
- **IR troppo lunga o header sospetto** → la lettura è protetta con un cap di 4 s a partire dalla v0.1.x.

Per approfondimenti tecnici (sicurezza, RT-safety, calibrazione dettagliata) vedi la **Guida Tecnica**.
