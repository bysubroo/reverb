# Snimanje Altiverb IR-ova (REAPER, Windows)

Altiverb 7 drži IR-ove u enkriptovanim `.irbulk` arhivama i ne čitamo ih direktno.
IR-ove snimamo **kroz sam Altiverb**: učitamo IR njegovim parametrom za automatizaciju
(`IR Autom`), propustimo jedan impuls (100 % wet, sve neutralno) i snimimo izlaz.
Konvolucija je linearna, pa je izlaz tačno impulsni odziv. Ništa se ne dešifruje.

Snimljeni IR-ovi **ne idu u git**. Dele se kao poseban zip, samo sa ljudima koje
pokriva dozvola Audio Ease-a.

## Kako Altiverb bira IR

- Stanje plugina (`vst_chunk`) je čist XML. U njemu piše putanja učitanog IR-a
  (`ir_ae/POST/domestic/Bedroom 1/s-s/info.iri2`) i njegov `IR HashFloat`.
- Parametar 47 `IR Autom` = `HashFloat / 2^31`. Postavljanje tačne vrednosti učitava taj IR
  (samo tačan pogodak, nema „najbližeg“).
- Putanja bez heša ne učitava ništa, zato heševe skupljamo jednom, klikom kroz browser.

## Koraci

1. **REAPER** (bilo koja verzija 7), novi projekat. U ovaj folder stavi skripte.
2. `1_discover.bat` (ili Actions → Load ReaScript → `SubRoo_Altiverb_1_Discover.lua`):
   napravi traku `ALTIVERB CAPTURE` sa Altiverbom i zapiše `altiverb_params.txt`.
3. `2_catalog.bat` (`SubRoo_Altiverb_2_Catalog.lua`) radi u pozadini. Otvori Altiverbov
   IR browser i klikni redom sve IR-ove koje želiš (npr. ceo POST). Svaki učitan IR se
   upiše u `altiverb_ir_catalog.txt`. Kad završiš, napravi prazan fajl `STOP_WATCHER2`.
   Savet: u list prikazu klikći redove odozdo nagore – proširenje ispod reda tada ne
   pomera redove iznad.
4. Lista za snimanje:
   `python make_capture_list.py altiverb_ir_catalog.txt capture_list.csv manifest.csv`
   (`s-s` IR-ovi se snimaju dvaput – impuls na levom i na desnom ulazu; `m-s`, `m-q`, `m-m`
   jednom, impulsom 0,5 + 0,5 jer Altiverb sabira L+R za mono-in IR).
5. `3_capture.bat` (`SubRoo_Altiverb_3_Capture.lua`): projekat prebaci na 48 kHz, traku na
   stereo, postavi neutralna podešavanja (direct/ER/tail uključeni, 100 % wet, bez EQ,
   decay, modulacije i reverse), pa za svaki red učita IR, sačeka da ga Altiverb potvrdi
   (heš u stanju) i renderuje impuls (action 41993). WAV-ovi idu u `AltiverbPOST\`.
   Već snimljeni fajlovi se preskaču, pa se posao može prekinuti (`STOP_CAPTURE`) i nastaviti.
   ~5 s po renderu; ceo POST (289 IR-ova, 381 render) traje oko pola sata.
6. Packovi za plugin:
   `python ../measure/altiverb_packs.py --captures AltiverbPOST --manifest manifest.csv --out "%USERPROFILE%\Documents\SubRoo Rooms\Packs"`

## Napomene

- Altiverb na izlazu obrće polaritet (direct = −1); `altiverb_packs.py` ga vraća.
- Direct je Altiverbov kalibrisani direktni zvuk (0 dB), pa je odnos direct/reverb isti kao
  u Altiverbu. Nivoi se ne normalizuju po fajlu – ceo pack ima jedan gain.
- VST2 Altiverb u REAPER-u daje stereo izlaz i za quad (`m-q`) IR-ove.
- „Next door“ kuće (`House 1`, `Coll's place`, `Next Door III - Coll`) dobijaju bitove za
  vrata i prozor, pa dugmad za vrata u pluginu biraju odgovarajući Altiverb snimak.
