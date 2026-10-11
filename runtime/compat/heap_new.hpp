/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — ponte de alocação original (quovix), Apache-2.0 */
#ifndef QVX_HEAP_NEW_HPP
#define QVX_HEAP_NEW_HPP

#ifdef __cplusplus
/* O primeiro argumento de operator new precisa do tipo size_t do TARGET.
   Não incluir MSL: seu typedef de size_t pode ter largura diferente. */
typedef __SIZE_TYPE__ qvx_alloc_size;
class JKRHeap;

struct qvx_heap_allocator {
    void* (*allocate)(qvx_alloc_size bytes, JKRHeap* owner, int alignment);
    void (*release)(void* memory, JKRHeap* owner, int alignment);
};

/* Configurar somente no startup, sem operações/allocations em andamento.
   NULL desliga a ponte. Retorna -1 para callbacks incompletos, 0 em sucesso.
   O owner é opaco; o backend deve respeitar heap e alinhamento solicitados. */
int qvx_heap_bind(const qvx_heap_allocator* allocator);
void qvx_heap_release(void* memory, JKRHeap* owner, int alignment);

/* Sobrecargas próprias com tamanho nativo; sem habilitar __MWERKS__.
   Falta de backend ou falha de allocation aborta (build sem exceptions).
   O delete comum do motor ainda exige um backend com lookup de ownership. */
void* operator new(qvx_alloc_size bytes, int alignment);
void* operator new(qvx_alloc_size bytes, JKRHeap* owner);
void* operator new(qvx_alloc_size bytes, JKRHeap* owner, int alignment);
void* operator new[](qvx_alloc_size bytes, int alignment);
void* operator new[](qvx_alloc_size bytes, JKRHeap* owner);
void* operator new[](qvx_alloc_size bytes, JKRHeap* owner, int alignment);

/* Cleanup se um constructor lançar; não é o delete comum de objetos vivos. */
void operator delete(void* memory, int alignment) throw();
void operator delete(void* memory, JKRHeap* owner) throw();
void operator delete(void* memory, JKRHeap* owner, int alignment) throw();
void operator delete[](void* memory, int alignment) throw();
void operator delete[](void* memory, JKRHeap* owner) throw();
void operator delete[](void* memory, JKRHeap* owner, int alignment) throw();
#endif
#endif
