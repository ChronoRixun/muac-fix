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

**Resolution guard (runtime 0.1.2).** The retail `Game.exe` keeps only 14 sizes from the primary monitor's mode
list (640x480 to 1920x1200, plus 2048x1536), then looks the saved `Settings\Display\Resolution` up in that list
with no end check, so any other saved size (2560x1440, 3840x2160, even 1600x900) crashed at startup
(`Game.exe+0x34cb9f`). After checking the bytes of both sites, the runtime hooks the filter so the saved size joins
the list when the primary monitor reports it, and makes the lookup return the saved size when it is still not
listed. Any other executable is left alone and the log says "resolution guard not installed". dgVoodoo2 must also
enumerate the size: Ultimate Legends 0.5.1 writes it to `[DirectXExt] ExtraEnumeratedResolutions`.

**DPI awareness (runtime 0.1.3).** `Game.exe` has no DPI manifest, so on a monitor with display scaling Windows
runs it DPI-unaware and multiplies its window coordinates. dgVoodoo2 sizes the window from the output's real pixels,
so with a 3840x2160 primary at 150% the Borderless window became 3840 *logical* = 5760 real pixels wide and a
Windowed 2560x1440 became 3840x2160 plus frame. `dinput8.dll` is a static import of `libIGDisplay.dll`, so its
`DllMain` runs before the game's `WinMain`; there the runtime calls
`SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` (only when `libIGDisplay.dll` is loaded, never under Wine).
A process already made aware, for example by the compatibility setting "Override high DPI scaling behavior", is
left as it is. `[Display] DpiAware=0` in `muac-fix.ini` turns it off.

**Borderless (runtime 0.1.3).** dgVoodoo2's `WindowedAttributes = borderless` is undone by the engine, which sets
`WS_OVERLAPPEDWINDOW` and a client edge itself and moves the window. When that attribute is set (windowed mode
only), the runtime's game-window subclass removes the frame styles on `WM_STYLECHANGING` and keeps the window on
the primary monitor's rectangle on `WM_WINDOWPOSCHANGING`. The game and dgVoodoo2 render on adapter 0, the primary.
The engine later installs its own window procedure, which swallows those two messages and moves the window to
(0,0), so the final placement is also checked on `WM_WINDOWPOSCHANGED` and corrected once (at most 64 times per
run). A Windowed window larger than the primary monitor (3840x2160 on a 4K screen) is centred on it horizontally
with the title bar at the top instead of spilling onto the next monitor.

Windows itself adds `HIGHDPIAWARE` under `HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers`
for `Game.exe` the first time the DPI-unaware game enters exclusive Fullscreen (DXGI writes it while dgVoodoo2 sets
up the device). From then on that path ran DPI aware, which is why Windowed looked right on some PCs and not on
others. The runtime does not read or write that flag.

## Reused code

The DirectInput adapter derives from MUA Controller Fix commit `893bef8`, and the DNS/local-IP rules and cursor
behaviour from XML2 Fix commit `a49d76c` (both MIT). MinHook 1.3.4 is vendored as source under its BSD licence.
Notices are in [licenses/](../licenses/). The local API tests do not stand in for verified multi-controller
gameplay.
