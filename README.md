# SubRoo Rooms

Konvolucioni reverb za post-produkciju i sound dizajn: biraš lokaciju (stan, kuću, kafanu, hotel, auto, studio...), klikneš gde stoji **zvučnik** (izvor), gde stoji **mikrofon** i koja su **vrata** otvorena. Plugin izračuna kako bi to stvarno zvučalo, uključujući zvuk iz druge sobe, kroz zatvorena vrata i kroz zid.

Radi na **Windowsu, macOS-u i Linuxu** kao **VST3, AU (Mac), CLAP, LV2 (Linux) i Standalone**. Testirani DAW-ovi: FL Studio, Cubase/Nuendo, Logic Pro, Ableton Live, Reaper, Studio One.

> Nezavisan projekat. „Indoor” i „Altiverb” su proizvodi i zaštićeni nazivi kompanije Audio Ease. SubRoo Rooms ne koristi njihov kod, grafiku ni Indoor impulse.

---

## Instalacija

Svaka verzija je na **Releases** strani repoa (`v1.0.0`, `v1.1.0`...). Stare verzije ostaju dostupne.

| Sistem | Kako |
|---|---|
| Windows | Skini `SubRoo-Rooms-<verzija>-Windows-Setup.exe` i pokreni. Installer stavlja VST3 u `C:\Program Files\Common Files\VST3`, CLAP u `...\Common Files\CLAP`, standalone app u Start meni i sam skida, proverava (SHA-256) i raspakuje sve izmerene lokacije u `C:\ProgramData\SubRoo Rooms\Packs`. Nova verzija se instalira preko stare; packovi se ponovo skidaju samo kad izađe nova verzija packova. Uninstall: Settings → Apps → SubRoo Rooms. |
| macOS | Skini `SubRoo-Rooms-<verzija>-macOS.pkg`, desni klik → **Open** (installer još nije notarizovan). Instalira AU, VST3, CLAP, app i packove (`/Library/Application Support/SubRoo Rooms/Packs`). |
| Linux | Zip: `./install_linux.sh` (u `~/.vst3`, `~/.clap`, `~/.lv2`). Packove ručno u `~/Documents/SubRoo Rooms/Packs`. |

Posle instalacije uradi rescan pluginova u DAW-u. FL Studio i Reaper učitavaju i **CLAP** verziju.

Windows može da prikaže „Windows protected your PC” (SmartScreen) jer installer nije digitalno potpisan: **More info → Run anyway**.

Sopstveni packovi i dalje mogu u `Documents/SubRoo Rooms/Packs`; plugin čita oba foldera.

---

## Kako se koristi

### Prikaz lokacije (desno)
- **Kvadrat** = pozicija zvučnika (izvora). Klik ga pomera tamo.
- **Jaje** = pozicija mikrofona. Strelica pokazuje gde mikrofon gleda.
- **Vrata / prozori** = klik ih otvara i zatvara. Sva vrata su jedan automatizabilan parametar (`Doors`).
- Na hover se vidi isprekidana linija do nove pozicije i promena nivoa u dB. **Žuti trougao** znači da bi se nivo promenio za više od 12 dB.
- Uređaji: **TV**, **FM** (auto/radio), **PA** (plafonsko ozvučenje), **TEL** (telefon na spikeru), **PC** (laptop). Zvuk se pušta „kroz” taj uređaj.
- **ISO / 2D** menja prikaz. **OPEN ALL / CLOSE ALL** otvara ili zatvara sva vrata.

### Kontrole (levo i dole)
| Kontrola | Šta radi |
|---|---|
| **Direct On** | Za insert na traci. Izlaz je kompletan zvuk u prostoru (direktan zvuk + refleksije + rep), sa realnim nivoom i pravcem. Ulaz se sabira u mono izvor. |
| **Direct Off** | Za aux/send. Bez direktnog zvuka, samo refleksije i rep. Svaki kanal ulaza je izvor na svojoj pan poziciji (radi N→N, npr. 5.1→5.1). |
| **Mic system** | **Spaced AB** (razmaknuti omni sa baflama: širi, obavijajući), **Coincident MS** (koincidentni: oštrija lokalizacija), **Hybrid** (uglovi AB, centar i bočni MS, kao 7.0 DTS sistemi). |
| **Angle: Recorded / Custom** | Recorded: mikrofon gleda kako je postavljen u lokaciji. Custom: krug određuje odakle dolazi direktan zvuk (0° = napred, L/R). Rotira se ceo prostor, ne mikrofon; automatizacija je bez klikova. |
| **Level Range** | Natural: realne razlike u glasnoći (zatvorena vrata = tiše). Flat: sve pozicije iste glasnoće. |
| **Spread** (Direct Off) | Koliko reverb prati panovan izvor (uzak) ili ga obavija (širok). |
| **Mix / Pre-delay** (Direct Off) | Wet/dry i kašnjenje reverba. |
| **Size / Decay** | Veličina lokacije (70–140 %) i dužina repa (50–200 %). |
| **Low / High** | Shelving EQ na reverbu (250 Hz / 4 kHz). |

