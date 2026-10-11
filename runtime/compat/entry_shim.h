/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — shim de entry point original (quovix), Apache-2.0 */
#ifndef QVX_ENTRY_SHIM_H
#define QVX_ENTRY_SHIM_H

/* O entry do jogo no decomp é `void main(void)` (convenção CodeWarrior/Wii, onde
   o __start do SDK chama um main que retorna void). O clang em modo hosted recusa
   `void main`. No eboot PS4 o entry real é o crt0/_start, nao `main`; o corpo do
   entry do jogo e' agnostico a isso.

   Este preinclude renomeia o identificador `main` para `qvx_game_entry`, sem tocar
   no source do decomp, de modo que o CORPO do entry compile. O boot stub do eboot
   chama `qvx_game_entry()` a partir do _start real. Em src/Game o token `main` so'
   aparece como identificador de codigo no entry (System/GameSystem.cpp); demais
   ocorrencias sao substrings (remain, domain, MainXxx, mMain, ...) ou comentarios,
   nao afetadas por esta macro de objeto. */
#define main qvx_game_entry

#endif
