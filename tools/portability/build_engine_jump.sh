#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# @orig none — ferramenta original (quovix), Apache-2.0
#
# Build do SALTO CONTROLADO: harness da casca (-fPIC, com QVX_ENGINE_JUMP) +
# os 1468 objetos do motor (derivados PRIVADOS) + stubs weak instrumentados
# para a superfície de plataforma que falta -> eboot PIE -> PKG.
#
# Reusa sce_sys/sce_module/gp4 da casca; só troca o eboot.bin. Saída PRIVADA.
#
# Uso: build_engine_jump.sh [--engine-objs <dir>] [--out <dir privado>]
set -euo pipefail

REPO="/Users/joaobembe/Workspace/galaxy-recomp"
PKG="$REPO/package"
ENGINE_OBJS="$REPO/out/boot-unlock/measure-full2/objects"
OUTDIR="$REPO/out/engine-jump"
TOOLCHAIN="${OO_PS4_TOOLCHAIN:-/Users/joaobembe/opt/openorbis/OpenOrbis/PS4Toolchain}"
LLVM="/opt/homebrew/opt/llvm@18/bin"
while [ $# -gt 0 ]; do case "$1" in
  --engine-objs) ENGINE_OBJS="$2"; shift 2;;
  --out) OUTDIR="$2"; shift 2;;
  *) echo "arg desconhecido: $1" >&2; exit 2;;
esac; done

CC="$LLVM/clang"; CXX="$LLVM/clang++"; LD="$LLVM/ld.lld"; NM="$LLVM/llvm-nm"
OBJ="$OUTDIR/obj"
mkdir -p "$OBJ"

INCLUDES="-I$PKG/../product/media -I$PKG/../runtime/compat -I$PKG/../runtime/video -I$PKG/../product/art -I$PKG/../runtime/platform/orbis"
CFLAGS="--target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $TOOLCHAIN -isystem $TOOLCHAIN/include $INCLUDES -fno-fast-math -ffp-contract=off"
CXXFLAGS="$CFLAGS -std=c++98 -fno-exceptions -fno-rtti"

# --- 1) harness C (backend incluído); main.c ganha -DQVX_ENGINE_JUMP ---------
SRC_C=(
  "$PKG/src/main.c"
  "$REPO/product/media/media_verify.c" "$REPO/product/media/sha1.c"
  "$REPO/runtime/compat/scalar_math.c" "$REPO/runtime/compat/math_smoke.c"
  "$REPO/runtime/video/video_out.c" "$REPO/runtime/video/text.c"
  "$REPO/product/art/art_splash.c" "$REPO/product/art/yaz0.c"
  "$REPO/product/art/rarc.c" "$REPO/product/art/tpl.c"
  "$REPO/runtime/platform/orbis/mtx.c" "$REPO/runtime/platform/orbis/gx_vi.c"
  "$REPO/runtime/platform/orbis/os.c" "$REPO/runtime/platform/orbis/dvd.c"
  "$REPO/runtime/platform/orbis/backend_bringup.c"
  "$REPO/runtime/platform/orbis/stub_trace.c"
)
SRC_CPP=( "$REPO/runtime/compat/heap_new.cpp" "$REPO/runtime/compat/heap_smoke.cpp" )

echo "=== compilando harness (-fPIC) ==="
for s in "${SRC_C[@]}"; do
  o="$OBJ/$(basename "${s%.c}").o"
  extra=""; [ "$(basename "$s")" = "main.c" ] && extra="-DQVX_ENGINE_JUMP"
  $CC $CFLAGS $extra -o "$o" "$s"
done
for s in "${SRC_CPP[@]}"; do
  $CXX $CXXFLAGS -o "$OBJ/$(basename "${s%.cpp}").o" "$s"
done
HARNESS_OBJS=$(find "$OBJ" -name '*.o')

