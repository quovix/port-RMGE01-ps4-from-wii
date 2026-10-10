/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Verificação de mídia (BYO-dump): confere que o dump do usuário bate com o
   manifesto antes de rodar. Nenhum asset é embarcado. */
#ifndef QVX_MEDIA_VERIFY_H
#define QVX_MEDIA_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QVX_MV_OK = 0,
    QVX_MV_IO_ERROR,        /* não leu o arquivo */
    QVX_MV_BAD_GAME_ID,     /* game ID != esperado */
    QVX_MV_HASH_MISMATCH,   /* hash não bate (dump errado/corrompido) */
} qvx_mv_result;

/* Lê o game ID (6 chars) do offset 0 de uma imagem de disco (GCM/ISO).
   out_id deve ter >= 7 bytes. Retorna 0 em sucesso, <0 em erro de I/O. */
int qvx_mv_read_game_id(const char *disc_path, char out_id[7]);

/* Verifica que o arquivo em `path` tem exatamente o sha1 hex esperado. */
qvx_mv_result qvx_mv_verify_file_sha1(const char *path, const char *expected_hex);

/* Verifica o executável do jogo (sys/main.dol) contra o manifesto RMGE01. */
qvx_mv_result qvx_mv_verify_maindol(const char *maindol_path);

const char *qvx_mv_strerror(qvx_mv_result r);

#ifdef __cplusplus
}
#endif
#endif
