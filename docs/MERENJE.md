# Merenje sopstvenih lokacija (Indoor princip)

Cilj: isti rig, isti gain i isti zvučnik u svakoj prostoriji. Tako se svi IR-ovi u packu mogu mešati bez razlika.

## Oprema (minimum → idealno)
- **Zvučnik:** jedan aktivni monitor sa ravnom karakteristikom (npr. Genelec 8030, Yamaha HS5). Uvek isti zvučnik i isti volumen.
- **Mikrofoni:**
  - minimum: stereo AB par (2 omni, razmak 40–60 cm);
  - dobro: quad (4 omni u kvadratu, 50 cm, sa malim baflama, npr. pena između kapsula);
  - idealno: quad + MS + 2 plafonska omnija (9 kanala), kao Indoor.
- **Interfejs** sa dovoljno ulaza; svi gainovi se zapišu i **ne diraju** do kraja snimanja.
- **Stalci, metar, skica stana** (dimenzije soba, pozicije vrata).

## Postupak
1. Napravi sweep: `python tools/measure/subroo_measure.py sweep --out sweep.wav` (48 kHz, 10 s eksponencijalni sweep, plus 4 s tišine).
2. Za svaku kombinaciju **pozicija zvučnika × pozicija mikrofona × stanje vrata** pusti sweep kroz zvučnik i snimi sve kanale rig-a u **jedan** višekanalni WAV. Ime fajla: `s_kauc__m_dnevna__10.wav` (vidi PACK_FORMAT.md).
3. Snimi i 10 s tišine za proveru šuma.
4. Dekonvolucija celog foldera, sa istim gainom za sve:
   `python tools/measure/subroo_measure.py batch --sweep sweep.json --recordings snimci/ --out "Documents/SubRoo Rooms/Packs/Moj stan/ir"`
5. Napravi `location.json` (šablon: `python tools/measure/subroo_measure.py new-pack --id moj_stan --name "Moj stan" --out "Packs/Moj stan"`), unesi sobe, vrata i pozicije.
6. U pluginu: **PACKS → Rescan packs**.

## Saveti
- Zatvoren prozor, isključen frižider i grejanje. Snimaj noću ako je ulica bučna.
- Kroz zatvorena vrata zvuk je i 40–60 dB tiši: sweep pusti glasnije (ali **isti** gain za sve!) ili ga ponovi više puta (`--repeats 4` usrednjava).
- Mikrofon na visini uha (1.6 m) ili kamere. Zvučnik na visini usta (1.6 m), a za sedenje 1.1 m.
- Fotografiši svaku poziciju i zapiši u Excel: id, soba, x, y, z, smer.
