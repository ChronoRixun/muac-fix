# MUA Classic Fix documentation

Start with the [project README](../README.md): what the fix does, what you need and how to set it up.

## Current

- [packaging.md](packaging.md): what the release package contains, how it is built from pinned sources, and the
  offline helper's `setup` contract.
- [runtime.md](runtime.md): the native `dinput8.dll`: controller, mouse, focus and online settings, and their limits.

## Proof records

Dated records of what was tested and how. Their "next steps" have since shipped in 0.2.0.

- [retail-proof.md](retail-proof.md): importing the retail disc and starting the protected executable (2026-11-02).
- [windowed-display.md](windowed-display.md): the dgVoodoo2 profile that gives a real window without changing the
  desktop resolution.

## Plans (historical)

Kept for the record. They describe the work as planned before 0.2.0, not the current state.

- [plans/implementation-plan.md](plans/implementation-plan.md): the original local-setup proof plan.
- [plans/muac-options-plan.md](plans/muac-options-plan.md): the runtime and launcher options plan, with its
  verification ledger.
