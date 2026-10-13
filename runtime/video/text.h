/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Texto na tela (fonte 8x8 própria) + um log on-screen que a casca desenha.
   Desenha no back buffer do renderer SceVideoOut. */
#ifndef QVX_TEXT_H
#define QVX_TEXT_H
#include <stdint.h>

/* Desenha `s` em (x,y) no back buffer, cor 0xAARRGGBB, escala inteira `scale`.
   '\n' quebra linha. Minúsculas viram maiúsculas (fonte é uppercase). */
void qvx_text_draw(int x, int y, const char *s, uint32_t color, int scale);

/* Log on-screen: acumula linhas (com scroll) e desenha todas. */
void qvx_log_line(const char *s);
void qvx_log_render(int x, int y, uint32_t color, int scale);

/* Canal de fase lido AO VIVO via ps4debug (g_qvx_phase, definido no main.c).
   Declarado aqui pra OSPanic/OSHalt (os.c) poderem gravar o motivo do halt no
   único canal que o poller lê — o os_halt_loop não encerra o processo, então o
   panic fica legível. @orig none. */
void qvx_set_phase(const char *s);

#endif
