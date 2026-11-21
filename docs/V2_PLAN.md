# SubRoo Rooms 2 – plan

Cilj: plugin koji radi kao Indoor (izometrijski 3D presek lokacije, klik na mesto
zvučnika / mikrofona, vrata koja se otvaraju), a ceo zvuk dolazi iz Altiverb 7 XL
impulsa (snimljenih kroz Altiverb, uz dozvolu Audio Ease-a).

Odluke (Marko, 21. 11. 2026):

- Altiverb više nije „pack". Sve lokacije su Altiverb prostori. Ugrađene sintetičke
  lokacije (stan, kuća, kafana...) se ne prikazuju.
- Pozicije koje Altiverb nema izvode se iz najbližeg snimka (ogledalo levo/desno, nivo,
  kašnjenje, filter). U GUI-ju su obeležene drugačije od pravih.
- Svih 221 lokacija dobija unikatan, detaljan 3D model sa teksturama.
- Size i Decay počinju od originalnog snimka (100 %) i samo povećavaju.
- Izgled kao Indoor (bez hardware ploče). Grafika i modeli su naši, ništa se ne kopira.
- Reference za modelovanje: fotografije iz Altiverba za svaki impuls (privatno, nikad u
  pluginu ni u gitu).

## Faze

1. **Engine** (urađeno na grani `subroo-rooms`)
   - izvedene pozicije: `sources[].derive = { from, mirror, gainDb, delayMs, lowpassHz }`
   - Size / Decay na snimljenim impulsima (`reshapeMeasured`): Size rasteže odziv,
     Decay produžava rep i menja šum snimka sintetičkim repom iste boje
   - konvolver drži do 10 s odziva
2. **Podaci**: definicije lokacija (geometrija, pozicije, 3D model) idu u plugin, a
   impulsi ostaju u šifrovanoj arhivi i spajaju se po `id`. Novi model ne traži novu
   arhivu impulsa.
3. **3D renderer** (OpenGL): mreže, teksture (proceduralne, naše), svetlo, senke,
   rotacija i zum kamere, izometrijski presek, izbor pozicija klikom, animirana vrata,
   3D mikrofon i 3D zvučnik.
4. **Biblioteka delova**: zidovi/podovi/plafoni sa materijalima, vrata, prozori, nameštaj,
   sedišta, kontrolna tabla, klupe, stubovi, stepenice...
5. **Modeli**: kategorija po kategorija (auta → stanovi/kupatila → hodnici/stepeništa →
   javni prostori → avioni/vozovi/brodovi → škole/kancelarije → spolja/pod zemljom),
   svaka kategorija = jedno izdanje.
6. **GUI u stilu Indoor-a** po screenshotovima koje pošalje Marko.
