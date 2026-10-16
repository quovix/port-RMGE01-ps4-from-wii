#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# @orig none — auditoria original (quovix), Apache-2.0
"""Audita o conteúdo publicável: código próprio, docs e arte própria aprovada."""

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


CODE = {".c", ".h", ".cpp", ".hpp", ".cc", ".py", ".sh"}
PRIVATE_DIRS = {"out", "build", "dumps", "diffs", "orig", "generated", "vendor",
                "petari", "smg-decomp", "rmge01-fs", "sce_module", "__pycache__"}
APPROVED_ART = {
    "package/sce_sys/icon0.png": {
        "3be9488175ca07f88b0577f74a7214f70b99bcd2453ac714e7c3a89ce95d6b94",
        "ca038da7d64cd32235c2351c52b478e43ac2efa60b468c3b02896ed8d6ade651",
    },
    "package/sce_sys/splash.png": {
        "ef3acd3d4b32fff50ccf3bf8f06b34847b5b133019bb770e514970c0d20b5a81",
    },
}
ORIGIN = re.compile(r"^\s*(?:/\*|//|#|\*)\s*@orig\s+([^\r\n*]+)", re.MULTILINE)
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|\d+|[^\s]')
WINDOW = 96


def fingerprints(text):
    """Heurística de cópia literal: janelas de 96 tokens, sem comentários."""
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.DOTALL)
    tokens = TOKENS.findall(text)
    return {hashlib.sha256("\0".join(tokens[i:i + WINDOW]).encode()).digest()
            for i in range(len(tokens) - WINDOW + 1)}


def private_fingerprints(root):
    if not root.is_dir():
        raise ValueError("decomp privado não encontrado: " + str(root))
    result = set()
    for directory in (root / "src", root / "include", root / "libs"):
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in CODE:
                result.update(fingerprints(path.read_text(errors="replace")))
    if not result:
        raise ValueError("nenhum source privado encontrado para comparação")
    return result


def validate(path, data, mode="100644", private=None):
    name = PurePosixPath(path)
    lower_parts = {part.lower() for part in name.parts}
    if mode not in {"100644", "100755"}:
        return "symlink, submodule ou modo Git não permitido"
    if lower_parts & PRIVATE_DIRS or "src/game/" in path.lower():
        return "diretório reservado a conteúdo privado/derivado"
    if path in APPROVED_ART:
        if hashlib.sha256(data).hexdigest() not in APPROVED_ART[path]:
            return "arte binária não consta dos hashes revisados"
        return None
    if len(data) > 1024 * 1024 or b"\0" in data:
        return "binário ou arquivo acima do limite de revisão textual"
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return "conteúdo não textual UTF-8"
    if text.startswith("version https://git-lfs.github.com/spec/v1"):
        return "pointer LFS: payload não auditado"
    root_text = path in {"AGENTS.md", "README.md", "ARCHITECTURE.md", "LICENSE", ".gitignore"}
    documentation = name.suffix == ".md" and name.parts[0] in {"docs", "research", "references", "tools"}
    source = name.suffix in CODE and (
        name.parts[0] in {"tools", "product", "runtime"} or path.startswith("package/src/"))
    build = path in {"package/Makefile", "package/link_engine.x"}
    hook = path in {".githooks/pre-commit", ".githooks/pre-push"}
    workflow = path.startswith(".github/workflows/") and name.suffix in {".yml", ".yaml"}
    if not any((root_text, documentation, source, build, hook, workflow)):
        return "path/tipo fora da lista de conteúdo público permitido"
    if source or build or hook or workflow:
        origins = ORIGIN.findall(text)
        if not origins or not ORIGIN.search("\n".join(text.splitlines()[:20])):
            return "código sem cabeçalho de proveniência @orig none"
        if any(not re.match(r"none(?:\s|$)", origin) for origin in origins):
            return "código derivado: apenas @orig none é publicável"
    if private and fingerprints(text) & private:
        return "trecho coincide com source privado (96 tokens); revisar origem"
    return None


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.PIPE)


