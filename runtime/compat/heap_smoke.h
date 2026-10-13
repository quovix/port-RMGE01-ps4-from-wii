/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — autoteste original da ponte (quovix), Apache-2.0 */
#ifndef QVX_HEAP_SMOKE_H
#define QVX_HEAP_SMOKE_H
#ifdef __cplusplus
extern "C" {
#endif
/* Executar uma vez no startup, antes de instalar qualquer backend do motor.
   Usa apenas heap nativo com owner NULL, deixa a ponte desligada ao terminar.
   0 = sucesso; negativo = falha na verificação (allocation falha aborta). */
int qvx_heap_smoke_test(void);

/* Vincula o alocador permissivo PERSISTENTE pro run do jogo (malloc/posix_memalign,
   owner opaco, aceita alinhamento negativo = JKR "do fim"). Chamar no startup,
   depois do smoke test e ANTES dos ctors do motor / qvx_game_entry.
   0 = sucesso; -1 = callbacks incompletos. */
int qvx_heap_bind_game(void);
#ifdef __cplusplus
}
#endif
#endif
