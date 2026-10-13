/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "stub_trace.h"
#include <orbis/libkernel.h>
#include <stdio.h>

/* Teto de linhas no klog. O primeiro crash do salto controlado é esperado cedo
   (stubs retornam 0 -> ponteiros nulos), então as primeiras chamadas bastam pra
   localizar a falta. Passar do teto significa progresso: só conta a partir daí,
   sem inundar o klog (cada linha é um syscall). */
#define QVX_TRACE_CAP 2000u

static unsigned g_calls;   /* contador global de chamadas a stubs */

/* --- Trilha de crash lida AO VIVO via ps4debug (sem depender do klog:3232) ----
   O klog está mudo pra este app. Então, além de logar, gravamos numa global em
   .data o nº total de chamadas e um RING com os últimos nomes de stub. Quando um
   stub retorna 0 e o motor deref-a nulo, o processo morre: o poller lê este ring
   e o ÚLTIMO nome é (quase sempre) a função que precisa ser implementada.
   Endereços estáveis (0x400000 + offset do nm). @orig none. */
#define QVX_RING 16u
#define QVX_NAME 160u
volatile unsigned g_qvx_stub_seq = 0;             /* = g_calls, legível externamente */
volatile char     g_qvx_stub_ring[QVX_RING][QVX_NAME];

static void qvx_ring_put(unsigned n, const char *name) {
    volatile char *slot = g_qvx_stub_ring[(n - 1u) % QVX_RING];
    const char *s = name ? name : "(null)";
    unsigned i = 0;
    for (; i < QVX_NAME - 1u && s[i]; i++) slot[i] = s[i];
    slot[i] = 0;
}

void qvx_stub_hit(const char *name) {
    /* O motor é multithread; incremento atômico pra o nº de sequência e o guard
       do teto ficarem corretos sob concorrência. buf é local -> sem corrida no
       texto; cada DebugOutText emite uma string completa. */
    unsigned n = __atomic_add_fetch(&g_calls, 1u, __ATOMIC_RELAXED);
    __atomic_store_n(&g_qvx_stub_seq, n, __ATOMIC_RELAXED);
    qvx_ring_put(n, name);     /* trilha legível via ps4debug */
    if (n <= QVX_TRACE_CAP) {
        /* nomes mangled de SMG (namespaces/templates) passam de 160 chars;
           512 preserva a cauda que distingue overloads/instâncias. */
        char buf[512];
        snprintf(buf, sizeof buf, "[quovix][stub #%u] %s\n", n, name ? name : "(null)");
        sceKernelDebugOutText(0, buf);
    } else if (n == QVX_TRACE_CAP + 1u) {
        sceKernelDebugOutText(0,
            "[quovix][stub] teto de trace atingido; silenciando (so conta)\n");
    }
}

unsigned qvx_stub_calls(void) { return __atomic_load_n(&g_calls, __ATOMIC_RELAXED); }
