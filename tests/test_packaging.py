import hashlib
import importlib.util
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_setup', ROOT / 'tools/package_setup.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackagingTests(unittest.TestCase):
    def test_notices_are_identical_in_package_and_installed_payload(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'src/minhook').mkdir(parents=True)
            (root / 'src/minhook/LICENSE.txt').write_text('synthetic bundled MinHook notice')
            (root / 'src/tiny-json.c').write_text('/* MIT\nPermission is hereby granted to the fixture. */\nvoid fixture();')
            runtime, licenses = root / 'runtime', root / 'licenses'
            runtime.mkdir(); licenses.mkdir()
            entries = package.write_notices(root, runtime, licenses)
            self.assertEqual(len(entries), 5)
            for entry in entries:
                name = entry['source']
                self.assertEqual((runtime / name).read_bytes(), (licenses / name).read_bytes())
                self.assertEqual(entry['target'], '_muac/licenses/' + name)
                self.assertEqual(entry['sha256'], hashlib.sha256((runtime / name).read_bytes()).hexdigest())
            self.assertNotIn('void fixture', (licenses / 'tiny-json-MIT.txt').read_text())

    def test_source_archive_excludes_untracked_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            (root / 'tracked.txt').write_text('source')
            (root / 'untracked.txt').write_text('private scratch')
            subprocess.run(['git', '-C', str(root), 'add', 'tracked.txt'], check=True)
            package.write_source_archive(root, root / 'source.zip')
            with zipfile.ZipFile(root / 'source.zip') as z:
                self.assertEqual(z.namelist(), ['tracked.txt'])

    def test_manifest_hash_and_launcher_floor(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / 'fixture.zip'
            archive.write_bytes(b'fixture')
            result = package.make_manifest(archive, '0.2.0-beta.1')
            self.assertEqual(result['min_launcher'], '0.5.0-beta.5')
            self.assertEqual(result['size'], 7)
            self.assertEqual(result['sha256'], hashlib.sha256(b'fixture').hexdigest())
