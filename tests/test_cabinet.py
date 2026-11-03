import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from muac_setup.cabinet import extract_cabinet
from muac_setup.layout import GameFile, SetupError


@unittest.skipUnless(os.name == 'nt', 'Windows cabinet API')
class CabinetTests(unittest.TestCase):
    def test_extracts_file_spanning_allowlisted_cabinets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            content = os.urandom(200000)
            (root / 'payload.bin').write_bytes(content)
            (root / 'test.ddf').write_text('\n'.join([
                '.OPTION EXPLICIT', '.Set CabinetNameTemplate=part*.cab',
                '.Set DiskDirectoryTemplate=.', '.Set Cabinet=on', '.Set Compress=on',
                '.Set MaxDiskSize=65536', '.Set MaxCabinetSize=65536',
                '.Set CompressionType=MSZIP', 'payload.bin']))
            subprocess.run(['makecab', '/F', 'test.ddf'], cwd=root, check=True, capture_output=True)
            out = root / 'out'
            out.mkdir()
            self.assertGreater(len(list(root.glob('part*.cab'))), 1)
            extract_cabinet(root / 'part1.cab', [GameFile('payload.bin', 'data.bin', len(content), 'part1.cab', '')],
                            out, resolve_next=lambda name: root / name)
            self.assertEqual((out / 'data.bin').read_bytes(), content)

    def test_extracts_only_requested_member_and_renames_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / 'payload.bin'
            payload.write_bytes(b'fixture content')
            cabinet = root / 'test.cab'
            subprocess.run(['makecab', str(payload), str(cabinet)], check=True, capture_output=True)
            out = root / 'out'
            out.mkdir()
            extract_cabinet(cabinet, [], out)
            self.assertEqual(list(out.iterdir()), [])
            extract_cabinet(cabinet, [GameFile('payload.bin', 'data/renamed.bin', 15, 'test.cab', '')], out)
            self.assertEqual((out / 'data/renamed.bin').read_bytes(), b'fixture content')

    def test_missing_member_is_not_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / 'payload.bin'
            payload.write_bytes(b'x')
            cabinet = root / 'test.cab'
            subprocess.run(['makecab', str(payload), str(cabinet)], check=True, capture_output=True)
            out = root / 'out'
            out.mkdir()
            with self.assertRaises(SetupError):
                extract_cabinet(cabinet, [GameFile('absent', 'absent.bin', 1, 'test.cab', '')], out)
