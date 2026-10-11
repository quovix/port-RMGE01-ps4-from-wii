#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# @orig none — ferramenta original (quovix), Apache-2.0
"""Mede o Petari local com clang, sem modificar o decomp ou seus objetos."""

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time


def source_path(entry):
    return (Path(entry["directory"]) / entry["file"]).resolve()


def adapt_arguments(entry, compiler, target, mode, output, delayed, preincludes=(), overlays=(), extra=()):
    """Preserva includes/defines; substitui target, ação e saída do compile DB.

    Só aceita argv de clang: comandos MWCC precisam de outra tradução. Nunca
    executa o campo command em shell, nem reaproveita destinos do build local.
    """
    arguments = entry.get("arguments")
    if arguments is None:
        arguments = shlex.split(entry["command"])
    if not arguments or not Path(arguments[0]).name.startswith("clang"):
        raise ValueError("compile DB deve conter comandos clang, sem wrappers")
    result = [compiler]
    skip_next = False
    paired = {"-o", "-MF", "-MT", "-MQ", "-MJ", "-target", "--target"}
    actions = {"-c", "-S", "-E", "-fsyntax-only", "-MD", "-MMD", "-MP",
               "-M", "-MM", "-emit-llvm", "-fdelayed-template-parsing"}
    for arg in arguments[1:]:
        if skip_next:
            skip_next = False
            continue
        if arg in paired:
            skip_next = True
        elif arg in actions or arg.startswith("--target="):
            continue
        elif arg.startswith(("@", "-o", "-MF", "-MT", "-MQ", "-MJ")):
            # Response files podem esconder -o/targets: não executá-los.
            if arg.startswith("@"):
                raise ValueError("response files não suportados; expanda o compile DB")
            continue
        elif arg.startswith(("-save-temps", "-fmodules", "-fmodule", "-serialize-diagnostics")):
            raise ValueError("flag com saída adicional não suportada: " + arg)
        else:
            result.append(arg)
    if skip_next:
        raise ValueError("flag sem argumento no compile DB")
    if target:
        result.append("--target=" + target)
    if delayed:
        result.append("-fdelayed-template-parsing")
    for header in preincludes:
        result += ["-include", str(header)]
    for overlay in overlays:
        result += ["-ivfsoverlay", str(overlay)]
    result += ["-fno-color-diagnostics", "-fno-caret-diagnostics",
               "-Wno-unknown-pragmas", "-ferror-limit=20"]
    result += list(extra)
    if mode == "syntax":
        result.append("-fsyntax-only")
    else:
        result += ["-c", "-o", str(output)]
    return result


