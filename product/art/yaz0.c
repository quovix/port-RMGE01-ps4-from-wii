/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "yaz0.h"

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

uint32_t qvx_yaz0_uncompressed_size(const uint8_t *src, size_t srclen) {
    if (srclen < 16 || src[0] != 'Y' || src[1] != 'a' || src[2] != 'z' || src[3] != '0')
        return 0;
    return be32(src + 4);
}

size_t qvx_yaz0_decompress(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstcap) {
    uint32_t outsize = qvx_yaz0_uncompressed_size(src, srclen);
    if (outsize == 0 || outsize > dstcap) return 0;

    size_t in = 16;          /* após o header */
    size_t out = 0;
    uint8_t group = 0;
    int bits = 0;

    while (out < outsize) {
        if (bits == 0) {                 /* novo byte de controle */
            if (in >= srclen) return 0;
            group = src[in++];
            bits = 8;
        }
        if (group & 0x80) {              /* bit 1: literal */
            if (in >= srclen) return 0;
            dst[out++] = src[in++];
        } else {                         /* bit 0: back-reference */
            if (in + 1 >= srclen) return 0;
            uint8_t b1 = src[in++], b2 = src[in++];
            uint32_t dist = (((uint32_t)(b1 & 0x0F)) << 8 | b2) + 1;
            uint32_t len = b1 >> 4;
            if (len == 0) {              /* comprimento estendido */
                if (in >= srclen) return 0;
                len = (uint32_t)src[in++] + 0x12;
            } else {
                len += 2;
            }
            if (dist > out) return 0;    /* referência inválida */
            size_t from = out - dist;
            for (uint32_t i = 0; i < len && out < outsize; i++)
                dst[out++] = dst[from + i];   /* cópia byte-a-byte (overlap ok) */
        }
        group <<= 1;
        bits--;
    }
    return out;
}
