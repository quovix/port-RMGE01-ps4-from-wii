# SPDX-License-Identifier: Apache-2.0
# @orig none — testes/fixtures originais do patch de friend (quovix)
"""Valida acesso privado em C++98 e a separação do header derivado."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("create_overlay", Path(__file__).with_name("create_overlay.py"))
OVERLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OVERLAY)
ROOT = Path(__file__).resolve().parents[2]

# Template sintético criado aqui; não é um recorte do jogo/decomp.
FIXTURE = """struct FixtureBase {};
template<class T> class LinkList : private FixtureBase {
    typedef LinkList Self;
public:
    class Cursor {
        T stored;
        explicit Cursor(T value) : stored(value) {}
        friend class Self;
    };
    static T roundtrip(T value) { Cursor cursor(value); return cursor.stored; }
};
"""


@unittest.skipUnless(shutil.which("clang++"), "requer clang++")
class FriendPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qvx-friend-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.petari = self.base / "private-input"
        self.header = self.petari / "libs/nw4r/include/nw4r/ut/LinkList.h"
        for name in ["libs/RVL_SDK/include/revolution/os/OSFastCast.h", "libs/nw4r/include/nw4r/math/arithmetic.h"]:
            path = self.petari / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// fixture original\n")
        self.header.parent.mkdir(parents=True, exist_ok=True)
        self.header.write_text(FIXTURE)

    def test_rewrite_preserves_friend_access_in_strict_cpp98(self):
        source = self.base / "probe.cpp"
        source.write_text(FIXTURE + "int main() { return LinkList<int>::roundtrip(42) == 42 ? 0 : 1; }\n")
        failed = subprocess.run(["clang++", "-std=c++98", "-pedantic-errors", "-fsyntax-only", str(source)],
                                capture_output=True, text=True)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn("typedef", failed.stderr)
        source.write_text(OVERLAY.rewrite_friend_alias(source.read_text()))
        result = subprocess.run(["clang++", "-std=c++98", "-pedantic-errors", str(source), "-o", str(self.base / "probe")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(self.base / "probe")]).returncode, 0)

    def test_generated_header_stays_outside_checkout_and_manifest_has_only_hashes_paths(self):
        destination = self.base / "overlay.json"
        private_dir = self.base / "generated-private"
        OVERLAY.create_overlay(self.petari, destination, True, private_dir)
        self.assertEqual(self.header.read_text(), FIXTURE)
        self.assertEqual((private_dir / "LinkList.h").read_text(), OVERLAY.rewrite_friend_alias(FIXTURE))
        manifest = json.loads(destination.with_suffix(".manifest.json").read_text())
        self.assertFalse(manifest[-1]["public"])
        self.assertEqual(manifest[-1]["changed_declarations"], 1)
        self.assertNotIn("stored", destination.read_text())
        self.assertNotIn("stored", destination.with_suffix(".manifest.json").read_text())

    def test_unknown_or_ambiguous_header_fails_before_creating_outputs(self):
        self.header.write_text(FIXTURE + "struct Other { friend class Self; };\n")
        destination = self.base / "overlay.json"
        private_dir = self.base / "generated-private"
        with self.assertRaises(ValueError):
            OVERLAY.create_overlay(self.petari, destination, True, private_dir)
        self.assertFalse(destination.exists())
        self.assertFalse(private_dir.exists())

    def test_derived_output_cannot_be_inside_public_repo_or_private_checkout(self):
        for forbidden in [ROOT / "out/derived-friend", self.petari / "derived"]:
            with self.assertRaises(ValueError):
                OVERLAY.create_overlay(self.petari, self.base / "overlay.json", True, forbidden)
        self.assertFalse((self.base / "overlay.json").exists())


if __name__ == "__main__":
    unittest.main()
