#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# @orig none — ferramenta original (quovix), Apache-2.0
#
# Link-test do motor: dados os objetos de jogo compilados (derivados, PRIVADOS),
# computa a superfície de símbolos externos (plataforma + símbolos de jogo de
# TUs não-compilados), gera stubs weak no-op e tenta linkar num binário.
#
# Prova que os objetos COMPÕEM num executável e enumera a superfície de
# plataforma (o TODO do backend Orbis). Nada derivado é publicado: objetos de
# entrada, stubs gerados e o binário de saída ficam no --out-dir PRIVADO.
#
# Uso:
#   link_engine.sh --objdir <dir com *.o derivados> --out-dir <dir privado>
#                  [--toolchain $OO_PS4_TOOLCHAIN] [--llvm <bin do llvm>]
#
# Requer: llvm-nm, ld.lld, clang (do LLVM "puro"); link.x + lib do OpenOrbis.
set -euo pipefail

OBJDIR="" OUTDIR="" TOOLCHAIN="${OO_PS4_TOOLCHAIN:-}" LLVM="/opt/homebrew/opt/llvm@18/bin"
while [ $# -gt 0 ]; do case "$1" in
  --objdir) OBJDIR="$2"; shift 2;;
  --out-dir) OUTDIR="$2"; shift 2;;
  --toolchain) TOOLCHAIN="$2"; shift 2;;
  --llvm) LLVM="$2"; shift 2;;
  *) echo "arg desconhecido: $1" >&2; exit 2;;
esac; done
[ -n "$OBJDIR" ] && [ -n "$OUTDIR" ] && [ -n "$TOOLCHAIN" ] || { echo "faltam args" >&2; exit 2; }

# guarda de cena limpa: saída precisa ser diretório privado, fora do repo/checkout
case "$OUTDIR" in */galaxy-recomp/out/*|/private/tmp/*|/tmp/*) ;; *)
  echo "recusado: --out-dir deve ser privado (out/ do repo ou /tmp)" >&2; exit 3;; esac
mkdir -p "$OUTDIR"

NM="$LLVM/llvm-nm"; LD="$LLVM/ld.lld"; CC="$LLVM/clang"
mapfile_objs() { find "$OBJDIR" -name '*.o'; }
N=$(mapfile_objs | wc -l | tr -d ' ')
echo "objetos: $N"

# definidos x indefinidos -> externos
mapfile_objs | xargs -I{} "$NM" --defined-only --format=just-symbols {} 2>/dev/null | sort -u > "$OUTDIR/defined.txt"
mapfile_objs | xargs -I{} "$NM" -u --format=just-symbols {} 2>/dev/null | sort -u > "$OUTDIR/undef.txt"
comm -23 "$OUTDIR/undef.txt" "$OUTDIR/defined.txt" > "$OUTDIR/externs.txt"
echo "externos (a stubar): $(wc -l < "$OUTDIR/externs.txt")"

# stubs weak no-op (arquivo PRIVADO: contém nomes derivados do jogo)
{ echo ".text"; awk '{print ".weak " $0 "\n" $0 ":\n  ret"}' "$OUTDIR/externs.txt"; } > "$OUTDIR/stubs.S"
"$CC" --target=x86_64-pc-freebsd12-elf -c "$OUTDIR/stubs.S" -o "$OUTDIR/stubs.o"
echo 'int main(void){return 0;}' > "$OUTDIR/dummymain.c"
"$CC" --target=x86_64-pc-freebsd12-elf -c "$OUTDIR/dummymain.c" -o "$OUTDIR/dummymain.o"

# link (non-PIE para o teste; objetos sem -fPIC ainda). --allow-multiple-definition
# contorna o ODR de inlines/globais do RVL emitidos como símbolo forte.
echo "=== link ==="
"$LD" $(mapfile_objs) "$OUTDIR/stubs.o" "$OUTDIR/dummymain.o" \
  -o "$OUTDIR/engine.elf" -m elf_x86_64 -no-pie --eh-frame-hdr \
  -L"$TOOLCHAIN/lib" -lc -lkernel -lc++ "$TOOLCHAIN/lib/crt1.o" \
  --allow-multiple-definition
echo "OK: $(ls -la "$OUTDIR/engine.elf" | awk '{print $5}') bytes"
"$LLVM/llvm-size" "$OUTDIR/engine.elf" || true
