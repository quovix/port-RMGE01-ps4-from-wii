/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — dispatch original para backend de heap (quovix), Apache-2.0 */
#include "heap_new.hpp"
#include <stdlib.h>

static qvx_heap_allocator backend = {0, 0};

int qvx_heap_bind(const qvx_heap_allocator* allocator) {
    if (allocator && (!allocator->allocate || !allocator->release)) return -1;
    if (allocator) backend = *allocator;
    else { backend.allocate = 0; backend.release = 0; }
    return 0;
}

static void* allocate(qvx_alloc_size bytes, JKRHeap* owner, int alignment) {
    if (!backend.allocate) abort();
    void* memory = backend.allocate(bytes, owner, alignment);
    if (!memory) abort();
    return memory;
}

void qvx_heap_release(void* memory, JKRHeap* owner, int alignment) {
    if (!memory) return;
    if (!backend.release) abort();
    backend.release(memory, owner, alignment);
}

/* operator new/delete PLANOS: sem estes, `new X()` do decomp caía no operator
   new do libc++ (malloc), fora do heap do jogo. Roteamos pro nosso allocate()
   (game_allocate), que é o comportamento esperado do SMG (alocar do heap atual)
   e memória conhecida-boa. (diagnóstico 2026-10-13: new GameSystem() antes disto). */
void* operator new(qvx_alloc_size bytes) { return allocate(bytes, 0, 0); }
void* operator new[](qvx_alloc_size bytes) { return allocate(bytes, 0, 0); }
void operator delete(void* memory) throw() { qvx_heap_release(memory, 0, 0); }
void operator delete[](void* memory) throw() { qvx_heap_release(memory, 0, 0); }

void* operator new(qvx_alloc_size bytes, int alignment) { return allocate(bytes, 0, alignment); }
void* operator new(qvx_alloc_size bytes, JKRHeap* owner) { return allocate(bytes, owner, 0); }
void* operator new(qvx_alloc_size bytes, JKRHeap* owner, int alignment) { return allocate(bytes, owner, alignment); }
void* operator new[](qvx_alloc_size bytes, int alignment) { return allocate(bytes, 0, alignment); }
void* operator new[](qvx_alloc_size bytes, JKRHeap* owner) { return allocate(bytes, owner, 0); }
void* operator new[](qvx_alloc_size bytes, JKRHeap* owner, int alignment) { return allocate(bytes, owner, alignment); }

void operator delete(void* memory, int alignment) throw() { qvx_heap_release(memory, 0, alignment); }
void operator delete(void* memory, JKRHeap* owner) throw() { qvx_heap_release(memory, owner, 0); }
void operator delete(void* memory, JKRHeap* owner, int alignment) throw() { qvx_heap_release(memory, owner, alignment); }
void operator delete[](void* memory, int alignment) throw() { qvx_heap_release(memory, 0, alignment); }
void operator delete[](void* memory, JKRHeap* owner) throw() { qvx_heap_release(memory, owner, 0); }
void operator delete[](void* memory, JKRHeap* owner, int alignment) throw() { qvx_heap_release(memory, owner, alignment); }
