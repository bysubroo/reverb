# Snimanje Altiverb IR-ova (REAPER)

Altiverb 7 IR fajlovi su enkriptovani i ne čitaju se direktno. Zato IR-ove snimamo kroz sam Altiverb: kroz plugin se pusti jedan impuls (100 % wet, sve kontrole neutralne) i snimi se izlaz. Konvolucija je linearna, pa je izlaz tačno impulsni odziv. Ništa se ne dešifruje.

1. U REAPER-u napravi novi projekat (48 kHz).
2. `Actions → Show action list → New action → Load ReaScript…` → `SubRoo_Altiverb_1_Discover.lua` → Run.
   Skripta napravi traku `ALTIVERB CAPTURE` sa Altiverbom (quad izlaz) i u `%APPDATA%\REAPER\SubRoo\` zapiše listu parametara i IR-ova.
3. Na osnovu liste napravi `capture_list.csv` (koje IR-ove snimamo) i `neutral.csv` (neutralna podešavanja).
4. Pokreni `SubRoo_Altiverb_2_Capture.lua`. WAV-ovi idu u `Documents\SubRoo Rooms\Altiverb Captures\`.
5. Napravi packove: `python tools/measure/subroo_measure.py from-captures --captures "<Altiverb Captures>" --out "<Documents>/SubRoo Rooms/Packs"`.

Snimljeni IR-ovi **ne idu u git**. Dele se kao poseban zip, samo sa ljudima koje pokriva dozvola Audio Ease-a.
