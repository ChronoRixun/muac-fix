import unittest

from muac_setup.layout import SetupError, safe_relative, game_files


class LayoutTests(unittest.TestCase):
    def test_invalid_msi_numbers_are_structured_errors(self):
        for size, attrs, seq, last in [('', '8192', '1', '1'), ('1', 'x', '1', '1'),
                                      ('1', '8192', 'x', '1'), ('1', '8192', '1', 'x')]:
            with self.subTest(size=size, attrs=attrs, seq=seq, last=last):
                with self.assertRaises(SetupError) as caught:
                    game_files([('INSTALLDIR', '', '.')], [('c', 'INSTALLDIR')],
                               [('f', 'c', 'game.exe', size, attrs, seq)], [('1', last, 'a.cab')])
                self.assertEqual(caught.exception.code, 'E_MSI')

    def test_refuses_unsafe_windows_paths(self):
        for name in ('../outside', '/root', 'C:/game', 'x:stream', 'a/../b',
                     'CON', 'nul.txt', 'file.', 'file ', 'a\\..\\b', 'a//b'):
            with self.subTest(name=name), self.assertRaises(SetupError):
                safe_relative(name)

    def test_normalizes_separator(self):
        self.assertEqual(safe_relative('data\\actors.bin'), 'data/actors.bin')

    def test_maps_cabinet_file_identifier_to_install_path(self):
        directories = [('TARGETDIR', '', 'SourceDir'), ('INSTALLDIR', 'TARGETDIR', '.:Setup'),
                       ('DATA', 'INSTALLDIR', 'data'), ('OUTSIDE', 'TARGETDIR', 'other')]
        components = [('C1', 'DATA'), ('C2', 'OUTSIDE')]
        files = [('key1', 'C1', 'SHORT.BIN|long name.bin', '12', '16384', '1'),
                 ('key2', 'C2', 'other.bin', '10', '16384', '2')]
        result = game_files(directories, components, files, [('1', '2', 'data.cab')])
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].path, 'data/long name.bin')
        self.assertEqual(result[0].key, 'key1')
        self.assertEqual(result[0].cabinet, 'data.cab')

    def test_refuses_case_collisions(self):
        with self.assertRaises(SetupError):
            game_files([('INSTALLDIR', '', '.')], [('c', 'INSTALLDIR')],
                       [('a', 'c', 'game.exe', '2', '16384', '1'),
                        ('b', 'c', 'GAME.EXE', '2', '16384', '2')], [('1', '2', 'a.cab')])

    def test_refuses_directory_cycle(self):
        with self.assertRaises(SetupError):
            game_files([('A', 'B', 'a'), ('B', 'A', 'b')], [('c', 'A')],
                       [('a', 'c', 'x.bin', '2', '16384', '1')], [('1', '1', 'a.cab')])


if __name__ == '__main__':
    unittest.main()
