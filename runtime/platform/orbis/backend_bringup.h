/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Bring-up do backend Orbis: valida os slices que já existem (MTX + GX/VI) no
   hardware e reporta o progresso via log on-screen (qvx_log_line). É o passo
   que tira a casca do estado "aguardando backend": prova que os símbolos do
   motor (nomes da API do Wii) estão linkados e executam de verdade. */
#ifndef QVX_BACKEND_BRINGUP_H
#define QVX_BACKEND_BRINGUP_H

/* Roda o bring-up e loga cada etapa na tela. Retorna 0 se tudo passou,
   ou o número de checagens que falharam. */
int qvx_backend_bringup(void);

#endif
