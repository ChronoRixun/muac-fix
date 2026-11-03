"""Create a versioned, self-contained setup artifact; never publishes a release."""
import argparse
import hashlib
import importlib.metadata
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=ROOT):
    return subprocess.check_output([str(x) for x in args], cwd=cwd, text=True).strip()


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dll32(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', data, pe + 4)[0] != 0x14c:
        raise ValueError(f'Expected a Win32 DLL: {path.name}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', required=True)
    parser.add_argument('--safedisc-source', required=True, type=Path)
    parser.add_argument('--safedisc-dll', required=True, type=Path)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[a-zA-Z0-9.]+)?', args.version):
        raise ValueError('Expected a release version')
    pins = json.loads((ROOT / 'packaging/dependencies.json').read_text())
    source = args.safedisc_source.resolve()
    if run('git', 'rev-parse', 'HEAD', cwd=source) != pins['safedisc']['commit'] or run('git', 'diff', 'HEAD', '--', cwd=source):
        raise ValueError('SafeDiscLoader2 source must match the pinned unmodified revision')
    dll32(args.runtime)
    dll32(args.safedisc_dll)
    # A fresh output avoids recursive deletion and accidental contamination by an old build.
    (ROOT / 'build').mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='package-' + args.version + '-', dir=ROOT / 'build'))
    subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onedir', '--noupx',
        '--name', 'muac-setup', '--paths', str(ROOT / 'src'), '--distpath', str(work / 'frozen'),
        '--workpath', str(work / 'work'), '--specpath', str(work), str(ROOT / 'packaging/entry.py')], check=True)
    package = work / 'frozen/muac-setup'
    runtime = package / 'runtime'
    runtime.mkdir()
    payload = [(args.runtime, 'dinput8.dll', 'dinput8.dll'), (args.safedisc_dll, 'version.dll', 'version.dll'),
        (ROOT / 'packaging/muac-fix.ini', 'muac-fix.ini', 'muac-fix.ini'),
        (ROOT / 'packaging/dgVoodoo.conf', 'dgVoodoo.conf', 'dgVoodoo.conf'),
        (ROOT / 'packaging/THIRD_PARTY.txt', 'THIRD_PARTY.txt', '_muac/THIRD_PARTY.txt'),
        (ROOT / 'LICENSE', 'muac-license.txt', '_muac/licenses/muac-fix.txt'),
        (source / 'LICENSE', 'safedisc-license.txt', '_muac/licenses/SafeDiscLoader2.txt')]
    (runtime / 'version.json').write_text('{"logging":false}\n', encoding='utf-8')
    files = [{'source': 'version.json', 'target': 'version.json', 'size': (runtime / 'version.json').stat().st_size,
              'sha256': sha(runtime / 'version.json')}]
    for origin, name, target in payload:
        shutil.copy2(origin, runtime / name)
        files.append({'source': name, 'target': target, 'size': origin.stat().st_size, 'sha256': sha(origin)})
    (runtime / 'package.json').write_text(json.dumps({'schema': 1, 'version': args.version,
        'files': files, 'dgvoodoo': pins['dgvoodoo'], 'safedisc': pins['safedisc']}, indent=2) + '\n')
    sources = package / 'sources'
    sources.mkdir()
    iso_source = pins['pycdlib_source']
    source_archive = sources / iso_source['filename']
    with urllib.request.urlopen(iso_source['url'], timeout=60) as response, source_archive.open('xb') as output:
        shutil.copyfileobj(response, output)
    if sha(source_archive) != iso_source['sha256']:
        raise ValueError('pycdlib source archive failed SHA-256 verification')
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(sources / 'SafeDiscLoader2-source.zip'),
                    pins['safedisc']['commit']], cwd=source, check=True)
    names = run('git', 'ls-files', '--cached', '--others', '--exclude-standard').splitlines()
    with zipfile.ZipFile(sources / 'muac-fix-source.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for name in names:
            file = ROOT / name
            if file.is_file():
                z.write(file, name)
    licenses = package / 'licenses'
    licenses.mkdir()
    shutil.copy2(ROOT / 'packaging/THIRD_PARTY.txt', package / 'THIRD_PARTY.txt')
    shutil.copy2(ROOT / 'LICENSE', licenses / 'muac-fix.txt')
    shutil.copy2(source / 'LICENSE', licenses / 'SafeDiscLoader2.txt')
    python_license = Path(sys.base_prefix) / 'LICENSE.txt'
    shutil.copy2(python_license, licenses / 'Python.txt')
    for project in ('pycdlib', 'pyinstaller'):
        dist = importlib.metadata.distribution(project)
        for file in dist.files or []:
            if 'license' in file.name.lower() or 'copying' in file.name.lower():
                located = Path(dist.locate_file(file))
                if located.is_file():
                    shutil.copy2(located, licenses / (project + '-' + file.name))
    (sources / 'BUILD.txt').write_text('Build using packaging/requirements.txt, runtime/CMakeLists.txt and tools/package_setup.py.\n'
        'To modify or replace the LGPL pycdlib library, extract the included source archive, install your modified copy into the packaging Python environment, and rerun tools/package_setup.py. No signing key is required.\n'
        'SafeDiscLoader2: MSBuild version-proxy.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:ForceImportBeforeCppTargets=<muac-fix>/packaging/static-runtime.props\n'
        'muac-fix revision: ' + run('git', 'rev-parse', 'HEAD') + '\n'
        'working tree contains changes: ' + str(bool(run('git', 'status', '--porcelain'))) + '\n')
    args.out.mkdir(parents=True, exist_ok=True)
    archive = args.out / f'muac-setup-{args.version}-win64.zip'
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
        for file in sorted(package.rglob('*')):
            if file.is_file():
                z.write(file, file.relative_to(package).as_posix())
    manifest = {'version': args.version, 'api': 1, 'content_version': 1, 'zip': archive.name,
                'size': archive.stat().st_size, 'sha256': sha(archive), 'min_launcher': '0.5.0-beta.4'}
    (args.out / 'muac-setup.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (args.out / 'SHA256SUMS.txt').write_text(manifest['sha256'] + '  ' + archive.name + '\n')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
