# MUA Classic Fix

Installation and compatibility work for Marvel: Ultimate Alliance (2006 PC),
intended for Ultimate Legends. Work in progress; a successful file import does
not yet establish that the game runs.

The setup helper accepts a player's ISO or an extracted DVD folder, including
a mounted DVD. It reads the MSI database and CAB archives without executing
the original installer. Existing game folders are detected separately.

This repository contains our tools and tests, never game files or replacement
game executables. The protected retail executable requires a separate compatible
runtime solution. No protection driver is installed and no executable is downloaded.

Development requires Windows and Python 3.11+. Install with `pip install -e .`.
Run tests with `python -m unittest discover -s tests -v`.

```powershell
muac-setup info --source "D:\MyDisc.iso"
muac-setup info --source "D:\ExtractedDisc"
muac-setup import --source "D:\MyDisc.iso" --out "D:\Games\MUA-2006"
muac-setup verify --out "D:\Games\MUA-2006"
```

Commands emit one JSON object per line. `info` classifies disc versus installed
folder and identifies the known protected retail executable. `import` requires a
new or empty destination and writes in a separate staging folder, promoting it
only after extraction and SHA-256 manifest generation succeed. `verify` detects
missing or changed imported files. Its manifest records the imported bytes; it
is not an independent authenticity certificate. Runtime readiness is separate
from file completeness and is currently always unverified.

The supported disc product code is `{932FB3F3-594D-4600-ABFA-F2DE80A14214}`.
ISO reading uses the ISO9660/Joliet namespace; UDF-only images are not supported.
The retail mastering has a stale MSI size for its protected executable; only the
exact known executable fingerprint receives that size correction. Other editions
and modified disc layouts may need additional profiles.

Interrupted imports leave their staging folder for inspection. Automatic resume,
repair, uninstall, launcher packaging and controller/display hooks are not yet
implemented. An installed folder is inspected without being modified.

Licensed under GPL-3.0-only. The content guard is adapted from Ultimate Legends
(ChronoRixun/ultimate-legends), also GPL-3.0. This fan project is not affiliated
with Marvel, Activision, Raven Software or Beenox.
