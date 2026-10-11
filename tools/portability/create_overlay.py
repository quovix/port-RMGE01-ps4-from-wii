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


def _balanced_body(source, open_index):
    """Dado o índice do '{' de abertura, devolve (corpo, índice_do_'}')
    por balanceamento de chaves. Localiza a edição dentro de um único método,
    sem depender de números de linha."""
    depth = 0
    for i in range(open_index, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[open_index + 1:i], i
    raise ValueError("'{' sem '}' correspondente")


def rewrite_j3dtevs_c_linkage(source):
    """J3DTevs.hpp (JSystem): tira o `extern "C"` das 3 redeclarações dentro de
    `namespace J3DTevsDefault`. Elas têm nome igual a globais `extern const` fora
    do namespace; sob C-linkage o nome não-mangled colide com o global C++ e o
    clang recusa ("declaration with C language linkage conflicts"). Viram externs
    C++ namespaced (os usos `J3DTevsDefault::...` continuam resolvendo). Edição de
    especificador de linkage: nenhum corpo de função é tocado."""
    block = re.compile(r"(namespace\s+J3DTevsDefault\s*\{)(.*?)(\})", re.DOTALL)
    found = block.findall(source)
    if len(found) != 1:
        raise ValueError("J3DTevs mudou: esperado exatamente um namespace J3DTevsDefault")
    head, body, tail = found[0]
    if len(re.findall(r'\bextern\s+"C"\s', body)) != 3:
        raise ValueError('J3DTevs mudou: esperadas 3 decls extern "C" no namespace')
    new_body = re.sub(r'\bextern\s+"C"\s+', "extern ", body)
    return block.sub(lambda m: head + new_body + tail, source, count=1)


def rewrite_tmatrix_this_get(source):
    """TMatrix.hpp (JSystem): prefixa `this->` nas chamadas não-qualificadas a
    `get(...)` dentro de `TRotation3<T>::mult33(const TVec3f&, TVec3f&)`. `get` é
    membro herdado da base dependente `T`; sob two-phase lookup o clang não a
    procura na base dependente para um nome não-qualificado. O `this->` força o
    lookup na instanciação. Edição restrita ao corpo desse método."""
    sig = re.compile(r"mult33\s*\(\s*const\s+TVec3f&\s+rSrc\s*,\s*TVec3f&\s+rDst\s*\)\s*const\s*\{")
    hits = list(sig.finditer(source))
    if len(hits) != 1:
        raise ValueError("TMatrix mudou: esperada uma mult33(const TVec3f&, TVec3f&)")
    open_index = hits[0].end() - 1
    body, end = _balanced_body(source, open_index)
    if len(re.findall(r"(?<![\w.>])get\s*\(", body)) != 9:
        raise ValueError("TMatrix mudou: esperadas 9 chamadas get() não-qualificadas em mult33")
    new_body = re.sub(r"(?<![\w.>])get\s*\(", "this->get(", body)
    return source[:open_index + 1] + new_body + source[end:]


def rewrite_tvec_this_template_set(source):
    """TVec.hpp (JSystem): na chamada não-qualificada a `set< f32 >(...)` dentro de
    `setRotate`, prefixa `this->template`. `set` é membro-template herdado de base
    dependente: além do `this->` do two-phase lookup, a palavra-chave `template`
    desambigua o `<` de um operador menor-que ("use 'template' keyword to treat
    'set' as a dependent template name"). Edição restrita ao corpo de setRotate;
    as demais chamadas a `set<>` já são qualificadas ou de tipo concreto."""
    sig = re.compile(r"setRotate\s*\(\s*const\s+TVec3<\s*f32\s*>&\s+rA\s*,\s*const\s+TVec3<\s*f32\s*>&\s+rB\s*,\s*f32\s+ratio\s*\)\s*\{")
    hits = list(sig.finditer(source))
    if len(hits) != 1:
        raise ValueError("TVec mudou: esperada uma setRotate(const TVec3<f32>&, const TVec3<f32>&, f32)")
    open_index = hits[0].end() - 1
    body, end = _balanced_body(source, open_index)
    if len(re.findall(r"(?<![\w.>:])set\s*<\s*f32\s*>\s*\(", body)) != 1:
        raise ValueError("TVec mudou: esperada 1 chamada set<f32>() não-qualificada em setRotate")
    new_body = re.sub(r"(?<![\w.>:])set(\s*<\s*f32\s*>\s*\()", r"this->template set\1", body)
    return source[:open_index + 1] + new_body + source[end:]


def rewrite_runtime_asm(source):
    """runtime.h (Runtime do CodeWarrior): declara helpers intrínsecos de PPC como
    `asm void NAME(void);` — protótipo-em-asm do MWCC que o clang recusa (top-level
    asm do clang exige `asm("...")`). A recipe remove só a keyword `asm` das 7
    declarações, virando funções externas comuns sem corpo (o codegen x86_64 do
    clang nem emite esses helpers de 64 bits). Remove também os 2 typedefs locais
    `u32`/`u64` (`unsigned long`, 64 bits no alvo LP64): os TUs que incluem este
    header já puxam `revolution/types.h` antes, então o tipo canônico vale e não há
    um 2º sítio de declaração de largura. Nenhum corpo de função é copiado."""
    if len(re.findall(r"\basm\s+void\s+\w+\s*\(\s*void\s*\)\s*;", source)) != 7:
        raise ValueError("runtime.h mudou: esperadas 7 decls `asm void NAME(void);`")
    out = re.sub(r"\basm\s+(void\s+\w+\s*\(\s*void\s*\)\s*;)", r"\1", source)
    typedef = r"^[ \t]*typedef\s+unsigned\s+long(?:\s+long)?\s+u(?:32|64)\s*;[ \t]*\n"
    if len(re.findall(typedef, out, re.MULTILINE)) != 2:
        raise ValueError("runtime.h mudou: esperados 2 typedefs locais u32/u64")
    return re.sub(typedef, "", out, flags=re.MULTILINE)


def rewrite_cw_float_literals(source):
    """MainLoopFramework.cpp (boot): normaliza os literais float estilo CodeWarrior
    `0f`/`1f` (um inteiro seguido de `f` sem ponto) para a forma que o clang aceita,
    `0.0f`/`1.0f` — regra geral `<dígitos>f` → `<dígitos>.0f`. É um pp-number
    inválido no clang, inalcançável por header/define; por isso o TU precisa de um
    derivado da própria fonte. A regex exige que o inteiro seja token completo
    (lookbehind/lookahead) para não tocar hex (`0x1f`), sufixos de `.5f`/`1e3f` nem
    identificadores. Só reescreve literais; nenhuma lógica do source muda."""
    pat = re.compile(r"(?<![\w.])(\d+)f(?![\w.])")
    if not pat.search(source):
        raise ValueError("MainLoopFramework mudou: nenhum literal float CodeWarrior <int>f encontrado")
    return pat.sub(r"\1.0f", source)


# Derivados privados gerados por recipe mecânica (fora do repo e do Petari).
# Cada tupla: (caminho no Petari, nome do derivado, recipe, rótulo da transformação).
# `friend` é a recipe estilo doc-08 já existente; os demais destravam a cadeia de
# boot (ver out/boot-recipe/RELATORIO.md). Só a recipe é publicável; o derivado não.
FRIEND_DERIVED = ("libs/nw4r/include/nw4r/ut/LinkList.h", "LinkList.h",
                  rewrite_friend_alias, "friend_alias_to_injected_class_name")
BOOT_DERIVED = [
    ("libs/JSystem/include/JSystem/J3DGraphBase/J3DTevs.hpp", "J3DTevs.hpp",
     rewrite_j3dtevs_c_linkage, "j3dtevs_drop_c_linkage_in_namespace"),
    ("libs/JSystem/include/JSystem/JGeometry/TMatrix.hpp", "TMatrix.hpp",
     rewrite_tmatrix_this_get, "tmatrix_this_get_in_mult33"),
    ("libs/JSystem/include/JSystem/JGeometry/TVec.hpp", "TVec.hpp",
     rewrite_tvec_this_template_set, "tvec_this_template_set_in_setRotate"),
    ("libs/Runtime/include/runtime.h", "runtime.h",
     rewrite_runtime_asm, "runtime_drop_asm_prototype_and_local_width_typedefs"),
    ("src/Game/System/MainLoopFramework.cpp", "MainLoopFramework.cpp",
     rewrite_cw_float_literals, "codewarrior_float_literals_to_valid"),
]


def create_overlay(petari, destination, patch_friend=False, private_dir=None, boot_unlock=False):
    repository = Path(__file__).resolve().parents[2]
    pairs = [
        ("libs/RVL_SDK/include/revolution/os/OSFastCast.h", "runtime/compat/overlay/OSFastCast.h", True),
        ("libs/nw4r/include/nw4r/math/arithmetic.h", "runtime/compat/overlay/arithmetic.h", True),
        ("libs/RVL_SDK/include/quovix/scalar_math.h", "runtime/compat/scalar_math.h", False),
    ]
    if boot_unlock:
        # size_t e entry_shim já são adapters próprios aprovados em repo; o size_t
        # entra como overlay (entry_shim é preinclude, fora daqui).
        pairs.append(("libs/MSL_C/include/size_t.h", "runtime/compat/overlay/size_t.h", True))
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
    # Derivados privados a gerar nesta execução.
    recipes = []
    if patch_friend or boot_unlock:
        recipes.append(FRIEND_DERIVED)
    if boot_unlock:
        recipes.extend(BOOT_DERIVED)
    writes = []
    if recipes:
        if private_dir is None:
            raise ValueError("patch privado exige --private-dir fora do repo e do Petari")
        private_dir = private_dir.resolve()
        if private_dir.is_relative_to(repository) or private_dir.is_relative_to(petari.resolve()):
            raise ValueError("derivado privado deve ficar fora do repo público e do checkout Petari")
        if private_dir.exists():
            raise ValueError("diretório privado já existe; escolha outro destino")
        for source_rel, out_name, recipe, label in recipes:
            source_file = (petari / source_rel).resolve()
            if not source_file.is_file():
                raise ValueError("source do decomp não encontrado: " + source_rel)
            raw = source_file.read_bytes()
            # Decodificar sem normalizar newline: só o trecho aprovado muda.
            rewritten = recipe(raw.decode("utf-8")).encode("utf-8")
            generated = private_dir / out_name
            writes.append((generated, rewritten))
            entries.append({"type": "file", "name": str(source_file),
                            "external-contents": str(generated)})
            record = {"private_header": str(source_file),
                      "private_sha256": hashlib.sha256(raw).hexdigest(),
                      "generated_private_header": str(generated),
                      "generated_private_sha256": hashlib.sha256(rewritten).hexdigest(),
                      "transformation": label, "public": False}
            if label == "friend_alias_to_injected_class_name":
                record["changed_declarations"] = 1
            manifest.append(record)
    output = {"version": 0, "case-sensitive": True, "use-external-names": True, "roots": entries}
    destination.parent.mkdir(parents=True, exist_ok=True)
    if writes:
        private_dir.mkdir(parents=True)
        for generated, rewritten in writes:
            generated.write_bytes(rewritten)
    destination.write_text(json.dumps(output, indent=2) + "\n")
    destination.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("petari", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--linklist-friend", action="store_true",
                        help="gera localmente o patch de declaração friend do nw4r")
    parser.add_argument("--boot-unlock", action="store_true",
                        help="gera os derivados privados da cadeia de boot "
                             "(friend + J3DTevs/TMatrix/TVec/runtime/MainLoopFramework) e o overlay completo")
    parser.add_argument("--private-dir", type=Path,
                        help="destino novo do header derivado, fora do repo e do Petari")
    args = parser.parse_args()
    try:
        create_overlay(args.petari.resolve(), args.out.resolve(), args.linklist_friend,
                       args.private_dir, args.boot_unlock)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print("Overlay próprio: " + str(args.out.resolve()))


if __name__ == "__main__":
    main()
