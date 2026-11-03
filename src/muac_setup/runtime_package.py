"""Offline, hash-checked runtime preparation inside an unpublished import stage."""
import json
import shutil
import zipfile
from pathlib import Path

from .install import RETAIL_EXE_SHA256, digest
from .layout import SetupError, safe_relative
from .sources import assert_no_links

REQUIRED = {'dinput8.dll', 'version.dll', 'version.json', 'muac-fix.ini', 'dgVoodoo.conf'}
OPTIONAL = {'_muac/THIRD_PARTY.txt', '_muac/licenses/SafeDiscLoader2.txt', '_muac/licenses/muac-fix.txt'} | {
    '_muac/licenses/' + name for name in ('minhook-BSD.txt', 'mua-controller-fix-MIT.txt',
        'xml2-fix-MIT.txt', 'SafeDiscLoader2-Minhook.txt', 'tiny-json-MIT.txt')}


def checked_file(path, size, checksum):
    assert_no_links(path)
    if (not isinstance(size, int) or isinstance(size, bool) or size < 1 or size > 128 * 1024 * 1024 or
            not isinstance(checksum, str) or len(checksum) != 64 or
            not path.is_file() or path.stat().st_size != size or digest(path) != checksum):
        raise SetupError('E_RUNTIME_HASH', f'Runtime file failed verification: {path.name}')


def validate_bundle(bundle, archive):
    bundle, archive = Path(bundle).absolute(), Path(archive).absolute()
    assert_no_links(bundle / 'package.json')
    try:
        manifest = json.loads((bundle / 'package.json').read_text(encoding='utf-8'))
        if manifest['schema'] != 1 or not isinstance(manifest['version'], str):
            raise ValueError('Unsupported runtime package schema')
        seen = set()
        for item in manifest['files']:
            source, target = safe_relative(item['source']), safe_relative(item['target'])
            if target not in REQUIRED | OPTIONAL or target in seen:
                raise ValueError('Unexpected or duplicate runtime target')
            seen.add(target)
            checked_file(bundle / source, item['size'], item['sha256'])
        if not REQUIRED <= seen:
            raise ValueError('Incomplete runtime package')
        graphics = manifest['dgvoodoo']
        if graphics['member'] != 'MS/x86/D3D9.dll':
            raise ValueError('Unexpected graphics archive member')
        checked_file(archive, graphics['size'], graphics['sha256'])
        with zipfile.ZipFile(archive) as z:
            members = [i for i in z.infolist() if i.filename == graphics['member']]
            if len(members) != 1 or members[0].file_size != graphics['dll_size'] or not 0 < members[0].file_size < 32 * 1024 * 1024:
                raise ValueError('Invalid graphics DLL size or duplicate archive member')
            import hashlib
            if hashlib.sha256(z.read(members[0])).hexdigest() != graphics['dll_sha256']:
                raise ValueError('Graphics DLL failed verification')
        return manifest
    except (KeyError, ValueError, TypeError, zipfile.BadZipFile) as exc:
        raise SetupError('E_RUNTIME_PACKAGE', f'Invalid runtime package: {exc}') from exc


def prepare_runtime(folder, bundle, archive):
    folder, bundle, archive = Path(folder), Path(bundle), Path(archive)
    manifest = validate_bundle(bundle, archive)
    assert_no_links(folder / 'Game.exe')
    if digest(folder / 'Game.exe') != RETAIL_EXE_SHA256:
        raise SetupError('E_RUNTIME_EXE', 'Automatic setup supports only the verified original MUA 2006 retail executable.')
    # This function only receives a fresh unpublished staging folder. Never replace
    # a game file or an existing mod, even if a disc layout unexpectedly contains it.
    targets = [item['target'] for item in manifest['files']] + ['D3D9.dll', '_muac/runtime.json']
    for target in targets:
        path = folder / target
        assert_no_links(path)
        if path.exists():
            raise SetupError('E_RUNTIME_CONFLICT', f'Runtime destination already exists: {target}')
    for item in manifest['files']:
        path = folder / item['target']
        path.parent.mkdir(parents=True, exist_ok=True)
        with (bundle / item['source']).open('rb') as source, path.open('xb') as output:
            shutil.copyfileobj(source, output)
        checked_file(path, item['size'], item['sha256'])
    graphics = manifest['dgvoodoo']
    with zipfile.ZipFile(archive) as z, (folder / 'D3D9.dll').open('xb') as output:
        output.write(z.read(graphics['member']))
    checked_file(folder / 'D3D9.dll', graphics['dll_size'], graphics['dll_sha256'])
    (folder / '_muac').mkdir(exist_ok=True)
    stamp = {'schema': 1, 'version': manifest['version'], 'prepared': True,
             'game_exe_sha256': RETAIL_EXE_SHA256,
             'files': [{k: item[k] for k in ('target', 'size', 'sha256')} for item in manifest['files']] +
                      [{'target': 'D3D9.dll', 'size': graphics['dll_size'], 'sha256': graphics['dll_sha256']}]}
    (folder / '_muac/runtime.json').write_text(json.dumps(stamp, indent=2) + '\n', encoding='utf-8')
