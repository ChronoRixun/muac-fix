# MUA 2006 local setup proof

> **Historical plan**, kept for the record. It predates the 0.2.0 release; for the current state see the
> [README](../../README.md) and the [docs index](../README.md).

Goal: turn the player's ISO or extracted disc folder into a verifiable local
installation, prove a compatible executable and then connect Ultimate Legends.
The user approved a local proof first and a public muac-fix repository. Launcher
changes start at v0.5.0-beta.3 on codex/muac-setup.

Architecture: a Python setup helper reads ISO files with pycdlib, queries MSI
metadata using Windows Installer's read-only API and extracts CAB members with
Windows SetupAPI. The helper emits JSON events usable by the existing launcher
builder process. No original setup program or custom action runs.

Inputs: ISO, extracted disc folder, mounted disc; recognize an existing installed
folder as a distinct source type. Validate product identity and required files,
never just extensions. Reject unsafe paths and source/destination overlap.

Tasks:
- [x] Tests for path confinement, source classification and MSI layout mapping.
- [x] Implement source readers, read-only MSI queries, safe CAB extraction.
- [x] Import to a new folder; record hashes and verify missing/modified files.
- [x] Inspect executable protection; report unresolved readiness.
- [x] Test the real ISO import; compare ISO and mounted-folder inventories.
- [ ] Prove launch, gameplay and saving with a compatible local executable.
- [ ] Integrate the proven flow in the launcher and test against beta 3.
- [ ] Scan repository contents, create the authorized public repo and publish code.

Constraints: never publish game files, change the ISO, install secdrv.sys, alter
other game installs or present an untested executable as working. Never overwrite
an arbitrary destination. Dependency or protection blockers remain explicit.

Verification: unit tests using synthetic MSI/CAB/ISO fixtures; real import using
the supplied disc outside the repository; complete file hash verification; manual
runtime checks. Launcher UI/build validation follows only once runtime is proven.

Current gate: source-built SafeDiscLoader2 starts the unchanged retail executable;
dgVoodoo 2.87.6 with both screen-mode overrides disabled gives a real 1080p window
without changing desktop resolution. See [retail-proof.md](../retail-proof.md) and
[windowed-display.md](../windowed-display.md).
Gameplay/save-load validation and runtime packaging are required before launcher
integration; no Play-ready state or finished runtime fix is being claimed.
