# MUAC runtime and launcher options

The user requested watermark removal, Xbox Bluetooth controller support, a
switch to release/capture the mouse, and the MUAC options in Ultimate Legends.
Work stays on codex/retail-setup and launcher codex/muac-setup (beta 3 base).

## Implementation

1. Build an x86 dinput8.dll in runtime/ from the MIT-licensed MUA Controller Fix's
   API-only input adapter. Omit all 2016-specific patches and Steam/Discord code.
   Preserve attribution. Add muac-fix.ini ControllerSupport/CaptureMouse settings.
2. Hook the engine's cursor clip/warp/capture imports and mouse DirectInput
   cooperative mode. With capture off, the game may not confine/recenter the
   pointer; with capture on, confinement remains limited to the foreground game.
3. Turn the local dgVoodoo watermark off. Keep its own CaptureMouse flag in sync
   with the engine option. Display changes take effect on restart.
4. Add a MUAC backend/settings panel in Ultimate Legends: windowed/borderless/
   fullscreen, resolution, frame limit, VSync, controller support, capture mouse
   and optional diagnostic watermark. Validate all values; preserve other config
   keys and reject writes during game/file operations. No unsupported Discord or
   multiplayer controls.
5. Enable existing-install setup only when the required runtime components are
   present; retain the separate disc importer for the later managed setup flow.
   Do not pretend unpublished downloads or unimplemented import UI work.
6. Verify x86 exports/controller enumeration and mouse behavior, test settings
   validation/readback and UI with the existing launcher test harness, build a
   separate test launcher from beta 3, review the final changes and install the
   runtime into the isolated test game. Leave the installed stable launcher intact.

## Verification ledger

- Initial inspection: retail libIGDisplay imports DirectInput8Create from
  DINPUT8.dll and calls ClipCursor, SetCursorPos and SetCapture itself.
- dgVoodoo CaptureMouse=false alone does not prevent the engine's cursor capture.
- The currently proven window profile needs FullScreenMode=false AND
  AppControlledScreenMode=false. Resolution is the game's HKCU Display value.
- Native controller mappings require actual in-game testing; successful API
  forwarding alone is not evidence that every action is bound correctly.
- Implemented x86 DLL, cursor hooks, controller toggle, and OpenSpy redirect with
  default-route adapter selection. Physical Bluetooth pad enumeration verified.
- Review found that DirectInput order cannot safely identify multiple XInput
  slots. This preview normalizes only one connected XInput pad; multi-pad mapping
  remains a required follow-up rather than silently routing the wrong pad.
- Launcher panel and native backend implemented; all CEF integration checks pass.
  133 existing launcher native tests, two UI save-recovery regressions and the
  new runtime tests pass. No multiplayer certification was requested.
- Follow-up: owner found the first IAT-only cursor guard incomplete. A dynamic
  GetProcAddress fixture reproduced the failure; process-local API hooks now
  catch it, and the owner confirmed free mouse movement.
- REA analysis found name-based Xbox glyph selection. Consistent Xbox ID/name
  presentation selects the game's own atlas; owner confirmed Xbox glyphs.
- Runtime 0.1.1 adds windowed focus policy and RunInBackground. Hidden-window
  tests cover both background choices, subclass chaining and explicit minimize.
- Per-original-name aliases carry an association marker; synthetic registry tests
  cover custom bindings, unrelated destinations, fresh/repeated setup and leaving
  existing player selection untouched. No game assets or analysis dumps are shipped.
