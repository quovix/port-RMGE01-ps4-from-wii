/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do leitor RARC: descomprime (Yaz0) e lista os arquivos.
   Saída é só estrutura (nomes/tamanhos); nenhum conteúdo do jogo é gravado. */
#include "../art/yaz0.h"
#include "../art/rarc.h"
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
    else   { buf = src; blen = n; }   /* já descomprimido */

    qvx_rarc r;
    if (qvx_rarc_open(&r, buf, blen) != 0) { fprintf(stderr, "RARC inválido\n"); return 1; }

    printf("RARC: %u nós, %u entries\n", r.num_nodes, r.num_entries);
    uint32_t files = 0;
    for (uint32_t i = 0; i < qvx_rarc_entry_count(&r); i++) {
        const char *name; int dir; const uint8_t *d; uint32_t s;
        if (qvx_rarc_entry(&r, i, &name, &dir, &d, &s) != 0) continue;
        if (dir) continue;
        files++;
        printf("  [%2u] %-28s %7u bytes\n", i, name, s);
    }
    printf("total de arquivos: %u\n", files);

    /* procura uma textura .tpl */
    const char *tn; uint32_t ts;
    if (qvx_rarc_find_ext(&r, "tpl", &tn, &ts))
        printf("TPL encontrado: %s (%u bytes)\n", tn, ts);
    else
        printf("(sem .tpl neste archive)\n");
    return 0;
}
