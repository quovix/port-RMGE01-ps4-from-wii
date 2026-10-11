# SPDX-License-Identifier: Apache-2.0
# @orig none — fixtures e testes originais (quovix), Apache-2.0
"""Testes de vazamentos via índice, histórico e cópia disfarçada."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location("publication_check", Path(__file__).with_name("check.py"))
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)
ORIGINAL = b"# @orig none - fixture original\nprint('fixture')\n"


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qvx-publication-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.write("tools/own.py", ORIGINAL)
        self.git("add", "tools/own.py")
        self.git("commit", "-qm", "fixture original")

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], stderr=subprocess.PIPE)

    def write(self, path, data):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

    def test_original_checkout_and_history_pass(self):
        failures, counts = CHECK.audit(self.root, "all")
        self.assertEqual(failures, [])
        self.assertEqual(counts["commits"], 1)

    def test_force_added_ignored_object_is_rejected(self):
        self.write(".gitignore", b"*.o\n")
        self.write("tools/hidden.o", b"object payload")
        self.git("add", ".gitignore")
        self.assertEqual(CHECK.audit(self.root, "workspace")[0], [])
        self.git("add", "-f", "tools/hidden.o")
        failures, _ = CHECK.audit(self.root, "index")
        self.assertTrue(any("tools/hidden.o" in failure for failure in failures))

    def test_index_is_audited_instead_of_working_copy(self):
        self.write("tools/own.py", b"# @orig RMGE01/main.dol @ 0x80000000\npass\n")
        self.git("add", "tools/own.py")
        self.write("tools/own.py", ORIGINAL)
        self.assertEqual(CHECK.audit(self.root, "workspace")[0], [])
        self.assertTrue(CHECK.audit(self.root, "index")[0])

    def test_deleted_game_file_is_still_rejected_in_history(self):
        self.write("src/Game/fixture.cpp", b"// synthetic fixture\nint fixture;\n")
        self.git("add", "src/Game/fixture.cpp")
        self.git("commit", "-qm", "fixture proibido")
        self.git("rm", "-q", "src/Game/fixture.cpp")
        self.git("commit", "-qm", "remove fixture")
        self.assertEqual(CHECK.audit(self.root, "index")[0], [])
        failures, _ = CHECK.audit(self.root, "history")
        self.assertTrue(any("src/Game/fixture.cpp" in failure for failure in failures))

    def test_literal_copy_cannot_be_hidden_with_original_marker(self):
        # Corpus inteiramente sintético; nenhum source do jogo nos testes.
        private = "\n".join(f"int fixture_private_{i}(int x) {{ return x + {i}; }}" for i in range(30))
        renamed = ("// @orig none - claim incorreto\n" + private.replace(" ", "  ")).encode()
        error = CHECK.validate("runtime/renamed.cpp", renamed, private=CHECK.fingerprints(private))
        self.assertIn("coincide", error)

    def test_unknown_binary_and_links_are_rejected(self):
        self.assertIsNotNone(CHECK.validate("docs/notes.md", b"binary\0payload"))
        self.assertIsNotNone(CHECK.validate("package/sce_sys/icon0.png", b"arte substituida"))
        self.assertIsNotNone(CHECK.validate("tools/link.py", b"target", "120000"))
        self.assertIsNotNone(CHECK.validate("tools/module", b"", "160000"))


if __name__ == "__main__":
    unittest.main()
