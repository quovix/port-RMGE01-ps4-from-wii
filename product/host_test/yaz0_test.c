/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do descompressor Yaz0.
   Uso: yaz0_test <arquivo.arc>  — descomprime e confere o magic RARC.
   A saída fica só na memória; nada do jogo é gravado nem commitado. */
#include "../art/yaz0.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s <arquivo.arc>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *src = malloc(n);
    if (fread(src, 1, n, f) != (size_t)n) { fprintf(stderr, "read\n"); return 1; }
    fclose(f);

    uint32_t usz = qvx_yaz0_uncompressed_size(src, n);
    printf("Yaz0: comprimido=%ld bytes, descomprimido=%u bytes\n", n, usz);
    if (!usz) { fprintf(stderr, "não é Yaz0\n"); return 1; }

    uint8_t *dst = malloc(usz);
    size_t out = qvx_yaz0_decompress(src, n, dst, usz);
    if (out != usz) { fprintf(stderr, "FALHA: escreveu %zu de %u\n", out, usz); return 1; }

    int rarc = (dst[0] == 'R' && dst[1] == 'A' && dst[2] == 'R' && dst[3] == 'C');
    printf("descomprimiu %zu bytes; magic interno: %c%c%c%c -> %s\n",
           out, dst[0], dst[1], dst[2], dst[3], rarc ? "RARC OK" : "inesperado");
    free(src); free(dst);
    return rarc ? 0 : 1;
}
