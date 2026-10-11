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

#endif
