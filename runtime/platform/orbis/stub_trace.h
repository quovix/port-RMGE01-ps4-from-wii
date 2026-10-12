/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#ifndef QVX_STUB_TRACE_H
#define QVX_STUB_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Chamado por cada stub weak no-op da superfície de plataforma ainda não
   implementada. Registra a chamada no klog (sceKernelDebugOutText) com número
   de sequência, pra que o rastro de chamadas até um crash fique capturável via
   GoldHEN:3232. É o sinal do loop crash-driven: a última linha antes do fault
   aponta a função que o motor pediu e nós ainda não temos. */
void qvx_stub_hit(const char *name);

/* Total de chamadas a stubs desde o boot (inclui as silenciadas pelo teto). */
unsigned qvx_stub_calls(void);

#ifdef __cplusplus
}
#endif

#endif
