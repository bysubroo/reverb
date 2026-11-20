The pack archive password is **not** stored in this repository. CI reads it from the
GitHub Actions secret `PACKS_KEY` (Settings → Secrets and variables → Actions) and
compiles it into the installers, so packs install without any input.

Local builds: put the key in `installer/packs-key.txt` (ignored by git) or set the
environment variable `PACKS_KEY`.
