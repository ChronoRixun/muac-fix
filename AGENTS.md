# Agent guidance for MUA Classic Fix

This is a source-only compatibility and setup project for legally owned MUA 2006.
The player supplies the disc image, extracted disc folder or existing installation.

## Content boundaries

- Commit our source, synthetic tests and original documentation only. Never commit
  game files, game executables (original or modified), decoded assets, live memory
  snapshots, decompiler listings, protection drivers or download links to game files.
- Keep local game installations and research artifacts outside this repository.
  Do not include personal paths, device identifiers, IP addresses, tokens or logs.
- Preserve licenses and attribution for reused code. Third-party runtime packaging
  and public distribution require the owner's explicit release decision; a working
  local experiment does not establish redistribution permission.
- Do not merge, tag, release or change packaging policy merely because a review
  passed. Follow the owner's authorization for the current task.

## Implementation and validation

- Preserve saves, mods, controller profiles and unrelated settings. Import into a
  new staging folder and promote only after verification. Never execute disc MSI
  custom actions. Treat archive paths and metadata as untrusted.
- Keep game-specific changes scoped to the supported executable. Input detection,
  glyphs, gameplay mappings and online host/join are separate validation claims.
- Run `python -m unittest discover -s tests -v` and
  `python tools/check_no_game_content.py --all` for importer/content changes.
- For runtime changes, configure Win32 with `cmake -S runtime -B build/runtime -A Win32`,
  build Release and run `ctest --test-dir build/runtime -C Release --output-on-failure`.
- Use short windowed game tests only when authorized. Avoid foreground game launches
  while the owner is away. Never copy test fixture DLLs into an actual installation.