**Formati kanala:** mono, stereo, LCR, quad, 5.0, 5.1, 7.0, 7.1, 7.0.2, 7.1.2. Ulaz može biti mono, stereo ili isti format kao izlaz. LFE kanal ne dobija reverb.

**Latencija:** 256 semplova (DAW je automatski kompenzuje).

### Ugrađene lokacije
Stan u zgradi (sa stepeništem), porodična kuća (dvorište, garaža), kafana (sala, bina, šank, toalet, ulica), hotel (soba, kupatilo, hodnik, lift, komšije iza zida), automobil (kabina, gepek, trotoar, ulica), studio (live room, režija, kabina), kancelarija (open space, staklena sala) i auto-radionica.

Ugrađene lokacije su **simulirane**: geometrijska akustika (izvori-slike za rane refleksije), statistički rep po oktavama, prolaz zvuka kroz vrata i zidove, virtuelni mic rig. **Izmerene lokacije** (packovi) se dodaju u folder `Documents/SubRoo Rooms/Packs`. Vidi [docs/PACK_FORMAT.md](docs/PACK_FORMAT.md) i [docs/MERENJE.md](docs/MERENJE.md).

**Altiverb POST packovi** (289 IR-ova iz Altiverb 7 XL, snimljeni kroz sam Altiverb uz dozvolu Audio Ease-a) se ne nalaze u ovom repou. Dele se kao poseban zip samo sa ljudima koje dozvola pokriva: raspakuj ga u `Documents/SubRoo Rooms/Packs`. Lokacije se pojavljuju u kategorijama „Altiverb · …“. Kako su snimljeni: [tools/altiverb-capture/README.md](tools/altiverb-capture/README.md).

---

## Build iz koda

Potrebno: CMake 3.22+, C++20 kompajler (Visual Studio 2022, Xcode 15+, GCC 11+/Clang 15+). JUCE i CLAP ekstenzije se preuzimaju automatski.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Rezultati su u `build/SubRooRooms_artefacts/Release/`. Testovi: `-DSUBROO_BUILD_TESTS=ON`, pa pokreni `SubRooRoomsTests`.

Svaki push na GitHub pokreće build za Windows, macOS (universal: Apple Silicon + Intel) i Linux, testove i pluginval. Windows build pravi i installer i testira ga (tiha instalacija, update preko postojeće verzije, uninstall); macOS pravi i testira `.pkg`. Nova verzija: podigni verziju u `CMakeLists.txt` (`project(... VERSION X.Y.Z)`) i dodaj sekciju u `CHANGELOG.md`; push na `main` pravi tag `vX.Y.Z` i Release sa installerima (može i ručni tag `v*`).

Installer: `installer/windows/SubRooRooms.iss` (Inno Setup 6.5+), `installer/macos/build_pkg.sh`. Podaci o packovima (URL, SHA-256, verzija) su u `installer/windows/packs.iss`; šifrovana arhiva je u releases repoa `bysubroo/subroo-rooms-content`, a lozinka arhive je GitHub Actions secret `PACKS_KEY` (nikad u gitu). Grafika installera i ikonica: `installer/branding/make_branding.py`.

### Struktura
```
Source/Acoustics   lokacije (JSON), simulacija prostora, sinteza IR-a, izmereni packovi, render thread
Source/DSP         particionisana matrična konvolucija sa crossfade zamenom IR-a (bez klikova)
Source/Plugin      procesor, parametri, fabrički preseti
Source/UI          izometrijski/2D prikaz, kontrole, hardware izgled (faceplate, knobovi, VU)
installer/         Windows (Inno Setup) i macOS (.pkg) installer, grafika
Resources/locations  ugrađene lokacije (JSON)
tools/             merenje IR-ova, snimanje Altiverb IR-ova
tests/             testovi engine-a, hosta i GUI-ja
```

## Licence
- JUCE 9: AGPLv3 ili JUCE licenca. Za interno korišćenje i deljenje sa kolegama uz izvorni kod dovoljna je AGPLv3. Za komercijalnu distribuciju zatvorenog koda treba JUCE licenca (Starter je besplatan do 50k $ prihoda).
- pffft (FFT): BSD-like licenca, `third_party/pffft/LICENSE.txt`.
- clap-juce-extensions: MIT.
- Barlow Condensed font: SIL Open Font License, `Resources/fonts/OFL.txt`.
- 7-Zip (dekoder u macOS installeru, `7zz`): GNU LGPL.

© SubRoo — subroo.xyz
