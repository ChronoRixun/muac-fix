# The native fix (dinput8.dll)

`runtime/` builds an x86 `dinput8.dll` for the 2006 PC release. Setup installs it next to `Game.exe`, beside the
source-built SafeDiscLoader2 `version.dll` and dgVoodoo2's `D3D9.dll`. It forwards DirectInput to Windows and does
not modify `Game.exe`. Its settings are in `muac-fix.ini` in the same folder (defaults:
[packaging/muac-fix.ini](../packaging/muac-fix.ini)) and take effect when the game next starts.

Build and test:

```powershell
cmake -S runtime -B build/runtime -A Win32
cmake --build build/runtime --config Release
ctest --test-dir build/runtime -C Release --output-on-failure
```

Never copy test fixture DLLs into a real installation.

## Controller (`[Input] ControllerSupport`)

A Bluetooth Xbox controller is presented with a wired Xbox 360 product ID and a name that contains "Xbox". Both are
needed: the game chooses its button atlas from the device name, so a fix that changed only the product ID still
showed numbered buttons. The owner confirmed Xbox glyphs in game. Every gameplay mapping has not been checked yet.

Only a single connected XInput controller is normalized. DirectInput enumeration order is not a reliable XInput
slot identity, so with several pads the fix refuses to synthesize devices rather than feed one controller's input to
the wrong player. The game's own pad handling stays available when no normalization applies.

Existing controller profiles are kept. A new Xbox alias is tied to the original device name, and a saved profile is
copied only into an absent, associated destination; an unrelated existing alias is refused. On the checked retail
build, an absent player-one selection is initialized for the pad; a saved selection or custom-binding choice is
never overwritten.

## Mouse (`[Input] CaptureMouse`)

`CaptureMouse=0` suppresses the engine's pointer clipping and re-centring through process-local API hooks,
including calls the protected executable resolves dynamically, and uses non-exclusive mouse input. Keep dgVoodoo2's
own `CaptureMouse` off as well: on its own it does not stop the engine's cursor capture. The owner confirmed that
the mouse can leave the window.

## Focus (`[Display] RunInBackground`)

`RunInBackground=1` filters deactivation in windowed and borderless mode, so the game keeps running while another
window has focus. Automatic minimization is suppressed there; the user's own minimize command is respected.

## Online (`[Online]`)

The fix redirects the game's GameSpy DNS lookups to OpenSpy. `Domain=off` disables the redirect. An optional
`Server=<IPv4>` overrides `Domain` for a local or private service. `LocalIP=auto` prefers the default-route adapter
over virtual adapters. Hosting and joining a game have not been tested end to end.

## Display

Windowing is dgVoodoo2's job, configured in `dgVoodoo.conf` (defaults:
[packaging/dgVoodoo.conf](../packaging/dgVoodoo.conf)). A real window needs both `FullScreenMode = false` and
`AppControlledScreenMode = false`. The proof behind that profile: [windowed-display.md](windowed-display.md).

## Reused code

The DirectInput adapter derives from MUA Controller Fix commit `893bef8`, and the DNS/local-IP rules and cursor
behaviour from XML2 Fix commit `a49d76c` (both MIT). MinHook 1.3.4 is vendored as source under its BSD licence.
Notices are in [licenses/](../licenses/). The local API tests do not stand in for verified multi-controller
gameplay.
