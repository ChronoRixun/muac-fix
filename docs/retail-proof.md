# Retail import proof — 2026-11-02

Status: file import proven on Windows 11; playable setup remains blocked at the
protected executable's disc check. This is not a released compatibility fix.

## Source and import

- Original English PC DVD, volume MUADVD, product code
  `{932FB3F3-594D-4600-ABFA-F2DE80A14214}`.
- ISO and mounted-disc folder inspection agree: 17 CABs, 18,831 installation
  files, 6,904,983,982 bytes.
- Import directly from the ISO succeeded without executing InstallShield, MSI
  custom actions or the original installer.
- Subsequent verification hashed all 18,831 imported files: no missing or
  changed files. Existing-install detection also recognizes the result.
- Synthetic ISO/folder, spanned CAB, transactional import and changed/missing
  manifest tests pass. Real extracted-folder import has not been repeated end
  to end; the mounted-disc folder uses the same reader.

## Retail-specific details

The MSI describes the pre-protection Game.exe size (4,681,728 bytes); the disc's
protected Game.exe is 6,066,073 bytes. The importer permits the difference only
for its exact SHA-256 fingerprint. It never replaces or patches Game.exe.

Maps3.cab includes a file continued in Maps31.cab. CAB continuation is resolved
against the MSI's cabinet names rather than arbitrary external paths.

pycdlib rejects this image's UDF tags. Its ISO9660/Joliet namespace is readable
and agrees with Windows' mounted-disc view. The reader explicitly chooses that
namespace with a narrow override against pinned pycdlib 1.22.0.

## Runtime experiment

The original executable showed an Access Denied dialog. A separate, local build
of [SafeDiscShim](https://github.com/RibShark/SafeDiscShim) 0.9.0 was then tested
with an app-local bootstrap and app-local child-process DLL lookup. No system
driver, global compatibility database or system DLL was installed.

The shim initialized, intercepted the SecDrv requests and relaunched the game.
The game then displayed a Cannot locate the DVD-ROM dialog asking for the
correct DVD. Additional diagnostics showed it accessing the mounted DVD's
00000001.TMP and issuing device requests. This rules out simply failing to find
the mounted drive; it does not prove every detail of the protection failure.

The MSI Registry table only creates the Activision parent key and game's user
key; no disc-path value is present in that table. InstallShield scripted actions
were not executed or exhaustively reversed.

The bootstrap, downloaded shim sources, compiled test DLLs, game files, logs and
screenshots remain outside this repository. SafeDiscShim itself is not shipped
or represented as a working MUA solution.

## Next gate

Prove one runtime path: either a compatible executable supplied locally by the
player, or additional compatibility work supporting the protected executable.
Then test graphics initialization, gameplay, saving/loading and controller input.
Only after this should launcher setup mark an imported installation playable.
