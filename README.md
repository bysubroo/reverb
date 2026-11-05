# SubRoo Rooms

Konvolucioni reverb za post-produkciju i sound dizajn: biraš lokaciju (stan, kuću, kafanu, hotel, auto, studio...), klikneš gde stoji **zvučnik** (izvor), gde stoji **mikrofon** i koja su **vrata** otvorena. Plugin izračuna kako bi to stvarno zvučalo, uključujući zvuk iz druge sobe, kroz zatvorena vrata i kroz zid.

Radi na **Windowsu, macOS-u i Linuxu** kao **VST3, AU (Mac), CLAP, LV2 (Linux) i Standalone**. Testirani DAW-ovi: FL Studio, Cubase/Nuendo, Logic Pro, Ableton Live, Reaper, Studio One.

> Nezavisan projekat. „Indoor” i „Altiverb” su proizvodi i zaštićeni nazivi kompanije Audio Ease. SubRoo Rooms ne koristi njihov kod, grafiku ni Indoor impulse.

---

## Instalacija

Gotovi buildovi se prave automatski na GitHubu: **Actions → poslednji „Build” → Artifacts**:
`SubRoo-Rooms-Windows`, `SubRoo-Rooms-macOS`, `SubRoo-Rooms-Linux`.

| Sistem | Kako |
|---|---|
| Windows | Raspakuj, desni klik na `install_windows.bat` → **Run as administrator**. VST3 ide u `C:\Program Files\Common Files\VST3`, CLAP u `...\Common Files\CLAP`. |
| macOS | Raspakuj, dupli klik na `install_mac.command` (ako macOS blokira: desni klik → Open). Instalira VST3, AU i CLAP i skida karantin, jer plugin nije notarizovan. |
| Linux | `./install_linux.sh` (u `~/.vst3`, `~/.clap`, `~/.lv2`). |

Posle instalacije uradi rescan pluginova u DAW-u. FL Studio i Reaper učitavaju i **CLAP** verziju.

---

## Kako se koristi

### Prikaz lokacije (desno)
- **Kvadrat** = pozicija zvučnika (izvora). Klik ga pomera tamo.
- **Jaje** = pozicija mikrofona. Strelica pokazuje gde mikrofon gleda.
- **Vrata / prozori** = klik ih otvara i zatvara. Sva vrata su jedan automatizabilan parametar (`Doors`).
- Na hover se vidi isprekidana linija do nove pozicije i promena nivoa u dB. **Žuti trougao** znači da bi se nivo promenio za više od 12 dB.
- Uređaji: **TV**, **FM** (auto/radio), **PA** (plafonsko ozvučenje), **TEL** (telefon na spikeru), **PC** (laptop). Zvuk se pušta „kroz” taj uređaj.
- **ISO / 2D** menja prikaz. **OPEN ALL / CLOSE ALL** otvara ili zatvara sva vrata.

### Kontrole (levo)
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

Svaki push na GitHub pokreće build za Windows, macOS (universal: Apple Silicon + Intel) i Linux, testove i pluginval. Tag `v*` (npr. `v0.9.0`) pravi Release sa zip fajlovima.

### Struktura
```
Source/Acoustics   lokacije (JSON), simulacija prostora, sinteza IR-a, izmereni packovi, render thread
Source/DSP         particionisana matrična konvolucija sa crossfade zamenom IR-a (bez klikova)
Source/Plugin      procesor, parametri, fabrički preseti
Source/UI          izometrijski/2D prikaz, kontrole, izgled
Resources/locations  ugrađene lokacije (JSON)
tools/             merenje IR-ova, snimanje Altiverb IR-ova
tests/             testovi engine-a, hosta i GUI-ja
```

## Licence
- JUCE 9: AGPLv3 ili JUCE licenca. Za interno korišćenje i deljenje sa kolegama uz izvorni kod dovoljna je AGPLv3. Za komercijalnu distribuciju zatvorenog koda treba JUCE licenca (Starter je besplatan do 50k $ prihoda).
- pffft (FFT): BSD-like licenca, `third_party/pffft/LICENSE.txt`.
- clap-juce-extensions: MIT.

© SubRoo — subroo.xyz
