/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do art_splash: carrega uma TPL nomeada de um .arc e reporta
   APENAS dimensão + checksum. A imagem decodada NÃO é gravada nem exibida. */
#include "../art/art_splash.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: %s <arquivo.arc> <tpl_name>\n", argv[0]); return 2; }
    static uint32_t px[512 * 288];
    uint32_t w = 0, h = 0;
    int rc = qvx_art_load_tpl_from_arc(argv[1], argv[2], px, 512 * 288, &w, &h);
    if (rc) { printf("falha rc=%d\n", rc); return 1; }

    unsigned long long hsh = 1469598103934665603ULL;
    size_t opaque = 0, total = (size_t)w * h;
    for (size_t i = 0; i < total; i++) {
        hsh ^= px[i]; hsh *= 1099511628211ULL;
        if ((px[i] >> 24) == 0xFF) opaque++;
    }
    printf("splash OK: %ux%u, %zu px, checksum=%016llx, opacos=%.1f%%\n",
           w, h, total, hsh, 100.0 * opaque / total);
    return 0;
}
