# Self-contained MUAC setup package

`tools/package_setup.py` produces `muac-setup-VERSION-win64.zip`, `muac-setup.json` and
`SHA256SUMS.txt`. The zip contains the frozen offline helper (no Python installation needed),
MUA Classic Fix's x86 dinput8.dll, pinned source-built SafeDiscLoader2 version.dll, default
configuration, license notices and corresponding source archives. It contains no game files.

On native Windows, the original dgVoodoo 2.87.6 archive is downloaded by the launcher separately, specifically
for the selected MUA 2006 installation. Both archive and x86 DLL hashes are pinned in
`packaging/dependencies.json`. The helper extracts that one DLL only after verification.
Wine/Proton skips the archive and DLL and uses the platform's D3D9 implementation instead.
The unused default `dgVoodoo.conf` remains in the common bundle. Runtime records identify the backend
as `dgvoodoo` or `wine-d3d9` and list only the runtime files actually placed.
Upstream terms: https://dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/

Build on Windows with Python 3.14 and Visual Studio C++ tools:

1. Install dependencies in an isolated Python environment with
   `pip install --require-hashes --only-binary=:all: -r packaging/requirements.lock`.
2. Build/test `runtime/` for Win32 Release.
3. Check out the SafeDiscLoader2 source revision in `packaging/dependencies.json`.
4. Build its `version-proxy.vcxproj` with Release/Win32 and
   `/p:ForceImportBeforeCppTargets=<absolute-path-to>/packaging/static-runtime.props`.
   This statically links its C runtime; do not substitute an unverified downloaded DLL.
5. Run `python tools/package_setup.py --version VERSION --safedisc-source SOURCE`
   `--safedisc-dll SOURCE/Release/version.dll --runtime build/runtime/bin/Release/dinput8.dll`.

The manual **Package setup preview** workflow performs these steps and uploads artifacts only.
It does not publish or tag. Review the artifact before releasing it, including its manifest,
source archives, licenses, PE architecture and absence of game content. The launcher requires
a published manifest asset on the selected update channel to perform automatic downloads.
The 0.2.3 manifest, like 0.2.2, requires Ultimate Legends **0.5.0-beta.8 or newer** (final 0.5.0 also qualifies):
an older launcher can install its runtime, and Ultimate Legends 0.5.1 also offers every resolution once it sees
runtime 0.1.2.
Earlier launchers lack the matching Wine mount
checks, graphics settings and migration transaction. In-place Update is implemented by the launcher,
not the helper: on Wine it retains a hash-named backup of an inherited `D3D9.dll`, records the removal,
then removes the live DLL transactionally. Existing settings and game files are preserved.
Only Git-tracked files enter our source archive. The lock targets Windows x64/Python 3.14;
`requirements.txt` documents the direct dependencies, while `requirements.lock` pins all
release-build dependencies and wheel hashes.

All compiled-in MIT/BSD notices are copied both into the setup package's `licenses/` and
the installed game's `_muac/licenses/`, including both MinHook copies and tiny-json.
Cancelled or failed managed setups remove only their own newly created staging folder;
plain `import` retains diagnostic staging. Copy and verification loops check cancellation
between bounded chunks. A forced kill or cleanup blocked by another process can still leave
files behind; the reported staging path identifies them without deleting unrelated folders.

Offline helper invocation: `muac-setup.exe setup --source DISC --out NEW_FOLDER --dgvoodoo ORIGINAL_ZIP`.
`runtime/` beside the executable supplies the verified bundle. JSONL events include `ev` and
the original `event` key. A `cancel` line on stdin requests cooperative cancellation. Successful
setup returns `prepared: true`, `files_complete: true`, and the output path; this is preparation
evidence, not a claim that every gameplay or multiplayer feature has been tested.

The full SafeDiscLoader2 source, our source, exact pycdlib LGPL source archive and rebuild
instructions are included with the package. End users can rebuild with a modified pycdlib;
there is no required signing key. Versioned source and build configuration remain available
alongside binary delivery. Keep all upstream license notices when redistributing.

## Managed staging and Wine paths

Managed `setup` creates its `.muac-stage-*` as a direct child of the chosen output
folder and reports it in the `extract` stage event. A `cleanup` stage event is
flushed before handled failure/cancellation deletion starts. The launcher can
finish deleting that exact contained stage after forcibly ending an unresponsive
helper. Plain `import` retains its sibling diagnostic-stage behavior.

After verification and runtime preparation, managed setup refuses unrelated files
added to the output container. It briefly detaches the verified stage beside the
output using a same-volume rename, removes the now-empty container, then renames
the stage into the final output. There is no cancellation point in this short
promotion sequence. Abrupt termination during these renames can still require
manual recovery of a sibling stage; the launcher never deletes sibling paths.

Wine is detected by `ntdll!wine_get_version`. Reparse-point exceptions apply to
absolute DOS drive roots (such as `Z:\`) and directory mount-point tags
(`0xA0000003`) whose final path equals their own path, as Wine represents Linux
mount points. Native Windows receives no exemption. Redirected junctions and
symbolic links remain refused, including inside the source/game tree. Source-tree
entries are still scanned without following links. The matching launcher applies
the same classifier to forced-kill cleanup and native runtime Update. Both repos
carry the same synthetic mount-point test vectors.
