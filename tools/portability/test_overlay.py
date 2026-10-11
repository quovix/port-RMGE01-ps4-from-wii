# SPDX-License-Identifier: Apache-2.0
# @orig none — fixtures/testes VFS originais (quovix), Apache-2.0
"""Compila os adapters de verdade contra headers sintéticos, sem jogo."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("create_overlay", Path(__file__).with_name("create_overlay.py"))
OVERLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OVERLAY)


@unittest.skipUnless(shutil.which("clang") and shutil.which("clang++"), "requer clang/clang++")
class OverlayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qvx math overlay ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.rvl = self.root / "libs/RVL_SDK/include"
        self.nw4r = self.root / "libs/nw4r/include"
        self.originals = [self.rvl / "revolution/os/OSFastCast.h", self.nw4r / "nw4r/math/arithmetic.h"]
        for header in self.originals:
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#error "fixture privado precisa de adapter"\n')
        (self.rvl / "revolution/types.h").write_text(
            "#ifndef QVX_FIXTURE_TYPES\n#define QVX_FIXTURE_TYPES\n"
            "typedef float f32; typedef unsigned short u16;\n#endif\n")
        self.overlay = self.root / "overlay.json"
        OVERLAY.create_overlay(self.root, self.overlay)

    def run_command(self, args):
        process = subprocess.run(args, capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)

    def test_cpp_adapters_compile_link_and_run_without_touching_private_headers(self):
        fixture = self.root / "fixture.cpp"
        fixture.write_text(
            '#include <nw4r/math/arithmetic.h>\n'
            '#include <revolution/os/OSFastCast.h>\n'
            'int main() { u16 input = 65535; volatile f32 output = 0;\n'
            'OSInitFastCast(); OSu16tof32(&input, &output);\n'
            'if (output != 65535.0f || nw4r::math::F32ToU16(-1.0f) != 0) return 1;\n'
            'if (nw4r::math::FAbs(-3.0f) != 3.0f) return 2;\n'
            'return nw4r::math::FSelect(-0.0f, 11.0f, 22.0f) == 11.0f ? 0 : 3; }\n')
        core = self.root / "scalar.o"
        self.run_command(["clang", "-std=c11", "-O2", "-c", str(ROOT / "runtime/compat/scalar_math.c"), "-o", str(core)])
        executable = self.root / "fixture"
        self.run_command(["clang++", "-std=c++98", "-nostdinc", "-I" + str(self.rvl), "-I" + str(self.nw4r),
                          "-include", str(ROOT / "runtime/compat/math_profile.h"),
                          "-ivfsoverlay", str(self.overlay), str(fixture), str(core), "-o", str(executable)])
        self.run_command([str(executable)])
        for header in self.originals:
            self.assertEqual(header.read_text(), '#error "fixture privado precisa de adapter"\n')
        self.assertFalse((self.rvl / "quovix").exists())

    def test_c_fastcast_supports_volatile_output(self):
        fixture = self.root / "fixture.c"
        fixture.write_text(
            '#include <revolution/os/OSFastCast.h>\n'
            'int main(void) { const float input = 23.75f; volatile u16 output = 0;\n'
            'OSf32tou16(&input, &output); return output == 23 ? 0 : 1; }\n')
        executable = self.root / "fixture-c"
        self.run_command(["clang", "-std=c11", "-nostdinc", "-I" + str(self.rvl),
                          "-ivfsoverlay", str(self.overlay), str(fixture),
                          str(ROOT / "runtime/compat/scalar_math.c"), "-o", str(executable)])
        self.run_command([str(executable)])

    def test_overlay_contains_only_paths_and_hashes_and_preserves_existing_evidence(self):
        mapping = json.loads(self.overlay.read_text())
        self.assertEqual(len(mapping["roots"]), 3)
        self.assertNotIn("fixture privado", self.overlay.read_text())
        manifest = json.loads(self.overlay.with_suffix(".manifest.json").read_text())
        self.assertEqual(len(manifest), 3)
        with self.assertRaises(ValueError):
            OVERLAY.create_overlay(self.root, self.overlay)


if __name__ == "__main__":
    unittest.main()
