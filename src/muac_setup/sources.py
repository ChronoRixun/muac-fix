"""Read-only, case-insensitive access to a disc image or extracted folder."""
import io
import ctypes
from functools import lru_cache
import os
import stat
from pathlib import Path

import pycdlib

from .layout import SetupError, safe_relative


def check_cancel(cancelled):
    if cancelled():
        raise SetupError('E_CANCELLED', 'Setup cancelled during file copy.')


class CancelWriter:
    def __init__(self, target, cancelled):
        self.target, self.cancelled = target, cancelled

    def write(self, data):
        check_cancel(self.cancelled)
        return self.target.write(data)

    def __getattr__(self, name):
        return getattr(self.target, name)


def is_reparse(path):
    return bool(path.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT) if os.name == 'nt' else path.is_symlink()


@lru_cache(maxsize=1)
def is_wine():
    if os.name != 'nt':
        return False
    try:
        # Wine exports this from the already loaded ntdll; no environment-variable
        # heuristics and no exception for arbitrary reparse points on Windows.
        return callable(ctypes.WinDLL('ntdll').wine_get_version)
    except (AttributeError, OSError):
        return False


def is_wine_drive_root(path):
    # Only X:\ itself, never UNC shares, relative paths or links below X:\.
    return (len(path.drive) == 2 and path.drive[1] == ':' and
            path == type(path)(path.anchor) and bool(path.root) and is_wine())


def is_wine_mount_point(path):
    # Wine exposes Unix mounts as directory mount tags resolving to themselves.
    # A real junction redirects elsewhere; symlink/unknown tags are never exempt.
    if not is_wine():
        return False
    try:
        if getattr(path.lstat(), 'st_reparse_tag', 0) != 0xA0000003 or not path.is_dir():
            return False
        real = os.path.realpath(path, strict=True).removeprefix('\\\\?\\')
        return os.path.normcase(real) == os.path.normcase(os.path.abspath(path))
    except (OSError, AttributeError, ValueError):
        return False


def assert_no_links(path):
    for part in (path, *path.parents):
        try:
            linked = is_reparse(part)  # lstat also sees broken links/junctions.
        except FileNotFoundError:
            continue
        if linked and not is_wine_drive_root(part) and not is_wine_mount_point(part):
            raise SetupError('E_PATH', 'Linked folders are not supported for setup.')


class Source:
    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        pass

    def add(self, relative, locator, size):
        key = safe_relative(relative).casefold()
        if key in self.entries:
            raise SetupError('E_SOURCE', f'Case-colliding source paths: {relative}')
        self.entries[key] = (locator, size)

    def has(self, relative):
        return safe_relative(relative).casefold() in self.entries

    def entry(self, relative):
        try:
            return self.entries[safe_relative(relative).casefold()]
        except KeyError:
            raise SetupError('E_SOURCE_MISSING', f'Source file missing: {relative}') from None

    def read(self, relative, limit=16 * 1024 * 1024):
        if self.entry(relative)[1] > limit:
            raise SetupError('E_SOURCE', f'Metadata file too large: {relative}')
        target = io.BytesIO()
        self.copy_to(relative, target)
        return target.getvalue()

    def copy(self, relative, destination, cancelled=lambda: False):
        destination = Path(destination)
        assert_no_links(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open('xb') as stream:
            self.copy_to(relative, stream, cancelled=cancelled)
        if destination.stat().st_size != self.entry(relative)[1]:
            raise SetupError('E_SOURCE', f'Short source read: {relative}')


class DirectorySource(Source):
    kind = 'folder'

    def __init__(self, path):
        self.path = Path(path).absolute()
        assert_no_links(self.path)
        self.entries = {}
        for root, dirs, files in os.walk(self.path, followlinks=False):
            for name in dirs + files:
                candidate = Path(root) / name
                if is_reparse(candidate):
                    raise SetupError('E_PATH', 'Linked files or directories are not supported in a source.')
            for name in files:
                candidate = Path(root) / name
                self.add(candidate.relative_to(self.path).as_posix(), candidate, candidate.stat().st_size)

    def copy_to(self, relative, target, cancelled=lambda: False):
        candidate = self.entry(relative)[0]
        assert_no_links(candidate)
        with candidate.open('rb') as stream:
            while True:
                check_cancel(cancelled)
                data = stream.read(1024 * 1024)
                if not data:
                    break
                target.write(data)


class _Iso9660(pycdlib.PyCdlib):
    # This DVD's UDF directory tags are rejected by pycdlib although its Joliet
    # tree is readable (and matches Windows' mounted view). Read the ISO9660 /
    # Joliet namespace explicitly. Pin pycdlib: this is one private extension point.
    # This never changes bytes in the image and does not support UDF-only images.
    def _parse_volume_descriptors(self):
        result = super()._parse_volume_descriptors()
        self._has_udf = False
        return result


class IsoSource(Source):
    kind = 'iso'

    def __init__(self, path):
        self.path = Path(path).absolute()
        self.entries = {}
        self.iso = _Iso9660()
        try:
            self.iso.open(str(self.path))
            self.namespace = 'joliet_path' if self.iso.has_joliet() else 'iso_path'
            for root, _, files in self.iso.walk(**{self.namespace: '/'}):
                for name in files:
                    actual = root.rstrip('/') + '/' + name
                    relative = actual.lstrip('/')
                    if self.namespace == 'iso_path':
                        # ISO9660 uses a terminal dot for extensionless files.
                        # Keep the original locator for pycdlib reads.
                        relative = relative.rsplit(';', 1)[0].removesuffix('.')
                    record = self.iso.get_record(**{self.namespace: actual})
                    self.add(relative, actual, record.data_length)
        except Exception as exc:
            self.close()
            if isinstance(exc, SetupError):
                raise
            raise SetupError('E_SOURCE', f'Could not read ISO9660/Joliet disc: {exc}') from exc

    def copy_to(self, relative, target, cancelled=lambda: False):
        self.iso.get_file_from_iso_fp(CancelWriter(target, cancelled), **{self.namespace: self.entry(relative)[0]})

    def close(self):
        try:
            self.iso.close()
        except Exception:
            pass


def open_source(path):
    path = Path(path)
    if path.is_dir():
        return DirectorySource(path)
    if path.is_file() and path.suffix.lower() == '.iso':
        assert_no_links(path.absolute())
        return IsoSource(path)
    raise SetupError('E_SOURCE', 'Choose a PC DVD ISO, extracted DVD folder or installed game folder.')
