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

/* Tick de boot: loga (tela + klog) e APRESENTA um frame. Definido em main.c
   (tem acesso ao framebuffer/splash). Usado aqui pra que cada sub-passo do
   bring-up fique visível — a última linha na tela localiza o fault, já que
   klog/beacon/sigaction não são confiáveis no Orbis. */
void qvx_boot_tick(const char *msg);

#endif