def index_entries(root):
    for row in git(root, "ls-files", "--stage", "-z").split(b"\0"):
        if row:
            metadata, path = row.split(b"\t", 1)
            mode, oid, stage = metadata.decode().split()
            if stage != "0":
                raise ValueError("índice contém conflito não resolvido")
            yield path.decode(), mode, oid


def audit(root, scope, private=None, revisions=()):
    failures, seen, blobs = [], set(), {}
    counts = {"workspace": 0, "index": 0, "history": 0, "commits": 0}

    def inspect(label, path, mode, data, identity):
        key = (path, mode, identity)
        counts[label.split(":", 1)[0]] += 1
        if key in seen:
            return
        seen.add(key)
        error = validate(path, data, mode, private)
        if error:
            failures.append(f"{label}: {path}: {error}")

    def blob(oid, mode):
        if mode == "160000":
            return b""
        if oid not in blobs:
            blobs[oid] = git(root, "cat-file", "blob", oid)
        return blobs[oid]

    if scope in {"all", "workspace"}:
        paths = git(root, "ls-files", "--cached", "--others", "--exclude-standard", "-z")
        for path in sorted(set(paths.decode().split("\0")) - {""}):
            local = root / path
            if local.is_symlink():
                inspect("workspace", path, "120000", b"", "symlink")
            elif local.is_file():
                data = local.read_bytes()
                inspect("workspace", path, "100644", data, hashlib.sha256(data).digest())
            elif local.is_dir():
                inspect("workspace", path, "160000", b"", "directory")
    if scope in {"all", "index"}:
        for path, mode, oid in index_entries(root):
            inspect("index", path, mode, blob(oid, mode), oid)
    if scope in {"all", "history"}:
        if git(root, "rev-parse", "--is-shallow-repository").strip() == b"true":
            raise ValueError("histórico incompleto (shallow); auditoria exige fetch-depth 0")
        commits = git(root, "rev-list", "--branches", "--tags", "--remotes", "HEAD", *revisions).decode().splitlines()
        counts["commits"] = len(commits)
        for commit in commits:
            for row in git(root, "ls-tree", "-r", "-z", commit).split(b"\0"):
                if row:
                    metadata, path = row.split(b"\t", 1)
                    mode, _, oid = metadata.decode().split()
                    inspect("history:" + commit[:12], path.decode(), mode, blob(oid, mode), oid)
    return failures, counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--scope", choices=("all", "workspace", "index", "history"), default="all")
    parser.add_argument("--private-source", type=Path, help="comparação adicional com Petari LOCAL")
    parser.add_argument("--push-input", action="store_true", help="lê os OIDs propostos pelo hook pre-push")
    args = parser.parse_args()
    private_path = args.private_source or os.environ.get("QVX_PRIVATE_SOURCE")
    sibling = args.root.resolve().parent / "smg-decomp" / "Petari"
    if not private_path and sibling.is_dir():
        private_path = sibling
    revisions = []
    try:
        if args.push_input:
            for line in sys.stdin:
                fields = line.split()
                if len(fields) != 4 or not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", fields[1]):
                    raise ValueError("entrada pre-push inválida")
                if set(fields[1]) != {"0"}:
                    revisions.append(fields[1])
        private = private_fingerprints(Path(private_path)) if private_path else None
        failures, counts = audit(args.root.resolve(), args.scope, private, revisions)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print("AUDITORIA NÃO CONCLUÍDA: " + str(error), file=sys.stderr)
        return 2
    if failures:
        print("PUBLICAÇÃO BLOQUEADA:\n" + "\n".join(failures), file=sys.stderr)
        return 1
    print(f"Auditoria OK: {counts['workspace']} arquivos no workspace, "
          f"{counts['index']} no índice e {counts['commits']} commits públicos locais. "
          + ("Comparação com source privado executada." if private else "Sem comparação com source privado."))
    return 0


if __name__ == "__main__":
    sys.exit(main())
