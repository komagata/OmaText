import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/file_io.py"
spec = importlib.util.spec_from_file_location("file_io", SCRIPT)
io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(io)


class FileIoTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "a file;$(nothing).txt"

    def request(self, action, **kwargs):
        result = subprocess.run([sys.executable, SCRIPT], input=json.dumps(dict(action=action, url=self.path.as_uri(), **kwargs)) + "\n", text=True, capture_output=True, timeout=5, check=True)
        self.assertEqual(result.stderr, "")
        return json.loads(result.stdout)

    def test_unicode_roundtrip_and_path_are_data(self):
        text = "Hello\n\u65e5\u672c\u8a9e\n\U0001f642"
        self.assertTrue(self.request("write", text=text)["ok"])
        self.assertEqual(self.request("read")["text"], text)
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)

    def test_bom_crlf_and_permissions(self):
        self.path.write_bytes(b"\xef\xbb\xbfhello\r\n")
        self.path.chmod(0o640)
        result = self.request("read")
        self.assertTrue(result["bom"] and result["crlf"])
        self.assertEqual(result["text"], "hello\n")
        self.assertTrue(self.request("write", text="hello\nworld\n", bom=True, crlf=True)["ok"])
        self.assertEqual(self.path.read_bytes(), b"\xef\xbb\xbfhello\r\nworld\r\n")
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o640)

    def test_invalid_and_oversized_files(self):
        for content in [b"\xff", b"a\0b", b"x" * (io.MAX_BYTES + 1)]:
            self.path.write_bytes(content)
            self.assertFalse(self.request("read")["ok"])
            self.assertEqual(self.path.read_bytes(), content)

    def test_fifo_never_blocks(self):
        os.mkfifo(self.path)
        self.assertFalse(self.request("read")["ok"])
        self.assertFalse(self.request("write", text="x")["ok"])

    def test_failed_replace_preserves_original_and_cleans_temp(self):
        self.path.write_text("original")
        with patch.object(io.os, "replace", side_effect=OSError("disk error")):
            result = io.handle(dict(action="write", url=self.path.as_uri(), text="changed"))
        self.assertFalse(result["ok"])
        self.assertEqual(self.path.read_text(), "original")
        self.assertEqual(list(self.path.parent.iterdir()), [self.path])

    def test_symlink_preserves_link(self):
        target = self.path.parent / "target.txt"
        target.write_text("original")
        self.path.symlink_to(target)
        self.assertTrue(self.request("write", text="changed")["ok"])
        self.assertTrue(self.path.is_symlink())
        self.assertEqual(target.read_text(), "changed")

    def test_lazy_listing_encodes_names_and_skips_hidden_links_specials(self):
        root = self.path.parent
        name = '日本語 space # "quote".md'
        (root / name).write_text("hello")
        (root / "nested").mkdir()
        (root / "nested" / "child.txt").write_text("child")
        (root / ".hidden").write_text("hidden")
        (root / "cycle").symlink_to(root, target_is_directory=True)
        os.mkfifo(root / "pipe")
        result = io.handle(dict(action="list", url=root.as_uri()))
        self.assertTrue(result["ok"], result)
        self.assertEqual({e["name"] for e in result["entries"]}, {name, "nested"})
        entry = next(e for e in result["entries"] if e["name"] == name)
        self.assertEqual(io.local_path(entry["url"]), root / name)
        self.assertFalse(io.handle(dict(action="list", url=(root / "cycle").as_uri()))["ok"])

    def test_listing_is_bounded_and_errors_are_retryable(self):
        root = self.path.parent
        for i in range(300):
            (root / f"file-{i}").touch()
        result = io.handle(dict(action="list", url=root.as_uri()))
        self.assertTrue(result["ok"], result)
        self.assertLessEqual(len(result["entries"]), 256)
        self.assertTrue(result["truncated"])
        self.assertLessEqual(len(json.dumps(result).encode()), 131072)
        with patch.object(io.os, "scandir", side_effect=PermissionError()):
            self.assertFalse(io.handle(dict(action="list", url=root.as_uri()))["ok"])
        self.assertFalse(io.handle(dict(action="list", url=(root / "deleted").as_uri()))["ok"])
        self.assertTrue(io.handle(dict(action="list", url=root.as_uri()))["ok"])

    def test_rejects_remote_urls_and_bad_requests(self):
        for url in ["https://example.org/a", "file://remote/a", "file:relative", "file:///a%00b"]:
            self.assertFalse(io.handle(dict(action="read", url=url))["ok"])
        self.assertFalse(io.handle([])["ok"])
        result = subprocess.run([sys.executable, SCRIPT], input="not-json\n", capture_output=True, text=True, timeout=5)
        self.assertFalse(json.loads(result.stdout)["ok"])
        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
