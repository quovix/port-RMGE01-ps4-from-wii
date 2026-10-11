/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do decoder TPL. Decodifica a 1ª textura de um .arc e reporta
   APENAS metadados e um checksum dos pixels — a imagem decodada NÃO é gravada
   nem exibida (é dado do jogo; fica só em memória). */
#include "../art/yaz0.h"
#include "../art/rarc.h"
#include "../art/tpl.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s <arquivo.arc>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *src = malloc(n); fread(src, 1, n, f); fclose(f);

    uint8_t *buf; size_t blen;
    uint32_t u = qvx_yaz0_uncompressed_size(src, n);
    if (u) { buf = malloc(u); blen = qvx_yaz0_decompress(src, n, buf, u); }
    else   { buf = src; blen = n; }

    qvx_rarc r;
    if (qvx_rarc_open(&r, buf, blen) != 0) { fprintf(stderr, "RARC?\n"); return 1; }
    const char *tn; uint32_t ts;
    const uint8_t *tpl = qvx_rarc_find_ext(&r, "tpl", &tn, &ts);
    if (!tpl) { fprintf(stderr, "sem .tpl\n"); return 1; }

    uint32_t w, h, fmt;
    if (qvx_tpl_info(tpl, ts, &w, &h, &fmt) != 0) { fprintf(stderr, "TPL inválido\n"); return 1; }
    printf("TPL: %s  %ux%u  formato=%u\n", tn, w, h, fmt);

    uint32_t *px = calloc((size_t)w * h, 4);
    int rc = qvx_tpl_decode(tpl, ts, px, (size_t)w * h);
    if (rc != 0) { printf("decode nao suportado/erro (rc=%d)\n", rc); return 1; }

    /* checksum FNV-1a + cobertura de alpha (sanidade; não é a arte) */
    uint64_t hsh = 1469598103934665603ULL;
    size_t opaque = 0, total = (size_t)w * h;
    for (size_t i = 0; i < total; i++) {
        uint32_t v = px[i];
        hsh ^= v; hsh *= 1099511628211ULL;
        if ((v >> 24) == 0xFF) opaque++;
    }
    printf("decodificado OK: %zu pixels, checksum=%016llx, opacos=%.1f%%\n",
           total, (unsigned long long)hsh, 100.0 * opaque / total);
    return 0;
}
