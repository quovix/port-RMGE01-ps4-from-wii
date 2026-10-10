/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* SHA-1 (FIPS 180-1) — implementação própria, sem dependências (portável p/ Orbis). */
#ifndef QVX_SHA1_H
#define QVX_SHA1_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[5];
    uint64_t len;       /* bytes totais */
    uint8_t  buf[64];
    size_t   buflen;
} qvx_sha1;

void qvx_sha1_init(qvx_sha1 *c);
void qvx_sha1_update(qvx_sha1 *c, const void *data, size_t n);
void qvx_sha1_final(qvx_sha1 *c, uint8_t out[20]);

/* Conveniência: sha1 de um arquivo -> string hex minúscula (41 bytes c/ NUL).
   Retorna 0 em sucesso, <0 em erro de I/O. */
int qvx_sha1_file_hex(const char *path, char out_hex[41]);

#endif
