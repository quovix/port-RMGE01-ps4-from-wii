/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Decoder de textura TPL (GC/Wii) → RGBA (0xAARRGGBB). Implementação própria do
   formato público (tiling + conversões). Decodifica texturas do dump do usuário
   em runtime; nenhum dado do jogo é embarcado. */
#ifndef QVX_TPL_H
#define QVX_TPL_H

#include <stddef.h>
#include <stdint.h>

enum {
    QVX_TPL_I4 = 0, QVX_TPL_I8 = 1, QVX_TPL_IA4 = 2, QVX_TPL_IA8 = 3,
    QVX_TPL_RGB565 = 4, QVX_TPL_RGB5A3 = 5, QVX_TPL_RGBA8 = 6,
    QVX_TPL_CMPR = 14,
};

/* Lê dimensões/formato da 1ª textura. 0 em sucesso. */
int qvx_tpl_info(const uint8_t *data, size_t size,
                 uint32_t *width, uint32_t *height, uint32_t *format);

/* Decodifica a 1ª textura em `out` (width*height pixels, 0xAARRGGBB).
   `out_cap` em pixels. Retorna 0 em sucesso, <0 em erro/formato não suportado. */
int qvx_tpl_decode(const uint8_t *data, size_t size,
                   uint32_t *out, size_t out_cap);

#endif