def measure(entry, command, timeout):
    started = time.monotonic()
    try:
        process = subprocess.run(command, cwd=entry["directory"],
                                 capture_output=True, text=True, errors="replace",
                                 timeout=timeout)
        code, diagnostics = process.returncode, process.stderr + process.stdout
    except subprocess.TimeoutExpired:
        code, diagnostics = None, "timeout por TU\n"
    return {"file": str(source_path(entry)), "command": command,
            "returncode": code, "seconds": round(time.monotonic() - started, 3),
            "diagnostics": diagnostics}


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("deve ser >= 1")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compile_db", type=Path)
    parser.add_argument("--out", type=Path, default=Path("out/phase-a"))
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--target", help="triple clang; omitido = host")
    parser.add_argument("--mode", choices=("syntax", "object"), default="syntax")
    parser.add_argument("--prefix", default="src/Game", help="subárvore no directory de cada TU")
    parser.add_argument("--limit", type=positive)
    parser.add_argument("--jobs", type=positive, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--timeout", type=positive, default=60)
    parser.add_argument("--strict-templates", action="store_true",
                        help="mede sem -fdelayed-template-parsing")
    parser.add_argument("--preinclude", type=Path, action="append", default=[],
                        help="header próprio de compatibilidade, antes de cada TU")
    parser.add_argument("--vfs-overlay", type=Path, action="append", default=[],
                        help="mapeamento clang de headers para adapters próprios")
    parser.add_argument("--fpic", action="store_true",
                        help="injeta -fPIC em cada TU (necessário para eboot PIE)")
    parser.add_argument("--extra-arg", action="append", default=[],
                        help="flag extra de clang por TU; repetível (ex. ataque a ODR)")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("compilador não encontrado: " + args.compiler)
    try:
        raw = args.compile_db.read_bytes()
        entries = json.loads(raw)
        selected = [entry for entry in entries if source_path(entry).is_relative_to(
            (Path(entry["directory"]) / args.prefix).resolve())]
        selected.sort(key=lambda entry: str(source_path(entry)))
        if args.limit:
            selected = selected[:args.limit]
        if not selected:
            parser.error("nenhum TU selecionado")
        preincludes = [header.resolve() for header in args.preinclude]
        if any(not header.is_file() for header in preincludes):
            parser.error("header de --preinclude não encontrado")
        overlays = [overlay.resolve() for overlay in args.vfs_overlay]
        if any(not overlay.is_file() for overlay in overlays):
            parser.error("arquivo de --vfs-overlay não encontrado")
        output = args.out.resolve()
        # Não sobrescrever evidência de uma execução anterior.
        if (output / "report.json").exists() or (output / "results.jsonl").exists():
            parser.error("destino já contém medição; escolha outro --out")
        extra = (["-fPIC"] if args.fpic else []) + list(args.extra_arg)
        commands = [adapt_arguments(entry, compiler, args.target, args.mode,
                                   output / "objects" / (str(i) + ".o"),
                                   not args.strict_templates, preincludes, overlays,
                                   extra)
                    for i, entry in enumerate(selected)]
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    version = subprocess.run([compiler, "--version"], check=True,
                             capture_output=True, text=True).stdout.strip()
    output.mkdir(parents=True, exist_ok=True)
    if args.mode == "object":
        (output / "objects").mkdir(exist_ok=True)
    results = []
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(measure, entry, command, args.timeout)
                   for entry, command in zip(selected, commands)]
        for future in as_completed(futures):
            results.append(future.result())
            if len(results) % 100 == 0:
                print(f"{len(results)}/{len(selected)} TUs", file=sys.stderr, flush=True)
    results.sort(key=lambda item: item["file"])
    passed = sum(item["returncode"] == 0 for item in results)
    missing = sum("file not found" in item["diagnostics"] for item in results)
    errors = Counter(line.split("error: ", 1)[1]
                     for item in results for line in item["diagnostics"].splitlines()
                     if "error: " in line)
    error_tus = Counter()
    warning_tus = Counter()
    subsystems = {}
    roots = {str(source_path(entry)): (Path(entry["directory"]) / args.prefix).resolve()
             for entry in selected}
    for item in results:
        lines = item["diagnostics"].splitlines()
        error_tus.update({line.split("error: ", 1)[1] for line in lines if "error: " in line})
        warning_tus.update({flag for line in lines for flag in re.findall(r"\[-W[^\]]+\]", line)})
        # Subárvore baseada no prefixo selecionado, independente de paths absolutos.
        relative = Path(item["file"]).relative_to(roots[item["file"]])
        subsystem = relative.parts[0] if len(relative.parts) > 1 else "."
        counts = subsystems.setdefault(subsystem, {"selected": 0, "passed": 0})
        counts["selected"] += 1
        counts["passed"] += item["returncode"] == 0
    report = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "compile_db": str(args.compile_db.resolve()),
        "compile_db_sha256": hashlib.sha256(raw).hexdigest(),
        "compiler": version, "target": args.target or "host",
        "mode": args.mode, "delayed_template_parsing": not args.strict_templates,
        "extra_args": extra,
        "preincludes": [{"path": str(header), "sha256": hashlib.sha256(header.read_bytes()).hexdigest()}
                        for header in preincludes],
        "overlays": [{"path": str(overlay), "sha256": hashlib.sha256(overlay.read_bytes()).hexdigest()}
                     for overlay in overlays],
        "prefix": args.prefix, "database_entries": len(entries),
        "selected": len(selected), "passed": passed,
        "failed": len(selected) - passed,
        "timed_out": sum(item["returncode"] is None for item in results),
        "missing_include_tus": missing,
        "pass_percent": round(100 * passed / len(selected), 2),
        "seconds": round(time.monotonic() - started, 3),
        "common_errors": errors.most_common(20),
        "common_error_tus": error_tus.most_common(20),
        "warning_tus": warning_tus.most_common(),
        "subsystems": subsystems,
    }
    (output / "results.jsonl").write_text(
        "".join(json.dumps(item, ensure_ascii=False) + "\n" for item in results))
    (output / "report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(f"{passed}/{len(selected)} TUs OK ({report['pass_percent']}%); "
          f"{missing} TUs com include ausente; relatório: {output / 'report.json'}")
    return 0 if passed == len(selected) else 1


if __name__ == "__main__":
    sys.exit(main())
