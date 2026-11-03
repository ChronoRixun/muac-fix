"""Validate MSI names and reconstruct installation paths without running actions."""
from dataclasses import dataclass
from pathlib import PurePosixPath
import re


class SetupError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def safe_relative(value):
    value = value.replace('\\', '/')
    parts = value.split('/')
    reserved = re.compile(r'^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)', re.I)
    if (not value or any(not p or p in ('.', '..') or p.endswith((' ', '.')) or
                        reserved.match(p) or any(ord(c) < 32 or c in ':<>"|?*' for c in p)
                        for p in parts)):
        raise SetupError('E_PATH', f'Unsafe relative path: {value!r}')
    return str(PurePosixPath(*parts))


def long_name(value):
    return value.split('|')[-1]


@dataclass(frozen=True)
class GameFile:
    key: str
    path: str
    size: int
    cabinet: str | None
    source: str


def game_files(directories, components, files, media):
    try:
        return _game_files(directories, components, files, media)
    except (ValueError, TypeError, KeyError, AttributeError, OverflowError) as exc:
        raise SetupError('E_MSI', 'Invalid or missing installation database metadata.') from exc


def _game_files(directories, components, files, media):
    dirs = {key: (parent, name) for key, parent, name in directories}
    comps = dict(components)
    discs = sorted((int(disk), int(last), cab) for disk, last, cab in media)

    def directory(key, source=False, seen=()):
        if key in seen:
            raise SetupError('E_MSI', 'Cycle in installation directories.')
        if not source and key == 'INSTALLDIR':
            return ''
        if key == 'TARGETDIR' or not key:
            return '' if source else None
        if key not in dirs:
            raise SetupError('E_MSI', f'Missing directory: {key}')
        parent, default = dirs[key]
        root = directory(parent, source, seen + (key,))
        if root is None:
            return None
        names = default.split(':', 1)
        name = long_name(names[-1] if source else names[0])
        if name == '.':
            return root
        return safe_relative('/'.join(filter(None, (root, name))))

    result, used, keys = [], set(), set()
    for key, component, name, size, attrs, seq in files:
        if component not in comps:
            raise SetupError('E_MSI', f'Missing component: {component}')
        folder = directory(comps[component])
        if folder is None:
            continue
        path = safe_relative('/'.join(filter(None, (folder, long_name(name)))))
        if path.casefold() in used or key.casefold() in keys:
            raise SetupError('E_MSI', f'Duplicate installation path or file key: {path}')
        used.add(path.casefold())
        keys.add(key.casefold())
        safe_relative(key)  # cabinet member identifiers must be names, never paths
        if '/' in key or '\\' in key:
            raise SetupError('E_MSI', 'A cabinet file key is not a filename.')
        cabinet = next((cab for _, last, cab in discs if int(seq) <= last), None)
        if cabinet is None:
            raise SetupError('E_MSI', f'No media record for {path}')
        cabinet = safe_relative(cabinet) if cabinet else None
        if int(attrs or 0) & 8192:
            cabinet = None
        source = safe_relative('/'.join(filter(None, (directory(comps[component], True), long_name(name)))))
        length = int(size)
        if length < 0:
            raise SetupError('E_MSI', 'Negative file size.')
        result.append(GameFile(key, path, length, cabinet, source))
    return result
