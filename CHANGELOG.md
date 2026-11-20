# Changelog

## 1.0.0

- **Installer za Windows** (`SubRoo-Rooms-1.0.0-Windows-Setup.exe`):
  - instalira VST3, CLAP i standalone app;
  - sam skida, proverava (SHA-256) i raspakuje svih 221 izmerenih lokacija;
  - nova verzija se instalira preko stare, a impulsi se ponovo skidaju samo kad izađe nova verzija paketa;
  - ima uninstaller.
- **Installer za macOS** (`.pkg`): AU, VST3, CLAP, app i impulsi.
- **Novi izgled u stilu hardvera:**
  - brušena grafitna ploča sa rack ušima;
  - bakelitni knobovi sa skalom i amber očitavanjem vrednosti;
  - osvetljeni tasteri;
  - amber displeji za lokaciju i preset;
  - ekran sa okvirom i analogni VU metar.
- Ikonica aplikacije.
- Plugin čita impulse iz zajedničkog foldera koji puni installer (`ProgramData` / `Library/Application Support`) i iz ličnog foldera `Documents/SubRoo Rooms/Packs`.

## 0.9.0

- Prva verzija: lokacije sa izvorom, mikrofonom i vratima.
- Izometrijski i 2D prikaz.
- Direct On/Off.
- Tri mikrofonska sistema.
- Formati od mono do 7.1.2.
- VST3, AU, CLAP, LV2 i Standalone.
- Izmereni Altiverb impulsi kao packovi.
