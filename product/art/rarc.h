/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Leitor de archive RARC (GC/Wii), big-endian. Opera sobre um buffer já
   descomprimido (ver yaz0). Lê arquivos do dump do usuário em runtime;
   nenhum dado do jogo é embarcado. */
#ifndef QVX_RARC_H
#define QVX_RARC_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;      /* buffer RARC inteiro (descomprimido) */
    size_t         size;
    const uint8_t *nodes;     /* tabela de nós (dirs), 0x10 cada */
    uint32_t       num_nodes;
    const uint8_t *entries;   /* tabela de file entries, 0x14 cada */
    uint32_t       num_entries;
    const char    *strings;   /* string table */
    uint32_t       strings_len;
    const uint8_t *filedata;  /* início da seção de dados */
} qvx_rarc;

/* Abre/valida um RARC em `data`. 0 em sucesso, <0 em erro. */
int qvx_rarc_open(qvx_rarc *r, const uint8_t *data, size_t size);

/* Nº de file entries (inclui diretórios). */
uint32_t qvx_rarc_entry_count(const qvx_rarc *r);

/* Dados de uma entry i. Retorna 0 em sucesso.
   `is_dir` != 0 para diretório; para arquivo, `*out_data`/`*out_size` apontam
   pro conteúdo dentro do buffer (sem cópia). `name` aponta na string table. */
int qvx_rarc_entry(const qvx_rarc *r, uint32_t i,
                   const char **name, int *is_dir,
                   const uint8_t **out_data, uint32_t *out_size);

/* Acha o 1º arquivo cujo basename == `name`. Retorna ponteiro+size, ou NULL. */
const uint8_t *qvx_rarc_find(const qvx_rarc *r, const char *name, uint32_t *out_size);

/* Acha o 1º arquivo cuja extensão (ex. "tpl") bate. NULL se não houver. */
const uint8_t *qvx_rarc_find_ext(const qvx_rarc *r, const char *ext,
                                 const char **out_name, uint32_t *out_size);

#endif
