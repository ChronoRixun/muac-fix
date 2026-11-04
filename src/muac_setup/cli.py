import argparse
import json
import sys
import threading
from pathlib import Path

from . import __version__
from . import sources
from .install import import_disc, inspect_source, verify
from .layout import SetupError
from .sources import open_source


def emit(event, data):
    # Both keys retain the original CLI contract and support the launcher runner.
    data = dict(data)
    if 'message' in data:
        data['msg'] = data['message']
    if event == 'progress' and data.get('total'):
        data['pct'] = 100 * data['done'] / data['total']
        data['overall'] = data['pct'] * (0.65 if data.get('stage') == 'extract' else 0.3) + (65 if data.get('stage') == 'verify' else 0)
        data['unit'] = 'files'
    print(json.dumps({'event': event, 'ev': event, **data}), flush=True)


def listen_for_cancel(stream, cancel):
    try:
        for line in stream:
            if line.strip() == 'cancel':
                cancel.set()
                break
    except (OSError, ValueError):
        pass


def main(argv=None):
    parser = argparse.ArgumentParser(description='MUA 2006 local disc setup (experimental).')
    parser.add_argument('--version', action='version', version=__version__)
    commands = parser.add_subparsers(dest='command', required=True)
    info = commands.add_parser('info', help='Inspect an ISO, extracted disc or installed game folder.')
    info.add_argument('--source', required=True)
    install = commands.add_parser('import', help='Extract a disc into a new game folder.')
    install.add_argument('--source', required=True)
    install.add_argument('--out', required=True)
    setup = commands.add_parser('setup', help='Import and prepare the supported retail game using verified offline runtimes.')
    setup.add_argument('--source', required=True)
    setup.add_argument('--out', required=True)
    setup.add_argument('--bundle', default=str(Path(sys.executable).parent / 'runtime'))
    setup.add_argument('--dgvoodoo', default='', required=not sources.is_wine(),
                       help='Verified dgVoodoo archive (native Windows only; unused on Wine/Proton).')
    check = commands.add_parser('verify', help='Check every file against its import manifest.')
    check.add_argument('--out', required=True)
    args = parser.parse_args(argv)
    cancel = threading.Event()
    if args.command in ('import', 'setup') and not sys.stdin.isatty():
        threading.Thread(target=listen_for_cancel, args=(sys.stdin, cancel), daemon=True).start()
    try:
        if args.command == 'info':
            with open_source(args.source) as source:
                result, _ = inspect_source(source)
        elif args.command == 'import':
            result = import_disc(args.source, args.out, emit, cancelled=cancel.is_set)
        elif args.command == 'setup':
            from .runtime_package import prepare_runtime, validate_bundle
            validate_bundle(args.bundle, args.dgvoodoo)
            emit('plan', {'stages': [{'id': stage, 'title': title, 'weight': weight} for stage, title, weight in
                [('extract', 'Copying your game files', 65), ('verify', 'Verifying the installation', 30), ('runtime', 'Preparing compatibility', 5)]]})
            result = import_disc(args.source, args.out, emit,
                prepare=lambda stage: prepare_runtime(stage, args.bundle, args.dgvoodoo), cancelled=cancel.is_set)
        else:
            result = verify(args.out)
        exit_code = 1 if args.command == 'verify' and not result['files_complete'] else 0
        emit('result', {'ok': exit_code == 0, 'exit': exit_code, **result})
        return exit_code
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
