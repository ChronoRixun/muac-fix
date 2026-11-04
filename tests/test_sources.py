import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path, PureWindowsPath
from unittest.mock import Mock, patch

import pycdlib

from muac_setup.layout import SetupError
from muac_setup.sources import open_source, assert_no_links, is_wine, is_wine_drive_root, DirectorySource


class SourceTests(unittest.TestCase):
    def test_shared_wine_mount_point_cases(self):
        cases = json.loads((Path(__file__).parent / 'fixtures/wine-mount-points.json').read_text())
        with tempfile.TemporaryDirectory() as tmp:
            mount = Path(tmp) / 'home'
            mount.mkdir()
            real = os.path.realpath
            lstat = Path.lstat
            for case in cases:
                with self.subTest(case=case['name']), \
                        patch('muac_setup.sources.is_wine', return_value=case['wine']), \
                        patch('muac_setup.sources.is_reparse', side_effect=lambda p: p == mount), \
                        patch.object(Path, 'lstat', autospec=True, side_effect=lambda p: Mock(
                            st_reparse_tag=case['tag']) if p == mount else lstat(p)), \
                        patch.object(Path, 'is_dir', return_value=case['directory']), \
                        patch('muac_setup.sources.os.path.realpath', side_effect=lambda p, **kw: str(
                            mount / 'elsewhere') if case['redirects'] and Path(p) == mount else real(p, **kw)):
                    if case['allowed']:
                        assert_no_links(mount / 'deck/game/new-output')
                    else:
                        with self.assertRaises(SetupError):
                            assert_no_links(mount / 'deck/game/new-output')

    def test_wine_detection_uses_ntdll_export(self):
        import muac_setup.sources as sources
        is_wine.cache_clear()
        try:
            with patch.object(sources.os, 'name', 'nt'), patch.object(sources.ctypes, 'WinDLL', create=True) as dll:
                dll.return_value = Mock(wine_get_version=lambda: b'synthetic')
                self.assertTrue(is_wine())
                dll.assert_called_once_with('ntdll')
                is_wine.cache_clear()
                dll.side_effect = OSError('No loader')
                self.assertFalse(is_wine())
                is_wine.cache_clear()
                dll.side_effect = None
                dll.return_value = object()  # Native ntdll has no Wine export.
                self.assertFalse(is_wine())
        finally:
            is_wine.cache_clear()

    def test_only_wine_drive_root_can_be_a_link(self):
        # Model a Wine Z: mapping without running Wine or touching a real drive.
        WinPath = PureWindowsPath
        path = WinPath('Z:/fixture/nested/disc')
        root = WinPath('Z:/')
        for wine in (False, True):
            with self.subTest(wine=wine), patch('muac_setup.sources.is_wine', return_value=wine):
                with patch('muac_setup.sources.is_reparse', side_effect=lambda p: p == root):
                    if wine:
                        assert_no_links(path)
                        assert_no_links(WinPath('Z:/new/output/not-created'))
                    else:
                        with self.assertRaises(SetupError):
                            assert_no_links(path)
                for linked in (path, WinPath('Z:/fixture'), WinPath('Z:/fixture/nested')):
                    with patch('muac_setup.sources.is_reparse', side_effect=lambda p: p in (root, linked)):
                        with self.assertRaises(SetupError):
                            assert_no_links(path)
                for candidate in ('Z:', 'Z:relative', 'Z:/fixture', '//server/share/'):
                    self.assertFalse(is_wine_drive_root(WinPath(candidate)))

    def test_source_tree_and_iso_still_refuse_links_under_wine(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'disc').mkdir()
            link = root / 'disc' / 'linked'
            link.mkdir()
            with patch('muac_setup.sources.is_wine', return_value=True), patch(
                    'muac_setup.sources.is_reparse', side_effect=lambda p: p == link):
                with self.assertRaises(SetupError):
                    DirectorySource(root / 'disc')
                with self.assertRaises(SetupError):
                    assert_no_links(link / 'new-output')
            image = root / 'disc.iso'
            image.write_bytes(b'synthetic')
            with patch('muac_setup.sources.is_wine', return_value=True), patch(
                    'muac_setup.sources.is_reparse', side_effect=lambda p: p == image):
                with self.assertRaises(SetupError) as error:
                    open_source(image)
                self.assertEqual(error.exception.code, 'E_PATH')

    @unittest.skipUnless(os.name == 'nt', 'Windows directory junctions')
    def test_real_junction_in_source_and_output_remains_forbidden_under_wine(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'disc'
            source.mkdir()
            outside = root / 'outside'
            outside.mkdir()
            (outside / 'keep.txt').write_text('keep')
            link = source / 'linked'
            subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(outside)],
                           check=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            with patch('muac_setup.sources.is_wine', return_value=True):
                with self.assertRaises(SetupError):
                    DirectorySource(source)
                with self.assertRaises(SetupError):
                    assert_no_links(link / 'new-output')
            self.assertEqual((outside / 'keep.txt').read_text(), 'keep')
            link.rmdir()  # Remove only this test's junction, never its target.

    def test_large_folder_and_iso_copy_can_be_cancelled_mid_file(self):
        for kind in ('folder', 'iso'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                data = b'x' * (4 * 1024 * 1024)
                if kind == 'folder':
                    source = root / 'disc'
                    source.mkdir()
                    (source / 'DATA.BIN').write_bytes(data)
                else:
                    source = root / 'disc.iso'
                    iso = pycdlib.PyCdlib()
                    iso.new()
                    iso.add_fp(io.BytesIO(data), len(data), iso_path='/DATA.BIN;1')
                    iso.write(str(source))
                    iso.close()
                checks = []
                def cancelled():
                    checks.append(True)
                    return len(checks) > 2
                with open_source(source) as src, self.assertRaises(SetupError) as error:
                    src.copy('data.bin', root / 'partial.bin', cancelled=cancelled)
                self.assertEqual(error.exception.code, 'E_CANCELLED')
                self.assertLess((root / 'partial.bin').stat().st_size, len(data))

    def test_folder_lookup_is_case_insensitive(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'Setup').mkdir()
            (root / 'Setup' / 'DATA.BIN').write_bytes(b'hello')
            with open_source(root) as src:
                self.assertTrue(src.has('setup/data.bin'))
                self.assertEqual(src.read('SETUP/data.BIN'), b'hello')
                with self.assertRaises(SetupError):
                    src.read('../outside')

    def test_iso_and_folder_return_same_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            iso = pycdlib.PyCdlib()
            iso.new(joliet=3)
            iso.add_fp(io.BytesIO(b'hello'), 5, iso_path='/DATA.BIN;1', joliet_path='/Data.bin')
            image = root / 'disc.iso'
            iso.write(str(image))
            iso.close()
            with open_source(image) as src:
                self.assertEqual(src.read('data.bin'), b'hello')
                src.copy('DATA.BIN', root / 'copied.bin')
            self.assertEqual((root / 'copied.bin').read_bytes(), b'hello')

    def test_invalid_iso_has_structured_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / 'bad.iso'
            image.write_bytes(b'not a disc')
            with self.assertRaises(SetupError) as error:
                open_source(image)
            self.assertEqual(error.exception.code, 'E_SOURCE')

    def test_iso9660_extensionless_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / 'plain.iso'
            iso = pycdlib.PyCdlib()
            iso.new()
            iso.add_fp(io.BytesIO(b'hello'), 5, iso_path='/README.;1')
            iso.add_fp(io.BytesIO(b'data'), 4, iso_path='/DATA.BIN;1')
            iso.write(str(image))
            iso.close()
            with open_source(image) as src:
                self.assertEqual(src.read('readme'), b'hello')
                self.assertEqual(src.read('data.bin'), b'data')


if __name__ == '__main__':
    unittest.main()
