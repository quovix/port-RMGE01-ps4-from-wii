/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — adapter de size_t original (quovix), Apache-2.0 */
/* Overlay VFS do header de typedef da MSL que, fora de __MWERKS__, fixa
   `size_t` em `unsigned int` (ILP32, largura de ponteiro do PPC/Wii). No alvo
   LP64 (x86_64-pc-freebsd12-elf) o ponteiro tem 64 bits, entao `offsetof` e os
   casts ponteiro->size_t do save-data (BinaryDataChunkHolder) truncam e viram
   erro duro. Usar o size_t nativo do target (`__SIZE_TYPE__`, o mesmo tipo que
   `sizeof` ja' produz) alinha o typedef a' largura real do ponteiro.
   Nao e' copia de source do decomp: e' um typedef de plataforma, proprio. */
#pragma once

typedef __SIZE_TYPE__ size_t;
