/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — fixtures/teste originais da ponte de heap (quovix) */
#include "../../runtime/compat/heap_new.hpp"
#include "../../runtime/compat/heap_smoke.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static qvx_alloc_size last_bytes;
static JKRHeap* last_owner;
static int last_alignment;
static int allocations, releases, destroyed;
static unsigned char large_request_probe;

static void* test_allocate(qvx_alloc_size bytes, JKRHeap* owner, int alignment) {
    last_bytes = bytes; last_owner = owner; last_alignment = alignment;
    ++allocations;
    /* Teste de transporte do argumento de 64 bits: sem reservar gigabytes,
       sem construir um objeto nesse endereço. */
    if (bytes > 1024 * 1024) return &large_request_probe;
    return malloc(bytes ? bytes : 1);
}

static void test_release(void* memory, JKRHeap* owner, int alignment) {
    assert(owner == last_owner && alignment == last_alignment);
    ++releases;
    if (memory != &large_request_probe) free(memory);
}

struct Value { int n; Value() : n(42) {} };
struct Throwing { Throwing() { throw 7; } };
struct ArrayThrowing {
    static int made;
    ArrayThrowing() { if (++made == 3) throw 8; }
    ~ArrayThrowing() { ++destroyed; }
};
int ArrayThrowing::made = 0;

int main() {
    assert(qvx_heap_smoke_test() == 0);
    assert(sizeof(qvx_alloc_size) == sizeof(void*));
    const qvx_heap_allocator allocator = {test_allocate, test_release};
    const qvx_heap_allocator incomplete = {test_allocate, 0};
    assert(qvx_heap_bind(&allocator) == 0);
    assert(qvx_heap_bind(&incomplete) == -1);
    JKRHeap* owner = reinterpret_cast<JKRHeap*>(&large_request_probe); /* opaco */

    Value* a = new (-64) Value;
    assert(a->n == 42 && last_owner == 0 && last_alignment == -64);
    a->~Value(); qvx_heap_release(a, 0, -64);
    Value* b = new (owner) Value;
    assert(b->n == 42 && last_owner == owner && last_alignment == 0);
    b->~Value(); qvx_heap_release(b, owner, 0);
    Value* c = new (owner, 32) Value;
    assert(c->n == 42 && last_bytes == sizeof(Value));
    c->~Value(); qvx_heap_release(c, owner, 32);

    unsigned char* d = new (16) unsigned char[19];
    assert(last_bytes >= 19 && last_owner == 0 && last_alignment == 16);
    d[18] = 1; qvx_heap_release(d, 0, 16);
    unsigned char* e = new (owner) unsigned char[23];
    assert(last_owner == owner && last_alignment == 0);
    e[22] = 2; qvx_heap_release(e, owner, 0);
    unsigned char* f = new (owner, -128) unsigned char[31];
    assert(last_owner == owner && last_alignment == -128);
    f[30] = 3; qvx_heap_release(f, owner, -128);

    const qvx_alloc_size large = (static_cast<qvx_alloc_size>(1) << 33) + 37;
    void* probe = ::operator new(large, owner, 64);
    assert(last_bytes == large);
    qvx_heap_release(probe, owner, 64);

    int cleanup_before = releases;
    try { (void)new (owner, 64) Throwing; assert(false); }
    catch (int value) { assert(value == 7 && releases == cleanup_before + 1); }
    cleanup_before = releases;
    try { (void)new (owner, 128) ArrayThrowing[5]; assert(false); }
    catch (int value) { assert(value == 8 && releases == cleanup_before + 1 && destroyed == 2); }

    qvx_heap_release(0, 0, 0);
    assert(allocations == releases);
    assert(qvx_heap_bind(0) == 0);
    puts("heap OK: seis overloads, tamanho de 64 bits, owner/alignment e cleanup de constructors");
    return 0;
}
