/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Carrega uma textura de branding do DUMP DO USUÁRIO em runtime e devolve RGBA
   para o blit da splash. Lê um .arc (Yaz0 -> RARC -> TPL); nenhum asset do jogo
   é embarcado nem gravado — a arte vive só no console, vinda do dump do dono. */
#ifndef QVX_ART_SPLASH_H
#define QVX_ART_SPLASH_H

#include <stddef.h>
#include <stdint.h>

/* Lê `arc_path`, acha a textura `tpl_name` e decodifica em `rgba` (cap em px).
   Preenche out_w e out_h. Retorna 0 em sucesso, <0 em erro (arquivo ausente,
   formato, capacidade). Não grava nada em disco. */
int qvx_art_load_tpl_from_arc(const char *arc_path, const char *tpl_name,
                              uint32_t *rgba, size_t rgba_cap,
                              uint32_t *out_w, uint32_t *out_h);

#endif
