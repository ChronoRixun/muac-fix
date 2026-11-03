"""Windows Installer read-only database access. Never installs or runs actions."""
import ctypes as C
from ctypes import wintypes as W

from .layout import SetupError, game_files

PRODUCT = '{932FB3F3-594D-4600-ABFA-F2DE80A14214}'
MSI_NAME = 'Marvel(TM) - Ultimate Alliance.msi'


class Database:
    def __init__(self, path):
        self.api = C.WinDLL('msi', use_last_error=True)
        signatures = {
            'MsiOpenDatabaseW': [W.LPCWSTR, W.LPCWSTR, C.POINTER(W.UINT)],
            'MsiDatabaseOpenViewW': [W.UINT, W.LPCWSTR, C.POINTER(W.UINT)],
            'MsiViewExecute': [W.UINT, W.UINT],
            'MsiViewFetch': [W.UINT, C.POINTER(W.UINT)],
            'MsiRecordGetFieldCount': [W.UINT],
            'MsiRecordGetStringW': [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)],
            'MsiCloseHandle': [W.UINT],
        }
        for name, args in signatures.items():
            fn = getattr(self.api, name)
            fn.argtypes, fn.restype = args, W.UINT
        self.handle = W.UINT()
        self.check(self.api.MsiOpenDatabaseW(str(path), None, C.byref(self.handle)))

    def check(self, code):
        if code:
            raise SetupError('E_MSI', f'Could not read installation database (Windows error {code}).')

    def close(self):
        self.api.MsiCloseHandle(self.handle)

    def query(self, sql):
        view = W.UINT()
        self.check(self.api.MsiDatabaseOpenViewW(self.handle, sql, C.byref(view)))
        try:
            self.check(self.api.MsiViewExecute(view, 0))
            rows = []
            while True:
                record = W.UINT()
                code = self.api.MsiViewFetch(view, C.byref(record))
                if code == 259:
                    return rows
                self.check(code)
                try:
                    row = []
                    for index in range(1, self.api.MsiRecordGetFieldCount(record) + 1):
                        length = W.DWORD(32768)
                        buf = C.create_unicode_buffer(length.value)
                        self.check(self.api.MsiRecordGetStringW(record, index, buf, C.byref(length)))
                        row.append(buf.value)
                    rows.append(tuple(row))
                finally:
                    self.api.MsiCloseHandle(record)
        finally:
            self.api.MsiCloseHandle(view)


def read_layout(path):
    db = Database(path)
    try:
        props = dict(db.query('SELECT `Property`, `Value` FROM `Property`'))
        if not props.get('ProductName', '').strip():
            raise SetupError('E_MSI', 'Installation database is missing its product name.')
        if props.get('ProductCode', '').upper() != PRODUCT:
            raise SetupError('E_WRONG_GAME', 'This is not the supported MUA 2006 PC DVD.')
        files = game_files(
            db.query('SELECT `Directory`, `Directory_Parent`, `DefaultDir` FROM `Directory`'),
            db.query('SELECT `Component`, `Directory_` FROM `Component`'),
            db.query('SELECT `File`, `Component_`, `FileName`, `FileSize`, `Attributes`, `Sequence` FROM `File`'),
            db.query('SELECT `DiskId`, `LastSequence`, `Cabinet` FROM `Media`'))
        return props, files
    finally:
        db.close()
