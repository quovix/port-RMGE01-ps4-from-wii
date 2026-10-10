/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "media_verify.h"
#include "manifest_rmge01.h"
#include "sha1.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

int qvx_mv_read_game_id(const char *disc_path, char out_id[7]) {
    FILE *f = fopen(disc_path, "rb");
    if (!f) return -1;
    size_t n = fread(out_id, 1, 6, f);
    fclose(f);
    if (n != 6) return -2;
    out_id[6] = 0;
    return 0;
}

qvx_mv_result qvx_mv_verify_file_sha1(const char *path, const char *expected_hex) {
    char got[41];
    if (qvx_sha1_file_hex(path, got) != 0) return QVX_MV_IO_ERROR;
    return (strcasecmp(got, expected_hex) == 0) ? QVX_MV_OK : QVX_MV_HASH_MISMATCH;
}

qvx_mv_result qvx_mv_verify_maindol(const char *maindol_path) {
    return qvx_mv_verify_file_sha1(maindol_path, QVX_MAINDOL_SHA1);
}

const char *qvx_mv_strerror(qvx_mv_result r) {
    switch (r) {
        case QVX_MV_OK:           return "ok";
        case QVX_MV_IO_ERROR:     return "erro de I/O (mídia não lida)";
        case QVX_MV_BAD_GAME_ID:  return "game ID inesperado (jogo errado)";
        case QVX_MV_HASH_MISMATCH:return "hash não bate (dump errado ou corrompido)";
    }
    return "desconhecido";
}
