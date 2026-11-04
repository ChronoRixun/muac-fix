# Changelog

## 0.2.1

### Fixed
- **Setup works under Proton / Wine (Steam Deck).** Wine maps every Linux folder behind a drive letter (`Z:`) that
  is itself a link, so 0.2.0 refused every source and destination with "Linked folders are not supported for
  setup." Only Wine's own drive roots are now accepted; links and junctions inside the chosen folders, and network
  (UNC) roots, are still refused. ISO sources get the same check as disc folders.
- **A cancelled setup can always be cleaned up.** The helper now stages its work in a `.muac-stage-*` folder inside
  the chosen game folder instead of beside it, so Ultimate Legends 0.5.0-beta.6 can remove exactly that folder when
  the helper's own cleanup takes longer than the launcher's cancel grace (slow USB or network drives).

### Changed
- The manifest (`muac-setup.json`) requires Ultimate Legends **0.5.0-beta.6 or newer**.

## 0.2.0

First release: setup from an owned MUA 2006 PC retail disc (ISO, mounted DVD or disc folder) into a new, verified
installation; Xbox controller support with the game's own button art; windowed play with a free mouse and
background running; OpenSpy redirect; the pinned, source-built SafeDiscLoader2 and the verified official dgVoodoo2.
Needs Ultimate Legends 0.5.0-beta.5 or newer.
