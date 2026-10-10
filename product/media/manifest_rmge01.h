/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Manifesto de mídia esperada. NÃO contém dados do jogo — só identificadores e
   hashes conhecidos usados para verificar o dump do próprio usuário (BYO-dump). */
#ifndef QVX_MANIFEST_RMGE01_H
#define QVX_MANIFEST_RMGE01_H

#define QVX_GAME_ID    "RMGE01"       /* Wii, NTSC-U */
#define QVX_REGION     "NTSC-U"
#define QVX_SOURCE     "wii"
#define QVX_ASIC       "FR69GJ5CI"     /* ver quovix/asirn */
#define QVX_CONTENT_ID "UP0001-QVOX00001_00-7E0C9848AC532465"

/* Hash conhecido do executável (sys/main.dol), Disc 1 Rev 0, NTSC-U.
   É o src_sha1 do ingest; verifica que o dump bate antes de rodar. */
#define QVX_MAINDOL_SHA1 "9a71008ae1ee9010e267fa67d1f0b0d4f0e895dd"

#endif