# --- 2) superfície de externos sobre (harness + motor) -----------------------
# Guard: sem objetos de motor, TODOS os símbolos viram stub e o link SUCEDE num
# eboot 100% no-op — falha silenciosa que não dá pra flashar. Aborta ruidoso.
NENG=$(find "$ENGINE_OBJS" -name '*.o' | wc -l | tr -d ' ')
echo "objetos de motor encontrados: $NENG"
if [ "$NENG" -lt 1000 ]; then
  echo "ABORTANDO: so $NENG objetos de motor em $ENGINE_OBJS (esperado ~1468)." >&2
  echo "Sem motor, o eboot seria 100% stub. Confira --engine-objs." >&2
  exit 4
fi
echo "=== calculando externos (harness + motor) ==="
ALL_OBJS=$(printf '%s\n' $HARNESS_OBJS; find "$ENGINE_OBJS" -name '*.o')
echo "$ALL_OBJS" | xargs -I{} "$NM" --defined-only --format=just-symbols {} 2>/dev/null | sort -u > "$OUTDIR/defined.txt"
echo "$ALL_OBJS" | xargs -I{} "$NM" -u --format=just-symbols {} 2>/dev/null | sort -u > "$OUTDIR/undef.txt"
comm -23 "$OUTDIR/undef.txt" "$OUTDIR/defined.txt" > "$OUTDIR/externs.txt"
NEXT=$(wc -l < "$OUTDIR/externs.txt" | tr -d ' ')
echo "externos a instrumentar: $NEXT"
# Sanidade: um salto em externos (ex.: objetos corrompidos lidos como vazios
# por nm 2>/dev/null) indicaria superfície mascarada. ~1556 é o esperado.
if [ "$NEXT" -gt 5000 ]; then
  echo "ABORTANDO: $NEXT externos é alto demais (esperado ~1556)." >&2
  echo "Provável leitura de objetos falhando. Confira os .o." >&2
  exit 5
fi

# --- 3) stubs instrumentados (PRIVADO) ---------------------------------------
python3 "$REPO/tools/portability/gen_trace_stubs.py" \
  --externs "$OUTDIR/externs.txt" --out "$OUTDIR/stubs_trace.S"
$CC --target=x86_64-pc-freebsd12-elf -fPIC -c "$OUTDIR/stubs_trace.S" -o "$OUTDIR/stubs_trace.o"

# --- 4) link PIE -------------------------------------------------------------
echo "=== link PIE (motor + harness + stubs) ==="
set +e
$LD $HARNESS_OBJS $(find "$ENGINE_OBJS" -name '*.o') "$OUTDIR/stubs_trace.o" \
  -o "$OUTDIR/eboot.elf" -m elf_x86_64 -pie --script "$TOOLCHAIN/link.x" --eh-frame-hdr \
  -L"$TOOLCHAIN/lib" -lc -lkernel -lc++ -lSceSystemService -lSceVideoOut \
  "$TOOLCHAIN/lib/crt1.o" --allow-multiple-definition 2> "$OUTDIR/link.err"
RC=$?
set -e
if [ $RC -ne 0 ]; then echo "LINK FALHOU:"; head -40 "$OUTDIR/link.err"; exit $RC; fi
file "$OUTDIR/eboot.elf"; "$LLVM/llvm-readelf" -h "$OUTDIR/eboot.elf" | grep -E 'Type|Entry'

# --- 5) create-fself + PKG (reusa staging da casca) --------------------------
echo "=== create-fself + PKG ==="
CDIR=macos
"$TOOLCHAIN/bin/$CDIR/create-fself-macos" -in="$OUTDIR/eboot.elf" -out="$OUTDIR/eboot.oelf" \
  --eboot "eboot.bin" --paid 0x3800000000000011
# o create-fself grava eboot.bin no cwd do -out? Checa ambos, com erro explícito.
if [ ! -f "$OUTDIR/eboot.bin" ] && [ -f eboot.bin ]; then mv eboot.bin "$OUTDIR/eboot.bin"; fi
[ -f "$OUTDIR/eboot.bin" ] || { echo "ERRO: create-fself nao gerou eboot.bin" >&2; exit 6; }
echo "eboot.bin: $(ls -la "$OUTDIR/eboot.bin" | awk '{print $5}') bytes"
echo "OK. Próximo: empacotar com gp4 da casca trocando eboot.bin (passo manual/CI)."
