#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# @orig none — ferramenta original (quovix), Apache-2.0
"""Gera stubs weak no-op INSTRUMENTADOS para a superfície de plataforma.

Dada uma lista de símbolos externos (um por linha), emite um .S x86-64 em que
cada símbolo S vira uma função weak que:
  1) chama qvx_stub_hit("S")  -> registra a chamada no klog (crash trail);
  2) retorna 0 (rax = 0), cobrindo retornos int/ponteiro/bool.

Retornos float/double (xmm0) e símbolos que na verdade são dados (vtables) não
são tratados — aceitável para o salto-sonda, cujo objetivo é só achar a PRIMEIRA
falta real do motor. Um símbolo forte definido em outro objeto (nosso backend)
vence o weak no link, então só o que falta de fato é instrumentado.

A SAÍDA contém nomes de símbolos derivados do jogo -> arquivo PRIVADO (out/).
Esta ferramenta é nossa (@orig none); o .S gerado não vai pro repo público.

Uso: gen_trace_stubs.py --externs externs.txt --out stubs_trace.S
"""
import argparse
import sys
from pathlib import Path

# nomes mangled C++ e símbolos C: [A-Za-z0-9_] — seguros como label e string asm.
_ALLOWED = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")


def _valid(sym: str) -> bool:
    return bool(sym) and all(c in _ALLOWED for c in sym)


# Símbolos que na verdade são DADOS, não código. Emitir uma função no-op aqui
# faria o motor ler bytes de instrução como ponteiro (ex.: entrada de vtable) e
# saltar pra um PC aleatório — mascarando o primeiro-fault real. Em vez disso,
# emitimos um objeto zerado: um virtual-call pela vtable salta pra 0 (null-deref
# limpo e previsível), que é o sinal que a sonda quer.
#   _ZTV = vtable, _ZTI = typeinfo, _ZTS = typeinfo name, _ZTC/_ZTT = construção
_DATA_PREFIXES = ("_ZTV", "_ZTI", "_ZTS", "_ZTC", "_ZTT")
_DATA_FILL = 256   # 32 ponteiros zerados: cobre vtables de init sem estourar

# Símbolos de DADOS que não casam pelos prefixos _ZT* acima: globais C do motor
# (ponteiro de display-list da GX, locale) e membros estáticos de classe
# (_ZN...E<nome>E sem assinatura de função). Instrumentar como função faria o
# motor ler bytes de código como dado -> fault mascarado. Emitimos objeto zerado.
_DATA_EXACT = frozenset({
    "__GDCurrentDL",                     # ponteiro da display-list corrente (GX)
    "_current_locale",                   # locale corrente (libc++/ctype)
    "_ZN10J3DMtxCalc10mMtxBufferE",      # J3DMtxCalc::mMtxBuffer (estático)
    "_ZN10J3DMtxCalc6mJointE",           # J3DMtxCalc::mJoint (estático)
})


def _is_data(sym: str) -> bool:
    return sym.startswith(_DATA_PREFIXES) or sym in _DATA_EXACT


def generate(symbols):
    out = []
    out.append("/* GERADO por gen_trace_stubs.py — PRIVADO (nomes derivados). NAO publicar. */")
    emitted = 0
    data_emitted = 0
    for i, sym in enumerate(symbols):
        if not _valid(sym):
            print(f"pulando simbolo invalido: {sym!r}", file=sys.stderr)
            continue
        if _is_data(sym):
            out.append("  .data")
            out.append(f"  .weak {sym}")
            out.append(f"  .type {sym}, @object")
            out.append(f"{sym}:")
            out.append(f"  .zero {_DATA_FILL}")
            data_emitted += 1
            continue
        out.append("  .section .rodata")
        out.append(f".Lqvxnm_{i}:")
        out.append(f'  .asciz "{sym}"')
        out.append("  .text")
        out.append(f"  .weak {sym}")
        out.append(f"  .type {sym}, @function")
        out.append(f"{sym}:")
        out.append("  subq $8, %rsp")                       # alinha pilha p/ call
        out.append(f"  leaq .Lqvxnm_{i}(%rip), %rdi")       # rdi = nome
        out.append("  xorl %eax, %eax")                     # al=0 (seguro p/ variadic)
        out.append("  call qvx_stub_hit@PLT")
        out.append("  xorl %eax, %eax")                     # retorno 0
        out.append("  addq $8, %rsp")
        out.append("  ret")
        emitted += 1
    return "\n".join(out) + "\n", emitted, data_emitted


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--externs", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    syms = [ln.strip() for ln in args.externs.read_text().splitlines()]
    syms = [s for s in syms if s and not s.startswith("#")]
    asm, n, nd = generate(syms)
    args.out.write_text(asm)
    print(f"stubs instrumentados: {n} funcoes + {nd} dados zerados "
          f"(de {len(syms)} simbolos) -> {args.out}")


if __name__ == "__main__":
    main()
