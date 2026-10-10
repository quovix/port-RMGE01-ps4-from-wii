/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do módulo de verificação de mídia.
   Uso: verify_test <caminho/sys/main.dol> */
#include "../media/media_verify.h"
#include "../media/manifest_rmge01.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: %s <main.dol>\n", argv[0]); return 2; }
    printf("manifesto: game=%s regiao=%s asic=%s\n", QVX_GAME_ID, QVX_REGION, QVX_ASIC);
    printf("esperado sha1(main.dol): %s\n", QVX_MAINDOL_SHA1);
    qvx_mv_result r = qvx_mv_verify_maindol(argv[1]);
    printf("resultado: %s -> %s\n", (r == QVX_MV_OK ? "OK" : "FALHA"), qvx_mv_strerror(r));
    return r == QVX_MV_OK ? 0 : 1;
}
