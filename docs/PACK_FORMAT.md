# Location pack format

Location pack je folder u `Documents/SubRoo Rooms/Packs/<ime-packa>/`:

```
Moj stan (izmeren)/
  location.json        geometrija, vrata, pozicije zvučnika i mikrofona, rig
  ir/
    s_kauc__m_dnevna__10.wav
    s_kauc__m_dnevna__00.wav
    ...
```

`location.json` koristi isti format kao ugrađene lokacije (`Resources/locations/*.json`) i dodaje blok `pack`:

```json
{
  "format": "subroo-location", "version": 1,
  "id": "moj_stan", "name": "Moj stan (izmeren)", "category": "Packs",
  "rooms":   [ { "id": "dnevna", "name": "Dnevna", "rect": [0,0,5,4], "height": 2.7, "floor": "parquet", "walls": "plaster" } ],
  "portals": [ { "id": "d1", "name": "Vrata dnevne", "type": "door", "rooms": ["dnevna","hodnik"], "at": [5,2], "width": 0.9, "tl": "interior" } ],
  "sources": [ { "id": "s_kauc", "name": "Kauč", "pos": [1.5, 3.2, 1.1] } ],
  "mics":    [ { "id": "m_dnevna", "name": "Kamera", "pos": [3.5, 1.5, 1.6], "yaw": 135 } ],
  "pack": {
    "rig": [ { "name": "L", "az": 45 }, { "name": "R", "az": -45 }, { "name": "Ls", "az": 135 }, { "name": "Rs", "az": -135 } ],
    "calibrationDb": 0,
    "irFolder": "ir"
  }
}
```

## Pravila
- **Koordinate** su u metrima: x = istok, y = sever, z = visina. `yaw` je u stepenima, 0 = +x (istok), 90 = +y (sever), suprotno od kazaljke na satu.
- **Sobe** su pravougaonici `[x0, y0, x1, y1]`. Susedne sobe dele ivicu, a zidovi između njih se nalaze automatski.
- **Vrata** (`type: door | window | opening`) stoje na zajedničkoj ivici dve sobe. Svaka vrata koja mogu da se otvore dobijaju jedan bit stanja, redom kako su navedena u fajlu (najviše 12).
- **Ime IR fajla:** `<sourceId>__<micId>__<doorKey>.wav`, gde je `doorKey` po jedan znak za svaka vrata koja se otvaraju (`1` otvorena, `0` zatvorena), redom iz `portals`. Ako lokacija nema takva vrata, `doorKey` je `-`.
- Ako za neko stanje vrata nema snimka, plugin uzima **najbliže snimljeno stanje**. U statusu piše „nearest door state”.
- Ako za kombinaciju zvučnik/mikrofon nema nijednog snimka, plugin koristi **simulaciju iste geometrije** (zato geometrija treba da bude približno tačna).
- **Kanali** u WAV-u idu redom kao `pack.rig`. `az` je azimut kapsule u stepenima (pozitivno = levo), a `el` je elevacija (plafonski kanali > 30°). Plugin rotira i rasporedi rig na bilo koji izlazni format, od mono do 7.1.2.
- **Nivo:** svi IR-ovi u packu moraju biti snimljeni i obrađeni sa **istim gainom** (bez normalizacije po fajlu). Samo tako „Natural” level range radi tačno. `calibrationDb` pomera ceo pack.
- Sample rate WAV-a može biti bilo koji. Plugin resampluje na rate sesije.

Materijali (`floor`, `walls`, `ceiling`) i propusnost vrata/zidova (`tl`): vidi `Source/Acoustics/Materials.cpp`. Može se zadati i niz od 7 vrednosti (oktave 125 Hz – 8 kHz).
