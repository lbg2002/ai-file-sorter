import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from filesort_lite import core


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "incoming"
        self.root.mkdir()
        (self.root / "paper.pdf").write_text("hello")
        (self.root / "photo.jpg").write_text("image")
        self.root, self.items, _ = core.scan(self.root)
        self.journals = Path(self.tmp.name) / "journal"

    def test_one_batch_manifest_and_omissions(self):
        m = json.loads(core.manifest(self.root, self.items))
        self.assertEqual(len(m["items"]), 2)
        p, bad = core.parse_plan('{"moves":[{"id":"1","category":"文档"},{"id":"999","category":"照片"}]}', self.items)
        _, issues = core.validate(self.root, self.items, p, bad)
        self.assertTrue(any("无中生有" in e for e in issues))
        self.assertTrue(any("遗漏" in e for e in issues))

    def test_reject_traversal_and_duplicate(self):
        p, issues = core.parse_plan('{"moves":[{"id":"1","category":"../out"},{"id":"1","category":"abc"}]}', self.items)
        _, bad = core.validate(self.root, self.items, p, issues)
        self.assertTrue(any("重复" in e for e in bad))
        self.assertTrue(any("非法" in e for e in bad))

    def test_apply_undo_roundtrip(self):
        mapping = {i["id"]: ("图片" if i["name"].endswith("jpg") else "文档") for i in self.items}
        log = core.apply_plan(self.root, self.items, mapping, self.journals)
        self.assertTrue((self.root / "文档" / "paper.pdf").exists())
        self.assertEqual(json.loads(log.read_text())["status"], "completed")
        moved, errs = core.undo(log)
        self.assertEqual((moved, errs), (2, []))
        self.assertTrue((self.root / "paper.pdf").exists())
        self.assertFalse((self.root / "文档").exists())
        self.assertEqual(json.loads(log.read_text())["status"], "undone")

    def test_conflict_does_not_overwrite(self):
        (self.root / "文档").mkdir()
        (self.root / "文档" / "paper.pdf").write_text("existing")
        mapping = {i["id"]: "文档" for i in self.items}
        with self.assertRaises(core.GuardError):
            core.apply_plan(self.root, self.items, mapping, self.journals)
        self.assertEqual((self.root / "文档" / "paper.pdf").read_text(), "existing")

    def test_external_change_prevents_move_or_undo(self):
        mapping = {i["id"]: "材料" for i in self.items}
        (self.root / "paper.pdf").write_text("changed")
        with self.assertRaises(core.GuardError):
            core.apply_plan(self.root, self.items, mapping, self.journals)
        root, fresh, _ = core.scan(self.root)
        log = core.apply_plan(root, fresh, {i["id"]: "材料" for i in fresh}, self.journals)
        (self.root / "材料" / "paper.pdf").write_text("edited after moving")
        count, errors = core.undo(log)
        self.assertEqual(count, 1)
        self.assertTrue(errors)
        self.assertTrue((self.root / "材料" / "paper.pdf").exists())

    def test_folder_move_opt_in_and_symlinks_skipped(self):
        (self.root / "existing").mkdir()
        (self.root / "link.txt").symlink_to(self.root / "paper.pdf")
        _, flat, ignored = core.scan(self.root)
        self.assertEqual(len(flat), 2)
        self.assertTrue(ignored)
        _, withdirs, _ = core.scan(self.root, include_folders=True)
        self.assertEqual(len(withdirs), 3)

    def test_api_called_once(self):
        class Reply:
            def __enter__(self): return self
            def __exit__(self, *_): pass
        response = Reply()
        response.read = lambda: b'{"choices":[{"message":{"content":"{\\"moves\\":[]}"}}]}'
        # json.load reads response exactly once
        with patch('urllib.request.urlopen', return_value=response) as api:
            core.request_plan(self.root, self.items, "https://example.com/v1", "model", "secret")
        self.assertEqual(api.call_count, 1)


if __name__ == "__main__":
    unittest.main()