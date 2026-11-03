"""Inspect, import and verify local game files. No game is downloaded or launched."""
import hashlib
import json
import shutil
import struct
import tempfile
from collections import defaultdict
from dataclasses import replace
from pathlib import Path

from . import __version__
from .cabinet import extract_cabinet
from .layout import SetupError, safe_relative
from .msi import MSI_NAME, read_layout
from .sources import assert_no_links, open_source

RETAIL_EXE_SHA256 = 'a92687ea9141d759ee485ceef0ffdac0449af7ff598176a20e8654b4257c53ed'
STAMP = '_muac/setup.json'
INSTALL_SENTINELS = ('game.exe', 'libIGCore.dll', 'libIGGfx.dll', 'data/herostat.engb', 'build.ini')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inspect_executable(data):
    try:
        if data[:2] != b'MZ':
            raise ValueError()
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        if data[pe:pe + 4] != b'PE\0\0':
            raise ValueError()
        machine, count = struct.unpack_from('<HH', data, pe + 4)
        opt_size = struct.unpack_from('<H', data, pe + 20)[0]
        if machine != 0x14c or count > 96:
            raise ValueError()
        offset = pe + 24 + opt_size
        if offset + count * 40 > len(data):
            raise ValueError()
        names = [data[offset + n * 40:offset + n * 40 + 8].rstrip(b'\0') for n in range(count)]
    except (struct.error, ValueError):
        raise SetupError('E_EXE', 'Expected a valid 32-bit PC executable.') from None
    sha = hashlib.sha256(data).hexdigest()
    protected = sha == RETAIL_EXE_SHA256 or (b'BoG_' in data and any(n.startswith(b'stxt') for n in names))
    return {'sha256': sha, 'known_retail': sha == RETAIL_EXE_SHA256,
            'safedisc': protected, 'runtime_verified': False,
            'status': 'retail_protection' if protected else 'unverified_executable'}


def inspect_source(source):
    if source.has(MSI_NAME):
        with tempfile.TemporaryDirectory(prefix='muac-info-') as tmp:
            db = Path(tmp) / 'disc.msi'
            source.copy(MSI_NAME, db)
            props, files = read_layout(db)
        if not files:
            raise SetupError('E_SOURCE', 'The installation database has no game files.')
        exe = next((f for f in files if f.path.casefold() == 'game.exe' and not f.cabinet), None)
        if exe is None:
            raise SetupError('E_SOURCE', 'Unsupported executable layout on this disc.')
        runtime = inspect_executable(source.read(exe.source))
        # Retail mastering wrapped Game.exe after building the MSI. This one
        # fingerprint has a stale MSI size, not a damaged file. Do not generalize
        # the exception to unknown executables or other loose files.
        if runtime['known_retail'] and exe.size == 4681728 and source.entry(exe.source)[1] == 6066073:
            files = [replace(f, size=6066073) if f == exe else f for f in files]
        for file in files:
            source.entry(file.cabinet or file.source)
            if not file.cabinet and source.entry(file.source)[1] != file.size:
                raise SetupError('E_SOURCE', f'Source file size differs from MSI: {file.path}')
        return {'kind': 'disc', 'container': source.kind, 'product': props['ProductName'],
                'file_count': len(files), 'bytes': sum(f.size for f in files),
                'cabinets': len({f.cabinet for f in files if f.cabinet}), 'runtime': runtime}, files
    if all(source.has(path) for path in INSTALL_SENTINELS):
        return {'kind': 'installed', 'container': source.kind,
                'runtime': inspect_executable(source.read('game.exe'))}, []
    raise SetupError('E_WRONG_GAME', 'Folder is neither a supported MUA 2006 PC disc nor a recognizable game installation.')


def check_destination(source, destination):
    source, destination = Path(source).absolute(), Path(destination).absolute()
    assert_no_links(destination)
    source, destination = source.resolve(), destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        raise SetupError('E_DESTINATION', 'Source and output must be separate, non-overlapping locations.')
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise SetupError('E_DESTINATION', 'Choose a new or empty output directory.')


