import contextlib
import io
import json
import os
import threading
import unittest
from unittest.mock import patch

from muac_setup.cli import emit, listen_for_cancel, main


class CliTests(unittest.TestCase):
    def test_cancel_listener_handles_pipe_and_eof(self):
        read, write = os.pipe()
        cancel = threading.Event()
        with os.fdopen(read) as source, os.fdopen(write, 'w') as target:
            listener = threading.Thread(target=listen_for_cancel, args=(source, cancel))
            listener.start()
            target.write('ignored\n cancel \n')
            target.flush()
            self.assertTrue(cancel.wait(2))
            listener.join(2)
            self.assertFalse(listener.is_alive())
        cancel.clear()
        listen_for_cancel(io.StringIO('ignored\n'), cancel)
        self.assertFalse(cancel.is_set())

    def test_emit_preserves_original_keys_and_adds_launcher_progress(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            emit('progress', {'stage': 'verify', 'done': 1, 'total': 2})
            emit('error', {'code': 'E_TEST', 'message': 'fixture'})
        progress, error = map(json.loads, output.getvalue().splitlines())
        self.assertEqual((progress['event'], progress['ev'], progress['pct'], progress['overall']), ('progress', 'progress', 50, 80))
        self.assertEqual(error['message'], error['msg'])

    def test_result_exit_matches_success_or_failed_verification(self):
        for complete in (True, False):
            with self.subTest(complete=complete), patch('muac_setup.cli.verify', return_value={'files_complete': complete}):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    code = main(['verify', '--out', 'fixture'])
                event = json.loads(output.getvalue())
                self.assertEqual(event['exit'], code)
                self.assertEqual(event['ok'], code == 0)
                self.assertEqual(code, 0 if complete else 1)
