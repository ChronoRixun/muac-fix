# Windowed startup proof

> **Dated record.** Since 0.2.0, setup installs this profile: the launcher downloads the official dgVoodoo2
> archive, and setup verifies it and extracts the x86 `D3D9.dll` with [packaging/dgVoodoo.conf](../packaging/dgVoodoo.conf).
> dgVoodoo2 is still not redistributed by this project. See [packaging.md](packaging.md).

Tested on Windows 11 with the original retail executable and the source-built
SafeDiscLoader2 described in [retail-proof.md](retail-proof.md). The game's video menu sets
resolution, but does not provide fullscreen/windowed/borderless selection.

Runtime: official [dgVoodoo 2.87.6](https://github.com/dege-diosg/dgVoodoo2/releases/tag/2.87.6),
MS/x86/D3D9.dll placed next to Game.exe, with a local dgVoodoo.conf. No system
graphics DLL is replaced.

- Official archive SHA-256: `9afa493396feec7108f42a2d97661eef96bbaac5addf0dfb5cfbc3001cd76a0a`
- x86 D3D9.dll SHA-256: `060cb161eb7a0c608a520185f0b334db325ad71d69016bc26060c9d5b9651f7d`

Changes to the release's default configuration:

```ini
[General]
OutputAPI = d3d11_fl11_0
FullScreenMode = false
CaptureMouse = false
CenterAppWindow = true

[GeneralExt]
FPSLimit = 60

[DirectX]
AppControlledScreenMode = false
```

Both screen-mode settings are necessary. FullScreenMode=false alone is ignored
when DirectX AppControlledScreenMode=true. Leave WindowedAttributes empty for
a normal titled window. The test retained the default disabled Alt+Enter toggle
and diagnostic watermark. The user selected 1920x1080 in the game's own settings.

Verification: visible title bar with minimize/maximize/close controls; active
desktop mode remains 2560x1440 as read through EnumDisplaySettingsW; the game
process responds and startup video advances. Win32_VideoController WMI values
were stale during the experiment and are not reliable display-mode evidence.

An older wrapper crashed during a display-mode change. Restore a disrupted
desktop only while the test game is stopped; do not change modes underneath a
running legacy fullscreen renderer. Gameplay, save/load, alt-tab stability and
other GPUs remain unverified. Neither graphics wrapper is bundled by this repo
yet; redistribution/packaging and launcher integration remain separate work.
