#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# @orig  none — trabalho original (quovix), Apache-2.0
# @gen   none — ferramenta, não é código derivado do jogo
"""Fase 1 do pipeline: ingest de um dump RVZ (BYO-dump).

Orquestra o DolphinTool (autoridade do formato RVZ) para:
  1. ler o header e confirmar o game ID esperado (RMGE01);
  2. verificar integridade do dump;
  3. extrair o executável PPC (main.dol) e a FST;
  4. computar o sha1 do main.dol -> entra na proveniência (src_sha1).

NÃO redistribui nada do jogo: tudo é escrito no workdir LOCAL do usuário.

Uso:
    python3 -I tools/ingest/ingest.py <dump.rvz> [--workdir DIR] [--expect RMGE01]

Requer DolphinTool (Dolphin.app). Instale no macOS com:
    brew install --cask dolphin-emu

Os flags exatos do DolphinTool são centralizados em DT_* e validados no primeiro
run contra o dump real (a CLI varia um pouco entre versões do Dolphin).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

EXPECTED_GAME_ID = "RMGE01"  # Wii, NTSC-U; origem do port [PORT][RMGE01][PS4<-WII]

# Locais prováveis do binário DolphinTool.
_DT_CANDIDATES = [
    "DolphinTool",
    "dolphin-tool",
    "/Applications/Dolphin.app/Contents/MacOS/DolphinTool",
    str(Path.home() / "Applications/Dolphin.app/Contents/MacOS/DolphinTool"),
]


def find_dolphintool() -> str:
    for c in _DT_CANDIDATES:
        p = shutil.which(c) or (c if Path(c).is_file() else None)
        if p:
            return p
    sys.exit(
        "DolphinTool não encontrado.\n"
        "  macOS:  brew install --cask dolphin-emu\n"
        "  depois: /Applications/Dolphin.app/Contents/MacOS/DolphinTool"
    )


def _run(args: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(args, capture_output=True, text=True)


def read_header(dt: str, rvz: Path) -> dict:
    # DolphinTool header -i <file> -j  → JSON com game_id, internal_name, etc.
    r = _run([dt, "header", "-i", str(rvz), "-j"])
    if r.returncode != 0:
        sys.exit(f"falha ao ler header:\n{r.stderr.strip()}")
    try:
        return json.loads(r.stdout)
    except json.JSONDecodeError:
        sys.exit(f"saída de header não-JSON (versão do DolphinTool?):\n{r.stdout[:400]}")


def verify(dt: str, rvz: Path) -> str:
    r = _run([dt, "verify", "-i", str(rvz)])
    return (r.stdout + r.stderr).strip()


def extract_main_dol(dt: str, rvz: Path, out: Path) -> Path:
    # DolphinTool extract -i <file> ... → extrai arquivos do disco.
    # O caminho do executável no disco de Wii é a seção SYS/main.dol da partição.
    out.mkdir(parents=True, exist_ok=True)
    r = _run([dt, "extract", "-i", str(rvz), "-o", str(out)])
    if r.returncode != 0:
        sys.exit(f"falha ao extrair:\n{r.stderr.strip()}\n"
                 "(confirmar subcomando/flags de extract nesta versão do DolphinTool)")
    hits = list(out.rglob("main.dol"))
    if not hits:
        sys.exit("main.dol não encontrado na extração — revisar layout do extract")
    return hits[0]


def sha1_of(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="ingest de dump RVZ (fase 1)")
    ap.add_argument("rvz", type=Path, help="caminho do dump .rvz")
    ap.add_argument("--workdir", type=Path, default=Path("out/ingest"),
                    help="diretório LOCAL de saída (default: out/ingest)")
    ap.add_argument("--expect", default=EXPECTED_GAME_ID, help="game ID esperado")
    ap.add_argument("--no-verify", action="store_true", help="pular verificação de integridade")
    ns = ap.parse_args(argv)

    if not ns.rvz.is_file():
        sys.exit(f"não achei o dump: {ns.rvz}")
    dt = find_dolphintool()

    hdr = read_header(dt, ns.rvz)
    gid = hdr.get("game_id") or hdr.get("gameId") or "?"
    print(f"game ID : {gid}")
    if gid != ns.expect:
        sys.exit(f"game ID {gid!r} != esperado {ns.expect!r} — dump errado?")

    if not ns.no_verify:
        print("verify  :", verify(dt, ns.rvz) or "(sem saída)")

    dol = extract_main_dol(dt, ns.rvz, ns.workdir)
    digest = sha1_of(dol)
    print(f"main.dol: {dol}  ({dol.stat().st_size} bytes)")
    print(f"sha1    : {digest}")

    report = {
        "game_id": gid,
        "expected": ns.expect,
        "header": hdr,
        "main_dol": {"path": str(dol), "size": dol.stat().st_size, "sha1": digest},
        "src_sha1": digest,  # usado na proveniência (ver docs/PROVENANCE.md)
    }
    rp = ns.workdir / "ingest-report.json"
    rp.write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(f"report  : {rp}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
