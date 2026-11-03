#pragma once

// API-only adapter derived from MUA Controller Fix (MIT), compiled here for
// MUA 2006's 32-bit DirectInput 8 path. See licenses/mua-controller-fix-MIT.txt.
//
// The fix presents every XInput-capable pad the game doesn't already recognise as a
// wired Xbox 360 controller (045E:028E), and builds its DirectInput state from XInput in
// that controller's input layout. In-game bindings must be verified separately.

namespace gamepad_fix
{
	// Hooks a freshly created IDirectInput8 (A or W) object and the devices it creates.
	void hook_direct_input(void* direct_input);
}
