/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Descompressor Yaz0 (formato de compressão da Nintendo, GC/Wii).
   Implementação própria, portável (host e Orbis). Lê arquivos do dump do
   usuário em runtime; nenhum dado do jogo é embarcado. */
#ifndef QVX_YAZ0_H
#define QVX_YAZ0_H

#include <stddef.h>
#include <stdint.h>

/* Lê o tamanho descomprimido do header Yaz0 (0 se magic inválido). */
uint32_t qvx_yaz0_uncompressed_size(const uint8_t *src, size_t srclen);

/* Descomprime `src` (stream Yaz0 completo) em `dst` (>= uncompressed_size).
   Retorna nº de bytes escritos, ou 0 em erro. */
size_t qvx_yaz0_decompress(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstcap);

#endif
