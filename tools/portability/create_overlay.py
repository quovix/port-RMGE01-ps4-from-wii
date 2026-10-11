#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# @orig none — gerador original de VFS overlay (quovix), Apache-2.0
"""Mapeia headers privados para adapters próprios, sem copiar/editar o decomp."""

import argparse
import hashlib
import json
from pathlib import Path
import re


def rewrite_friend_alias(source):
    """Receita estreita: usa o nome injetado da classe em vez de seu typedef.

    O input/output são privados. Recusa layouts diferentes do header estudado
    para não transformar nomes que poderiam designar outra classe.
    """
    alias = re.findall(r"\btypedef\s+LinkList\s+Self\s*;", source)
    owner = re.findall(r"\bclass\s+LinkList\s*:\s*private\b", source)
    target = re.compile(r"\bfriend\s+class\s+Self\s*;")
    if len(alias) != 1 or len(owner) != 1 or len(target.findall(source)) != 1:
        raise ValueError("LinkList mudou: esperado um owner, um typedef e um friend correspondente")
    return target.sub("friend class LinkList;", source, count=1)


def create_overlay(petari, destination, patch_friend=False, private_dir=None):
    repository = Path(__file__).resolve().parents[2]
    pairs = [
        ("libs/RVL_SDK/include/revolution/os/OSFastCast.h", "runtime/compat/overlay/OSFastCast.h", True),
        ("libs/nw4r/include/nw4r/math/arithmetic.h", "runtime/compat/overlay/arithmetic.h", True),
        ("libs/RVL_SDK/include/quovix/scalar_math.h", "runtime/compat/scalar_math.h", False),
    ]
    if destination.exists() or destination.with_suffix(".manifest.json").exists():
        raise ValueError("overlay já existe; escolha outro destino")
    entries, manifest = [], []
    for private_name, original_name, required in pairs:
        private_header = (petari / private_name).resolve()
        own_header = (repository / original_name).resolve()
        if (required and not private_header.is_file()) or not own_header.is_file():
            raise ValueError("header não encontrado: " + private_name)
        if not required and private_header.exists():
            raise ValueError("alias virtual já existe no decomp: " + private_name)
        entries.append({"type": "file", "name": str(private_header),
                        "external-contents": str(own_header)})
        manifest.append({"private_header": str(private_header),
                         "private_sha256": hashlib.sha256(private_header.read_bytes()).hexdigest() if required else None,
                         "own_header": str(own_header),
                         "own_sha256": hashlib.sha256(own_header.read_bytes()).hexdigest()})
    generated = None
    if patch_friend:
        if private_dir is None:
            raise ValueError("patch privado exige --private-dir fora do repo e do Petari")
        private_dir = private_dir.resolve()
        if private_dir.is_relative_to(repository) or private_dir.is_relative_to(petari.resolve()):
            raise ValueError("header derivado deve ficar fora do repo público e do checkout Petari")
        if private_dir.exists():
            raise ValueError("diretório privado já existe; escolha outro destino")
        header = (petari / "libs/nw4r/include/nw4r/ut/LinkList.h").resolve()
        raw = header.read_bytes()
        # Decodificar sem normalizar newline: apenas o trecho aprovado muda.
        rewritten = rewrite_friend_alias(raw.decode("utf-8")).encode("utf-8")
        generated = private_dir / "LinkList.h"
        entries.append({"type": "file", "name": str(header), "external-contents": str(generated)})
        manifest.append({"private_header": str(header),
                         "private_sha256": hashlib.sha256(raw).hexdigest(),
                         "generated_private_header": str(generated),
                         "generated_private_sha256": hashlib.sha256(rewritten).hexdigest(),
                         "transformation": "friend_alias_to_injected_class_name",
                         "changed_declarations": 1, "public": False})
    output = {"version": 0, "case-sensitive": True, "use-external-names": True, "roots": entries}
    destination.parent.mkdir(parents=True, exist_ok=True)
    if generated is not None:
        generated.parent.mkdir(parents=True)
        generated.write_bytes(rewritten)
    destination.write_text(json.dumps(output, indent=2) + "\n")
    destination.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("petari", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--linklist-friend", action="store_true",
                        help="gera localmente o patch de declaração friend do nw4r")
    parser.add_argument("--private-dir", type=Path,
                        help="destino novo do header derivado, fora do repo e do Petari")
    args = parser.parse_args()
    try:
        create_overlay(args.petari.resolve(), args.out.resolve(), args.linklist_friend, args.private_dir)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print("Overlay próprio: " + str(args.out.resolve()))


if __name__ == "__main__":
    main()
