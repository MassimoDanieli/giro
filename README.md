# Giro

App nativa (C++ / JUCE) per giri di accordi play-along: griglia stile lead sheet, basso (walking nello swing, con note di approccio), comping e batteria, **import MIDI con riconoscimento degli accordi**, e **accordo corrente evidenziato in rosso** durante la riproduzione.

- Su macOS i suoni sono quelli del synth General MIDI di Apple (DLSMusicDevice): contrabbasso, piano, batteria. Su Linux/Windows un synth interno.
- **Import da iReal Pro**: incolla un link `irealb://` (un brano, più brani o una playlist) oppure apri/trascina l'export HTML della playlist: ripetizioni, finali 1./2., `x`, `r`, coda e segno vengono espansi in una griglia lineare; stile e tempo stimati dallo stile iReal.
- Stili: swing, bossa, straight 8, ballad, shuffle blues. Metri da 2/4 a 7/4.
- Tempo (anche dal vivo), trasposizione (riscrive la griglia), loop su un intervallo di battute, count-in, numero di giri, mix dei tre strumenti.
- Libreria salvata in `~/Library/Application Support/Giro/library.json` (6 giri d'esempio al primo avvio).

## Compilare su Mac

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release      # scarica JUCE 8.0.4 da GitHub la prima volta
cmake --build build --target Giro -j8
open build/Giro_artefacts/Release/Giro.app     # oppure trascinala in /Applications
```

Con un JUCE già sul disco: `cmake -B build -DGIRO_JUCE_DIR=/percorso/JUCE`.
Per lavorarci in Xcode: `cmake -B xcode -G Xcode && open xcode/Giro.xcodeproj`.

Il workflow `.github/workflows/build.yml` compila su macOS e Linux, esegue i test e pubblica `Giro-mac.zip` come artifact di ogni push.

## Test

```bash
cmake --build build --target giro_tests giro_engine_test && (cd build && ctest --output-on-failure)
```

`giro_ireal_tests` confronta l'import iReal con i vettori del progetto pyRealParser; `giro_tests` copre parsing degli accordi, formato della griglia, trasposizione (es. Blues in F → C), parser SMF e riconoscimento accordi; `giro_engine_test` fa un render offline del motore audio (timing, count-in, cambio tempo, stop).

## Struttura

```
core/      C++17 puro, nessuna dipendenza
  theory    simboli di accordo, spelling, trasposizione
  chart     formato testo della griglia (| ... |, _, %, NC, [A]) e appiattimento in battiti
  midi      parser Standard MIDI File + riconoscimento accordi (template matching, stima tonalità)
  ireal     import link iReal Pro (irealb:// e irealbook://), de-offuscamento, espansione di ripetizioni e finali
  patterns  dal giro alla lista di eventi MIDI per stile (basso, comping, batteria)
app/       JUCE
  Engine    scheduler sample-accurate + synth (AU DLSMusicDevice su Mac, SimpleSynth altrove)
  Chart     disegno della griglia, evidenziazione rossa, loop
  Main      finestra, libreria, trasporto, editor
tests/     test del core e del motore
```

## Formato della griglia

```
[A]
| F7 | Bb7 | F7 | Cm7 F7 |
| Bb7 | Bdim7 | F7 | Am7b5 D7 |
| Gm7 | C7 | F7 D7 | Gm7 C7 |
```

Battute separate da `|`; più accordi nella stessa battuta divisi in parti uguali; `_` tiene l'accordo un battito in più (`A7 _ _ Eb7` = 3+1); `%` ripete la battuta precedente; `NC` nessun accordo; `[A]` apre una sezione.

## Release firmate e notarizzate

Il workflow `.github/workflows/release.yml` parte a ogni tag `v*` (es. `git tag v0.1.0 && git push --tags`): compila universal (arm64 + x86_64), firma con il Developer ID, notarizza con Apple, applica lo staple e pubblica `Giro-<versione>.dmg` nella Release di GitHub. Richiede questi secret nel repo (Settings → Secrets and variables → Actions):

| Secret | Contenuto |
|---|---|
| `MAC_CERT_P12` | certificato **Developer ID Application** esportato da Portachiavi come `.p12`, in base64 (`base64 -i cert.p12 \| pbcopy`) |
| `MAC_CERT_PASSWORD` | password scelta all'esportazione del `.p12` |
| `ASC_KEY_ID` | Key ID della API key di App Store Connect (Users and Access → Integrations → App Store Connect API) |
| `ASC_ISSUER_ID` | Issuer ID della stessa pagina |
| `ASC_KEY_P8` | contenuto del file `AuthKey_XXXX.p8` |