def import_disc(source_path, destination, emit=lambda *_: None, prepare=None, cancelled=lambda: False):
    def check_cancel():
        if cancelled():
            raise SetupError('E_CANCELLED', 'Setup cancelled. No installation was registered.')
    check_cancel()
    destination = Path(destination).absolute()
    check_destination(source_path, destination)
    with open_source(source_path) as source:
        info, files = inspect_source(source)
        if info['kind'] != 'disc':
            raise SetupError('E_INSTALLED', 'This is already installed. Use info to inspect it; no disc extraction is needed.')
        if prepare and not info['runtime'].get('known_retail'):
            raise SetupError('E_RUNTIME_EXE', 'Automatic setup supports only the verified original MUA 2006 retail executable.')
        destination.parent.mkdir(parents=True, exist_ok=True)
        cabinet_names = {f.cabinet.casefold(): f.cabinet for f in files if f.cabinet}
        extra = sum(source.entry(name)[1] for name in cabinet_names.values()) if source.kind == 'iso' else 0
        if shutil.disk_usage(destination.parent).free < info['bytes'] + extra + 128 * 1024 * 1024:
            raise SetupError('E_SPACE', 'Not enough free space for installation and temporary cabinet extraction.')
        # Only a new sibling staging directory is written. Failed imports leave it
        # for diagnosis; never delete a directory chosen by the player.
        stage = Path(tempfile.mkdtemp(prefix='.muac-stage-', dir=destination.parent))
        emit('stage', {'id': 'extract', 'state': 'running', 'staging': str(stage)})
        try:
            groups = defaultdict(list)
            for file in files:
                groups[file.cabinet].append(file)
            done = 0
            for cabinet, members in groups.items():
                check_cancel()
                if cabinet:
                    with tempfile.TemporaryDirectory(prefix='muac-cab-', dir=destination.parent) as tmp:
                        def resolve_volume(name):
                            if name.casefold() not in cabinet_names:
                                raise SetupError('E_CAB', 'Cabinet requested a volume outside the installation database.')
                            actual = cabinet_names[name.casefold()]
                            if source.kind == 'folder':
                                return source.entry(actual)[0]
                            cached = Path(tmp) / actual
                            if not cached.exists():
                                source.copy(actual, cached)
                            return cached
                        extract_cabinet(resolve_volume(cabinet), members, stage, resolve_next=resolve_volume, cancelled=cancelled)
                else:
                    for file in members:
                        check_cancel()
                        source.copy(file.source, stage / file.path)
                done += len(members)
                emit('progress', {'stage': 'extract', 'done': done, 'total': len(files), 'cabinet': cabinet})
            emit('stage', {'id': 'verify', 'state': 'running'})
            manifest = []
            for index, file in enumerate(files):
                check_cancel()
                path = stage / file.path
                if path.stat().st_size != file.size:
                    raise SetupError('E_VERIFY', f'File size differs from MSI: {file.path}')
                manifest.append({'path': file.path, 'size': file.size, 'sha256': digest(path)})
                if index % 1000 == 0:
                    emit('progress', {'stage': 'verify', 'done': index, 'total': len(files)})
            stamp = {'schema': 1, 'tool_version': __version__, 'complete': True,
                     'source': info, 'files': manifest}
            (stage / '_muac').mkdir()
            (stage / STAMP).write_text(json.dumps(stamp, indent=2) + '\n', encoding='utf-8')
            if prepare:
                check_cancel()
                emit('stage', {'id': 'runtime', 'state': 'running'})
                prepare(stage)
            check_cancel()
            check_destination(source_path, destination)
            if destination.exists():
                destination.rmdir()  # only the checked empty directory
            stage.rename(destination)
        except BaseException:
            emit('incomplete', {'staging': str(stage), 'message': 'Import did not finish. No ready installation was registered.'})
            raise
        return {'install': str(destination), 'file_count': len(files), 'bytes': info['bytes'],
                'files_complete': True, 'runtime': info['runtime'], 'prepared': prepare is not None, 'playable': False}


def verify(folder):
    folder = Path(folder).absolute()
    assert_no_links(folder)
    path = folder / STAMP
    try:
        assert_no_links(path)
        stamp = json.loads(path.read_text(encoding='utf-8'))
        if stamp.get('schema') != 1 or stamp.get('complete') is not True or not stamp.get('files'):
            raise ValueError()
        entries = stamp['files']
        if not isinstance(entries, list):
            raise ValueError()
        seen = set()
        for file in entries:
            relative = safe_relative(file['path'])
            if relative.casefold() in seen or not isinstance(file['size'], int) or file['size'] < 0:
                raise ValueError()
            seen.add(relative.casefold())
            if len(file['sha256']) != 64 or any(c not in '0123456789abcdef' for c in file['sha256']):
                raise ValueError()
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as exc:
        raise SetupError('E_MANIFEST', 'Missing or invalid setup manifest; this folder cannot be verified as a managed import.') from exc
    missing, changed = [], []
    for file in entries:
        path = folder / file['path']
        assert_no_links(path)
        if not path.is_file():
            missing.append(file['path'])
        elif path.stat().st_size != file['size'] or digest(path) != file['sha256']:
            changed.append(file['path'])
    return {'files_complete': not missing and not changed, 'checked': len(entries),
            'missing': missing, 'changed': changed, 'runtime_verified': False}
