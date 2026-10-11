/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — autoteste original, sem headers/source do jogo (quovix) */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include "heap_new.hpp"
#include "heap_smoke.h"
#include <stdint.h>
#include <stdlib.h>

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
