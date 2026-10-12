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

void qvx_stub_hit(const char *name) {
    /* O motor é multithread; incremento atômico pra o nº de sequência e o guard
       do teto ficarem corretos sob concorrência. buf é local -> sem corrida no
       texto; cada DebugOutText emite uma string completa. */
    unsigned n = __atomic_add_fetch(&g_calls, 1u, __ATOMIC_RELAXED);
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
