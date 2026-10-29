import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("upstream_check", Path(__file__).parents[1] / "scripts/check-upstream.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class UpstreamCheckTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.commit("initial\n")
        self.base = self.git("rev-parse", "HEAD")
        self.git("update-ref", "refs/remotes/upstream/main", self.base)

    def git(self, *args):
        return CHECK.git(self.root, *args)

    def commit(self, text):
        (self.root / "fixture.txt").write_text(text, encoding="utf-8")
        self.git("add", "fixture.txt")
        self.git("commit", "-qm", "fixture")

    def test_current_base(self):
        report = CHECK.inspect(self.root)
        self.assertEqual(report["upstream_only_commits"], 0)
        self.assertEqual(report["text_conflicts"], [])

    def test_missing_update_and_preserved_working_copy(self):
        self.commit("upstream\n")
        self.git("update-ref", "refs/remotes/upstream/main", "HEAD")
        self.git("checkout", "-q", "--detach", self.base)
        (self.root / "untracked.txt").write_text("preserve", encoding="utf-8")
        report = CHECK.inspect(self.root)
        self.assertEqual(report["upstream_only_commits"], 1)
        self.assertEqual(report["upstream_files"], ["fixture.txt"])
        self.assertIn("untracked.txt", report["working_tree"])
        self.assertEqual(self.git("rev-parse", "HEAD"), self.base)
        self.assertEqual((self.root / "untracked.txt").read_text(), "preserve")

    def test_conflicting_changes(self):
        self.commit("upstream\n")
        self.git("update-ref", "refs/remotes/upstream/main", "HEAD")
        self.git("checkout", "-q", "--detach", self.base)
        self.commit("local\n")
        before = self.git("rev-parse", "HEAD")
        report = CHECK.inspect(self.root)
        self.assertEqual(report["text_conflicts"], ["fixture.txt"])
        self.assertEqual(self.git("rev-parse", "HEAD"), before)
        self.assertFalse(report["merge_in_progress"])


if __name__ == "__main__":
    unittest.main()
