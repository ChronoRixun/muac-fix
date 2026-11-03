import argparse
import json
import sys

from . import __version__
from .install import import_disc, inspect_source, verify
from .layout import SetupError
from .sources import open_source


def emit(event, data):
    print(json.dumps({'event': event, **data}), flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description='MUA 2006 local disc setup (experimental).')
    parser.add_argument('--version', action='version', version=__version__)
    commands = parser.add_subparsers(dest='command', required=True)
    info = commands.add_parser('info', help='Inspect an ISO, extracted disc or installed game folder.')
    info.add_argument('--source', required=True)
    install = commands.add_parser('import', help='Extract a disc into a new game folder.')
    install.add_argument('--source', required=True)
    install.add_argument('--out', required=True)
    check = commands.add_parser('verify', help='Check every file against its import manifest.')
    check.add_argument('--out', required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == 'info':
            with open_source(args.source) as source:
                result, _ = inspect_source(source)
        elif args.command == 'import':
            result = import_disc(args.source, args.out, emit)
        else:
            result = verify(args.out)
        emit('result', result)
        return 1 if args.command == 'verify' and not result['files_complete'] else 0
    except SetupError as exc:
        emit('error', {'code': exc.code, 'message': str(exc)})
        return 2
    except KeyboardInterrupt:
        emit('error', {'code': 'E_CANCELLED', 'message': 'Setup cancelled.'})
        return 130
    except OSError as exc:
        emit('error', {'code': 'E_IO', 'message': str(exc)})
        return 3


if __name__ == '__main__':
    sys.exit(main())
