import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('session_summary', Path(__file__).resolve().parents[1] / 'scripts/summarize-sessions.py')
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)

class SessionEvidence(unittest.TestCase):
    def test_missing_end_is_not_proof_of_crash(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'session.log'
            path.write_text('[Session] started\n')
            self.assertEqual(summary.summarize(path)['outcome'], 'incomplete-or-unspecified; cause unknown')

    def test_normal_shutdown_with_handled_error(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'session.log'
            path.write_text('[Session] handled-error: failed load\n[Session] end outcome=normal exit=0\n')
            result = summary.summarize(path)
            self.assertEqual(result['outcome'], 'normal')
            self.assertTrue(result['handled_errors_recorded'])

    def test_handled_startup_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'session.log'
            path.write_text('[Session] end outcome=handled-error exit=1\n')
            self.assertEqual(summary.summarize(path)['outcome'], 'handled-error')

    def test_fatal_report_is_direct_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'session.log'
            path.write_text('[Session] started\n')
            path.with_name('session-crash.txt').write_text('fatal fixture')
            self.assertEqual(summary.summarize(path)['outcome'], 'fatal-report-present')

if __name__ == '__main__':
    unittest.main()
