# Changelog

## 0.2.2 (unreleased)

### Added
- Wine/Proton setup uses the platform's D3D9 implementation. It does not require or place dgVoodoo's
  `D3D9.dll`, which crashes under Wine; `_muac/runtime.json` records `graphics: wine-d3d9`.
- Linux mount points exposed by Wine as directory reparse points resolving to themselves are accepted,
  alongside Wine drive roots. Redirected junctions, symlink tags and linked disc-tree entries stay refused.

### Changed
- Requires Ultimate Legends **0.5.0-beta.8 or newer**, including final 0.5.0. The matching launcher implements
  Wine cleanup/update guards and backs up/removes a Windows installation's D3D9.dll during Update.
- Native Windows keeps the verified dgVoodoo setup path. The native fix DLL is unchanged.

### Validation limits
- Prior Proton investigation reached the main menu without dgVoodoo. This release's full disc setup,
  Windows-to-Proton Update and gameplay still need coordinator validation on a native Linux filesystem.

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
