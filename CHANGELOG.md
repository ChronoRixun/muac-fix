# Changelog

## 0.2.3

### Fixed
- **The game no longer crashes at startup with a resolution it doesn't know** (Ultimate Legends issue #31). The
  2006 game keeps only 14 resolutions (640x480 to 1920x1200, plus 2048x1536) and read past the end of that list when
  any other size was saved, so 2560x1440, 2560x1080, 3840x2160 and even 1600x900 crashed a few seconds after start.
  The native fix (`dinput8.dll`, now **0.1.2**) adds the saved size to the game's list when the primary monitor
  reports it and otherwise uses the saved size directly, after checking the game's code bytes. `Game.exe` on disk
  is unchanged; another executable is left alone.

### Changed
- The manifest still requires Ultimate Legends **0.5.0-beta.8 or newer**, so 0.5.0 can install this release through
  **Update**. Ultimate Legends **0.5.1** is needed to pick a resolution the original game
  doesn't know (2560x1440, 3840x2160): it offers them once it sees runtime 0.1.2 and tells dgVoodoo2 about the chosen size. Older
  launchers keep offering every monitor mode; with an older runtime (MUA Classic Fix 0.2.2 or before) a size such as
  2560x1440 still crashes.
- Setup helper, SafeDiscLoader2, dgVoodoo2 and the Proton runtime set are unchanged from 0.2.2.

### Known limits
- Tested on Windows 11 with dgVoodoo2 at 2560x1440, 2560x1080, 1920x1080, 1600x900 and 3840x2160 (a 4K monitor),
  to the main menu and, at 3840x2160, into the first level. A size dgVoodoo2 can't offer on the monitor it uses
  (3840x2160 on a 1440p monitor, say) still fails with "Display failed!"; Ultimate Legends only offers the primary
  monitor's own modes.
- Under Proton the guard is installed too, but display mode stays Proton's: the launcher doesn't write a resolution
  there.

## 0.2.2

### Added
- **Steam Deck / Linux (Proton) support for setup and play.** With Ultimate Legends 0.5.0-beta.8 or newer,
  automatic setup from your own disc works under Proton, and the game starts there. Put the game folder on a
  Linux filesystem (the Deck's internal storage, an ext4 SD card or your Linux home), not an NTFS or network drive.
- Under Wine/Proton, graphics use Proton's own D3D9 (DXVK) instead of dgVoodoo2, whose `D3D9.dll` crashes under
  Wine. Setup neither downloads the dgVoodoo2 archive nor places its DLL there, and `_muac/runtime.json` records
  `graphics: wine-d3d9`. Display mode is left to Proton / Steam Deck.
- Linux mount points that Wine shows as linked folders (`/home`, an SD card) are accepted, alongside Wine drive
  roots (`Z:\`). Redirected junctions, symbolic links and linked entries inside the disc or game tree are still
  refused.

### Changed
- **A Windows-made installation moved to Steam Deck / Linux: choose Update** on the game's page in Ultimate
  Legends. Under Proton, Update keeps a backup of the installation's `D3D9.dll` at
  `_muac/backups/D3D9-<sha256>.dll`, records the removal in `_muac/runtime.json` and removes the live DLL so
  Proton's D3D9 is used. `Game.exe`, the other game files, saves, mods and your settings stay as they are; a
  failed or cancelled Update puts the original files back. This also works when the installation is already
  on 0.2.2 (same-version Update). On Windows, Update keeps `D3D9.dll` unchanged.
- The manifest (`muac-setup.json`) requires Ultimate Legends **0.5.0-beta.8 or newer** (final 0.5.0 also
  qualifies). That launcher writes its update journal and runtime record crash-safely, so a launcher stopped
  mid-update recovers on the next Update instead of getting stuck.
- `dgVoodoo.conf` is still installed under Proton: the native fix reads its `FullScreenMode` /
  `AppControlledScreenMode` values to enable Run in background.
- Native Windows is unchanged: setup still verifies and places the official dgVoodoo2 2.87.6 `D3D9.dll`, and the
  native fix DLL's source is the same as in 0.2.1.

### Known limits
- **Tested under GE-Proton 11 on Linux, not yet on a real Steam Deck:** setup from the ISO into an ext4 folder,
  Update of a Windows-made 0.2.1 installation, and starting the game to its main menu. Please report how it goes
  on a Deck.
- Under Proton, the dgVoodoo2 display options (mode, resolution, frame limit, VSync, watermark) don't apply and
  the launcher hides them. Controller, Run in background, mouse and online options stay available.
- Gameplay past the main menu, controllers through Steam Input, saves and online play are not certified under
  Proton yet (on Windows, the known limits in the README still apply).

## 0.2.1

### Fixed
- **A cancelled setup can always be cleaned up.** The helper now stages its work in a `.muac-stage-*` folder inside
  the chosen game folder instead of beside it, so Ultimate Legends 0.5.0-beta.6 can remove exactly that folder when
  the helper's own cleanup takes longer than the launcher's cancel grace (slow USB or network drives).
- Wine's own drive roots (`Z:\`) are no longer treated as linked folders; links and junctions inside the chosen
  folders, and network (UNC) roots, are still refused. ISO sources get the same check as disc folders.

### Known limits
- **Proton / Steam Deck is not supported yet.** Wine also shows Linux mount points (`/home`, an SD card) as linked
  folders, so setup there still stops with "Linked folders are not supported for setup.", and the 2006 game itself
  does not start under Proton yet.

### Changed
- The manifest (`muac-setup.json`) requires Ultimate Legends **0.5.0-beta.6 or newer**.

## 0.2.0

First release: setup from an owned MUA 2006 PC retail disc (ISO, mounted DVD or disc folder) into a new, verified
installation; Xbox controller support with the game's own button art; windowed play with a free mouse and
background running; OpenSpy redirect; the pinned, source-built SafeDiscLoader2 and the verified official dgVoodoo2.
Needs Ultimate Legends 0.5.0-beta.5 or newer.
