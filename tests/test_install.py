import tempfile
import hashlib
import json
import os
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

from muac_setup.install import check_destination, inspect_executable, inspect_source, import_disc, verify
from muac_setup.layout import SetupError, GameFile
from muac_setup.sources import open_source


class InstallTests(unittest.TestCase):
    def test_verify_detects_same_size_changes_and_missing_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / '_muac').mkdir()
            file = root / 'fixture.txt'
            file.write_bytes(b'abc')
            (root / '_muac/setup.json').write_text(json.dumps({'schema': 1, 'complete': True,
                'files': [{'path': 'fixture.txt', 'size': 3, 'sha256': hashlib.sha256(b'abc').hexdigest()}]}))
            self.assertTrue(verify(root)['files_complete'])
            file.write_bytes(b'xyz')
            self.assertEqual(verify(root)['changed'], ['fixture.txt'])
            file.unlink()
            self.assertEqual(verify(root)['missing'], ['fixture.txt'])

    @unittest.skipUnless(os.name == 'nt', 'Windows cabinet API')
    def test_import_promotes_verified_files_into_empty_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'disc'
            source.mkdir()
            (source / 'payload.bin').write_bytes(b'fixture')
            subprocess.run(['makecab', str(source / 'payload.bin'), str(source / 'data.cab')],
                           check=True, capture_output=True)
            info = {'kind': 'disc', 'bytes': 7, 'runtime': {'runtime_verified': False}}
            files = [GameFile('payload.bin', 'data/fixture.bin', 7, 'data.cab', '')]
            output = root / 'game'
            output.mkdir()
            # Isolate the MSI reader only; filesystem, cabinet and manifest I/O are real.
            with patch('muac_setup.install.inspect_source', return_value=(info, files)):
                result = import_disc(source, output)
            self.assertTrue(result['files_complete'])
            self.assertFalse(result['playable'])
            self.assertEqual((output / 'data/fixture.bin').read_bytes(), b'fixture')
            self.assertTrue(verify(output)['files_complete'])
            self.assertFalse(list(root.glob('.muac-stage-*')))

    def test_rejects_source_destination_overlap_and_nonempty_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'disc'
            source.mkdir()
            with self.assertRaises(SetupError):
                check_destination(source, source / 'out')
            with self.assertRaises(SetupError):
                check_destination(source, root)
            dest = root / 'output'
            dest.mkdir()
            (dest / 'keep.txt').write_text('keep')
            with self.assertRaises(SetupError):
                check_destination(source, dest)
            self.assertEqual((dest / 'keep.txt').read_text(), 'keep')

    def test_game_exe_name_alone_does_not_identify_an_install(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'game.exe').write_bytes(b'MZ')
            with open_source(root) as source, self.assertRaises(SetupError):
                inspect_source(source)

    def test_truncated_executable_rejected(self):
        with self.assertRaises(SetupError):
            inspect_executable(b'MZ')

    def test_unmanaged_folder_cannot_pass_verify(self):
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(SetupError):
            verify(Path(tmp))


if __name__ == '__main__':
    unittest.main()
