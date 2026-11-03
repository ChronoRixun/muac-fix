import io
import tempfile
import unittest
from pathlib import Path

import pycdlib

from muac_setup.layout import SetupError
from muac_setup.sources import open_source


class SourceTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()
