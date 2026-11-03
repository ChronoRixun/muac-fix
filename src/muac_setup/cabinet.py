"""Allowlisted CAB extraction using Windows SetupAPI, with no subprocesses."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path

from .layout import SetupError, safe_relative
from .sources import assert_no_links


class FileInCabinet(C.Structure):
    _fields_ = [('name', W.LPCWSTR), ('size', W.DWORD), ('error', W.DWORD),
                ('date', W.WORD), ('time', W.WORD), ('attributes', W.WORD),
                ('target', W.WCHAR * 260)]


class FilePaths(C.Structure):
    _fields_ = [('target', W.LPCWSTR), ('source', W.LPCWSTR),
                ('error', W.UINT), ('flags', W.DWORD)]


class CabinetInfo(C.Structure):
    _fields_ = [('path', W.LPCWSTR), ('file', W.LPCWSTR), ('disk', W.LPCWSTR),
                ('set_id', W.USHORT), ('number', W.USHORT)]


def extract_cabinet(cabinet, files, output, resolve_next=None, cancelled=lambda: False):
    output = Path(output).absolute()
    assert_no_links(output)
    wanted = {f.key.casefold(): f for f in files}
    if len(wanted) != len(files):
        raise SetupError('E_CAB', 'Duplicate cabinet keys.')
    seen, errors = set(), []
    api = C.WinDLL('setupapi', use_last_error=True)
    callback_type = C.WINFUNCTYPE(W.UINT, C.c_void_p, W.UINT, C.c_size_t, C.c_size_t)

    @callback_type
    def callback(_, notification, param1, param2):
        try:
            if cancelled():
                raise SetupError('E_CANCELLED', 'Setup cancelled during extraction.')
            if notification == 0x11:  # SPFILENOTIFY_FILEINCABINET
                entry = C.cast(param1, C.POINTER(FileInCabinet)).contents
                key = entry.name.casefold()
                if key not in wanted:
                    return 2  # FILEOP_SKIP
                file = wanted[key]
                if key in seen or entry.size != file.size:
                    raise SetupError('E_CAB', f'Unexpected cabinet member size or duplicate: {file.path}')
                target = output / safe_relative(file.path)
                assert_no_links(target)
                if target.exists():
                    raise SetupError('E_CAB', f'Refusing to overwrite {file.path}')
                if len(str(target)) >= 260:
                    raise SetupError('E_PATH', 'Choose a shorter output directory (CAB API path limit).')
                target.parent.mkdir(parents=True, exist_ok=True)
                entry.target = str(target)
                seen.add(key)
                return 1  # FILEOP_DOIT
            if notification == 0x13:  # SPFILENOTIFY_FILEEXTRACTED
                paths = C.cast(param1, C.POINTER(FilePaths)).contents
                if paths.error:
                    raise SetupError('E_CAB', f'Cabinet write failed (Windows error {paths.error}).')
            if notification == 0x12:  # NEEDNEWCABINET: resolve only source-allowlisted volumes
                info = C.cast(param1, C.POINTER(CabinetInfo)).contents
                name = safe_relative(info.file)
                if '/' in name or resolve_next is None:
                    raise SetupError('E_CAB', 'Required continuation cabinet is unavailable.')
                next_path = Path(resolve_next(name)).absolute()
                assert_no_links(next_path)
                if not next_path.is_file():
                    raise SetupError('E_CAB', 'Required continuation cabinet is missing.')
                new_folder = str(next_path.parent) + '\\'
                if len(new_folder) >= 260:
                    raise SetupError('E_PATH', 'Cabinet source path is too long.')
                value = C.create_unicode_buffer(new_folder)
                C.memmove(param2, value, C.sizeof(value))
            return 0
        except Exception as exc:
            errors.append(exc)
            return 0 if notification == 0x11 else 1  # FILEOP_ABORT / Windows error

    api.SetupIterateCabinetW.argtypes = [W.LPCWSTR, W.DWORD, callback_type, C.c_void_p]
    api.SetupIterateCabinetW.restype = W.BOOL
    success = api.SetupIterateCabinetW(str(cabinet), 0, callback, None)
    if errors:
        raise errors[0]
    if not success:
        raise SetupError('E_CAB', f'Could not extract cabinet (Windows error {C.get_last_error()}).')
    if seen != set(wanted):
        raise SetupError('E_CAB', 'Cabinet is missing required game files.')
    for file in files:
        if (output / file.path).stat().st_size != file.size:
            raise SetupError('E_CAB', f'Incomplete file: {file.path}')
