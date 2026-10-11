# SPDX-License-Identifier: Apache-2.0
# @orig none — testes originais (quovix), Apache-2.0
"""Integração: compile DB com espaços, isolamento de saídas e falha real."""

import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("measure.py")


@unittest.skipUnless(shutil.which("clang++"), "requer clang++")
class MeasurementTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qvx portability ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "src/Game").mkdir(parents=True)
        (self.root / "includes com espaços").mkdir()
        (self.root / "includes com espaços/config.h").write_text("#define EXPECTED 42\n")
        (self.root / "src/Game/probe.cpp").write_text(
            '#include "config.h"\n'
            'typedef char check[(VALUE == EXPECTED) ? 1 : -1];\n'
            'int probe() { return VALUE; }\n')
        self.original = self.root / "original.o"
        self.original.write_bytes(b"nao sobrescrever")
        self.depfile = self.root / "original.d"
        self.depfile.write_bytes(b"nao sobrescrever dependencias")
        self.arguments = ["clang++", "--target=powerpc-eabi", "-I", "includes com espaços",
                          "-DVALUE=42", "-std=c++98", "-MMD", "-MF", str(self.depfile),
                          "-c", "src/Game/probe.cpp", "-o", str(self.original)]

    def run_probe(self, command_field=False, extra=()):
        entry = {"directory": str(self.root), "file": "src/Game/probe.cpp"}
        if command_field:
            entry["command"] = shlex.join(self.arguments)
        else:
            entry["arguments"] = self.arguments
        database = self.root / "compile_commands.json"
        database.write_text(json.dumps([entry]))
        output = self.root / "report"
        process = subprocess.run(
            [sys.executable, str(SCRIPT), str(database), "--out", str(output), *extra],
            capture_output=True, text=True)
        self.assertEqual(self.original.read_bytes(), b"nao sobrescrever")
        self.assertEqual(self.depfile.read_bytes(), b"nao sobrescrever dependencias")
        return process, output

    def test_argv_preserves_includes_and_defines(self):
        process, output = self.run_probe()
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(report["passed"], 1)
        self.assertFalse((output / "objects").exists())

    def test_command_emits_object_away_from_original(self):
        process, output = self.run_probe(command_field=True, extra=("--mode", "object"))
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertGreater((output / "objects/0.o").stat().st_size, 0)
        # A ferramenta deve preservar a medição, inclusive numa segunda execução.
        retry = subprocess.run([sys.executable, str(SCRIPT), str(self.root / "compile_commands.json"),
                                "--out", str(output)], capture_output=True, text=True)
        self.assertEqual(retry.returncode, 2)
        self.assertIn("destino já contém", retry.stderr)

    def test_missing_include_is_reported_as_failure(self):
        (self.root / "src/Game/probe.cpp").write_text('#include "ausente.h"\n')
        process, output = self.run_probe()
        self.assertEqual(process.returncode, 1)
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(report["passed"], 0)
        self.assertEqual(report["missing_include_tus"], 1)

    def test_response_file_is_rejected_before_execution(self):
        self.arguments.append("@opcoes.rsp")
        process, output = self.run_probe()
        self.assertEqual(process.returncode, 2)
        self.assertIn("response files não suportados", process.stderr)
        self.assertFalse(output.exists())

    def test_preinclude_is_applied_before_translation_unit(self):
        self.arguments.remove("-DVALUE=42")
        header = self.root / "compat original.h"
        header.write_text("#define VALUE 42\n")
        process, output = self.run_probe(extra=("--preinclude", str(header)))
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(report["passed"], 1)
        self.assertEqual(report["preincludes"][0]["path"], str(header.resolve()))

    def test_vfs_overlay_replaces_private_header_without_modifying_it(self):
        private_header = self.root / "includes com espaços/config.h"
        private_header.write_text('#error "fixture incompatível"\n')
        replacement = self.root / "header próprio.h"
        replacement.write_text("#define EXPECTED 42\n")
        overlay = self.root / "overlay.json"
        overlay.write_text(json.dumps({"version": 0, "roots": [
            {"type": "file", "name": str(private_header.resolve()),
             "external-contents": str(replacement.resolve())}]}))
        process, output = self.run_probe(extra=("--vfs-overlay", str(overlay)))
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(private_header.read_text(), '#error "fixture incompatível"\n')
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(report["overlays"][0]["path"], str(overlay.resolve()))


if __name__ == "__main__":
    unittest.main()
