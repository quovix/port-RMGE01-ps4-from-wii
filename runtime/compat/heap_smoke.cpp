/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — autoteste original, sem headers/source do jogo (quovix) */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include "heap_new.hpp"
#include "heap_smoke.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#ifndef MAP_ANON
#  ifdef MAP_ANONYMOUS
#    define MAP_ANON MAP_ANONYMOUS
#  endif
#endif

/* --- Pool de BAIXO ENDEREÇO (< 4GB) para objetos do motor --------------------
   O motor faz masking de 32-bit em ponteiros (this de JKRArchive, arenas, etc.);
   objeto acima de 4GB (malloc dava 0x201...) é truncado -> deref de lixo -> crash.
   Servimos TODO operator new de um mmap fixo < 4GB com bump-alloc: endereços
   baixos (masking lossless) e muito espaço (sem o overflow dos game heaps finitos).
   Bump sem free individual: suficiente pro boot; free vira reuso só no topo.
   (diagnóstico emulador 2026-10-13: ArchiveHolder/JKRMemArchive em 0x201...). */
#define QVX_LOWPOOL_BASE 0xA0000000UL              /* > MEM2(0x94000000), < 4GB */
#define QVX_LOWPOOL_SIZE 0x10000000UL              /* 256 MB -> termina 0xB0000000.
   PS4/shadPS4 tem ~367MB de flexible memory livre; 1.25GB nao cabia (Out of
   flexible memory). Os objetos de operator-new iriam pros game heaps de 88MB no
   HW real, entao 256MB cobre o boot com folga. (diagnostico emulador 2026-10-13) */
static char*  g_lp_base = 0;
static size_t g_lp_off  = 0;
static int    g_lp_fail = 0;
static int lowpool_init(void) {
    if (g_lp_base) return 1;
    if (g_lp_fail) return 0;
    void* want = (void*)QVX_LOWPOOL_BASE;
    void* got = mmap(want, QVX_LOWPOOL_SIZE, PROT_READ | PROT_WRITE,
                     MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
    if (got == MAP_FAILED || got != want) { g_lp_fail = 1; return 0; }
    g_lp_base = (char*)want; g_lp_off = 0;
    return 1;
}
static void* lowpool_alloc(qvx_alloc_size bytes, qvx_alloc_size align) {
    if (!lowpool_init()) return 0;
    if (align < sizeof(void*)) align = sizeof(void*);
    size_t off = (g_lp_off + (align - 1)) & ~(size_t)(align - 1);
    if (off + bytes > QVX_LOWPOOL_SIZE) return 0;   /* esgotou: cai no fallback */
    g_lp_off = off + (bytes ? bytes : 1);
    return g_lp_base + off;
}
static int lowpool_owns(void* p) {
    return g_lp_base && (char*)p >= g_lp_base && (char*)p < g_lp_base + QVX_LOWPOOL_SIZE;
}

/* Exposto p/ C: alocar do pool baixo (< 4GB). Usado pelo conversor de dados RARC
   (qvx_rarc_conv.c), cujo buffer convertido precisa ser < 4GB (ponteiros/masking). */
extern "C" void* qvx_pool_alloc(unsigned long bytes, unsigned long align) {
    if (align < sizeof(void*)) align = sizeof(void*);
    void* m = lowpool_alloc((qvx_alloc_size)bytes, (qvx_alloc_size)align);
    return m; /* NULL se o pool esgotar: o chamador trata (não converte) */
}

static void* native_allocate(qvx_alloc_size bytes, JKRHeap* owner, int alignment) {
    /* Não tratar um heap do jogo como malloc silenciosamente. */
    if (owner || alignment < 0) return 0;
    if (alignment == 0) return malloc(bytes ? bytes : 1);
    qvx_alloc_size align = static_cast<qvx_alloc_size>(alignment);
    if ((align & (align - 1)) != 0) return 0;
    if (align < sizeof(void*)) align = sizeof(void*);
    void* memory = 0;
    return posix_memalign(&memory, align, bytes ? bytes : 1) == 0 ? memory : 0;
}

static void native_release(void* memory, JKRHeap*, int) { free(memory); }

/* --- Alocador PERSISTENTE pro run real do jogo -------------------------------
   Diferente do native_allocate do smoke test (que recusa owner!=NULL e
   alinhamento<0 DE PROPÓSITO, pra não mascarar heap do jogo em teste de host),
   este é permissivo: trata owner como opaco (JKRHeap é stub -> ignora) e aceita
   alinhamento negativo (convenção JKR = alocar do fim do heap) pelo módulo.
   Sem ele vinculado, o 1º `new (pHeap, align)` do motor (FileRipper::setup)
   cai em abort() -> SIGABRT ~5s no boot. @orig none. */
static void* game_allocate(qvx_alloc_size bytes, JKRHeap* /*owner*/, int alignment) {
    int a = alignment < 0 ? -alignment : alignment;   /* JKR: <0 = do fim */
    qvx_alloc_size align = a ? static_cast<qvx_alloc_size>(a) : sizeof(void*);
    if ((align & (align - 1)) != 0) {                 /* não pot. de 2: arredonda */
        qvx_alloc_size p = 1; while (p < align) p <<= 1; align = p;
    }
    /* pool de baixo endereço primeiro (endereços < 4GB p/ o masking do motor) */
    void* m = lowpool_alloc(bytes, align);
    if (m != 0) return m;
    /* pool esgotou ou não mapeou: fallback malloc (raro; pode cair alto) */
    if (a == 0) return malloc(bytes ? bytes : 1);
    void* memory = 0;
    return posix_memalign(&memory, align, bytes ? bytes : 1) == 0 ? memory : 0;
}
static void game_release(void* memory, JKRHeap*, int) {
    /* alocações do pool são bump-only (sem free individual); só libera o fallback. */
    if (lowpool_owns(memory)) return;
    free(memory);
}

/* Vincula o alocador do jogo de forma PERSISTENTE (não desliga depois). Chamar
   no main ANTES dos ctors do motor e do qvx_game_entry. C-callable. */
extern "C" int qvx_heap_bind_game(void) {
    static const qvx_heap_allocator allocator = {game_allocate, game_release};
    return qvx_heap_bind(&allocator);
}

int qvx_heap_smoke_test(void) {
    typedef char native_width[(sizeof(qvx_alloc_size) == sizeof(void*)) ? 1 : -1];
    (void)sizeof(native_width);
    const qvx_heap_allocator allocator = {native_allocate, native_release};
    if (qvx_heap_bind(&allocator) != 0) return -1;
    unsigned long* scalar = new (static_cast<JKRHeap*>(0), 64) unsigned long(0x515658UL);
    unsigned char* array = new (128) unsigned char[257];
    array[0] = 0x51; array[256] = 0x58;
    int result = (*scalar == 0x515658UL && array[0] == 0x51 && array[256] == 0x58 &&
                  (reinterpret_cast<uintptr_t>(scalar) & 63) == 0 &&
                  (reinterpret_cast<uintptr_t>(array) & 127) == 0) ? 0 : -2;
    qvx_heap_release(scalar, 0, 64);
    qvx_heap_release(array, 0, 128);
    qvx_heap_bind(0);
    return result;
}
