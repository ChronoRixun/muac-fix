# MUA Classic Fix

Installation and compatibility work for Marvel: Ultimate Alliance (2006 PC),
intended for Ultimate Legends. Work in progress; a successful file import does
not yet establish that the game runs.

The setup helper accepts a player's ISO or an extracted DVD folder, including
a mounted DVD. It reads the MSI database and CAB archives without executing
the original installer. Existing game folders are detected separately.

This repository contains our tools and tests, never game files or replacement
game executables. The protected retail executable was started with a source-built
SafeDiscLoader2 version.dll; windowed rendering was checked with dgVoodoo 2.87.6.
Those dependencies are not downloaded or installed by the importer yet.

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
repair, uninstall and the launcher's disc-import wizard are not yet implemented.
An installed folder is inspected without being modified.

## Native input and online preview

`runtime/` builds an x86 `dinput8.dll` for MUA 2006. It adds a single-controller
XInput adapter, optional mouse capture, and a GameSpy-to-OpenSpy DNS redirect.
It forwards DirectInput to Windows and does not modify Game.exe. It is separate
from the SafeDiscLoader2 and dgVoodoo DLLs used in the retail startup experiment.

```powershell
cmake -S runtime -B build/runtime -A Win32
cmake --build build/runtime --config Release
ctest --test-dir build/runtime -C Release --output-on-failure
```

Copy the resulting `dinput8.dll` next to Game.exe. Optional `muac-fix.ini`:

```ini
[Input]
ControllerSupport=1
CaptureMouse=0

[Display]
RunInBackground=1

[Online]
Domain=openspy.net
LocalIP=auto
```

The Bluetooth Xbox controller is presented with a wired Xbox 360 product ID and
an Xbox-containing name. Both are needed: the 2006 game chooses its button atlas
from the name, so a PID-only fix still showed numbered buttons. The owner confirmed
Xbox glyphs and mouse escape in-game. Full gameplay mappings remain unverified.
This preview only
normalizes a single connected XInput controller: DirectInput enumeration order
is not a reliable XInput slot identity, so it refuses ambiguous multipad synthesis
instead of feeding another controller's input into the wrong player. Existing
native pad handling remains available when no normalization is applied.

CaptureMouse=0 suppresses pointer clipping/recentering through process-local API
hooks, including calls resolved dynamically by the protected executable, and uses
nonexclusive mouse input. Keep dgVoodoo's CaptureMouse setting off as well.
RunInBackground=1 filters deactivation in windowed/borderless mode. Automatic
minimization is suppressed there, while the user's minimize command is respected.
Changes take effect after restarting the game.

Existing controller profiles are retained. A new Xbox alias is tied to the
original name, and a saved profile is copied only into an absent, associated
destination. An unrelated existing alias is refused. On the checked retail build,
an absent player-one selection is initialized for the pad; a saved selection or
custom-binding choice is never overwritten.

Online Domain=off disables the redirect. An optional Server=IPv4 overrides Domain
for a local/private service; LocalIP=auto prefers the default-route adapter over
virtual adapters. Host/join connectivity was not tested; the user requested the
redirect without requiring end-to-end multiplayer certification.

The Ultimate Legends MUAC options implementation lives on its `codex/muac-setup`
branch. It edits the native fix, graphics wrapper and game's resolution settings,
and currently accepts an already prepared installation. See the [windowed profile](docs/windowed-display.md).

## Reused code

The DirectInput adapter derives from MUA Controller Fix commit `893bef8`, and the
DNS/local-IP rules and cursor behavior from XML2 Fix commit `a49d76c` (both MIT).
MinHook 1.3.4 is vendored as source under its BSD license. Notices are in `licenses/`.
Do not confuse the local API tests with verified multi-controller gameplay.

Licensed under GPL-3.0-only. The content guard is adapted from Ultimate Legends
(ChronoRixun/ultimate-legends), also GPL-3.0. This fan project is not affiliated
with Marvel, Activision, Raven Software or Beenox.
