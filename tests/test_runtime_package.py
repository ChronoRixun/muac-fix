import hashlib
import contextlib
import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from muac_setup.layout import SetupError
from muac_setup.runtime_package import prepare_runtime, validate_bundle


def sha(data):
    return hashlib.sha256(data).hexdigest()


class RuntimePackageTests(unittest.TestCase):
    def test_wine_cli_setup_does_not_require_graphics_argument(self):
        from muac_setup.cli import main
        with tempfile.TemporaryDirectory() as tmp:
            bundle, _, game, _ = self.fixture(Path(tmp))
            with patch('muac_setup.sources.is_wine', return_value=True), \
                    patch('muac_setup.cli.import_disc', return_value={'prepared': True, 'files_complete': True}), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                result = main(['setup', '--source', str(game), '--out', str(game / 'new'), '--bundle', str(bundle)])
            self.assertEqual(result, 0)
            self.assertTrue(json.loads(output.getvalue().splitlines()[-1])['prepared'])

    def test_wine_preparation_needs_no_graphics_archive_and_records_backend(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            archive.unlink()
            with patch('muac_setup.runtime_package.RETAIL_EXE_SHA256', sha(b'retail fixture')), \
                    patch('muac_setup.sources.is_wine', return_value=True):
                prepare_runtime(game, bundle, archive)
            self.assertFalse((game / 'D3D9.dll').exists())
            for name in ('dinput8.dll', 'version.dll', 'muac-fix.ini', 'dgVoodoo.conf'):
                self.assertTrue((game / name).is_file(), name)
            stamp = json.loads((game / '_muac/runtime.json').read_text())
            self.assertEqual(stamp['graphics'], 'wine-d3d9')
            self.assertNotIn('D3D9.dll', [f['target'] for f in stamp['files']])

    def test_wine_still_verifies_payload_before_writing(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            (bundle / 'dinput8.dll').write_bytes(b'bad')
            with patch('muac_setup.sources.is_wine', return_value=True), self.assertRaises(SetupError):
                prepare_runtime(game, bundle, archive)
            self.assertFalse((game / 'version.dll').exists())

    def fixture(self, root):
        bundle = root / 'bundle'
        bundle.mkdir()
        files = []
        for name in ('dinput8.dll', 'version.dll', 'version.json', 'muac-fix.ini', 'dgVoodoo.conf'):
            data = ('synthetic ' + name).encode()
            (bundle / name).write_bytes(data)
            files.append({'source': name, 'target': name, 'size': len(data), 'sha256': sha(data)})
        archive = root / 'graphics.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('MS/x86/D3D9.dll', b'graphics fixture')
        manifest = {'schema': 1, 'version': '0.2.1', 'files': files, 'dgvoodoo': {
            'url': 'https://example.invalid/fixture.zip', 'size': archive.stat().st_size,
            'sha256': sha(archive.read_bytes()), 'member': 'MS/x86/D3D9.dll',
            'dll_size': 16, 'dll_sha256': sha(b'graphics fixture')}}
        (bundle / 'package.json').write_text(json.dumps(manifest))
        game = root / 'game'
        game.mkdir()
        (game / 'Game.exe').write_bytes(b'retail fixture')
        return bundle, archive, game, manifest

    def test_prepares_only_verified_runtime_and_preserves_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            with patch('muac_setup.runtime_package.RETAIL_EXE_SHA256', sha(b'retail fixture')):
                prepare_runtime(game, bundle, archive)
            self.assertEqual((game / 'Game.exe').read_bytes(), b'retail fixture')
            self.assertEqual((game / 'D3D9.dll').read_bytes(), b'graphics fixture')
            self.assertTrue((game / '_muac/runtime.json').is_file())

    def test_corrupt_payload_or_archive_is_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            (bundle / 'version.dll').write_bytes(b'bad')
            with self.assertRaises(SetupError):
                validate_bundle(bundle, archive)
            self.assertFalse((game / 'version.dll').exists())

    def test_unknown_executable_is_not_prepared(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            with self.assertRaises(SetupError):
                prepare_runtime(game, bundle, archive)
            self.assertFalse((game / 'version.dll').exists())

    def test_corrupt_graphics_archive_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle, archive, game, _ = self.fixture(Path(tmp))
            archive.write_bytes(b'corrupt')
            with self.assertRaises(SetupError):
                prepare_runtime(game, bundle, archive)
            self.assertFalse((game / 'dinput8.dll').exists())

    def test_payload_cannot_overwrite_game_or_escape_bundle(self):
        for field, value in [('target', 'Game.exe'), ('source', '../outside')]:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as tmp:
                bundle, archive, _, manifest = self.fixture(Path(tmp))
                manifest['files'][0][field] = value
                (bundle / 'package.json').write_text(json.dumps(manifest))
                with self.assertRaises(SetupError):
                    validate_bundle(bundle, archive)
