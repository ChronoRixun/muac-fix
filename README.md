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

Licensed under GPL-3.0-only. The content guard is adapted from Ultimate Legends
(ChronoRixun/ultimate-legends), also GPL-3.0. This fan project is not affiliated
with Marvel, Activision, Raven Software or Beenox.
