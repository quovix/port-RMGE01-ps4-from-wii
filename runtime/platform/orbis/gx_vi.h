/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Backend Orbis — primeiro slice: VI (video interface) + o caminho de
   boot/clear/present do GX, apoiados no nosso renderer SceVideoOut.
   Implementa os NOMES da API C do Wii (GXInit, VIInit, ...) com código próprio.
   NÃO implementa desenho de geometria (GXBegin/TEV/display lists) — isso é o
   GNM, research-grade, e segue stubado. Tipos mínimos definidos aqui; no link
   com o motor só importam os símbolos e a ABI (funções C). */
#ifndef QVX_GX_VI_H
#define QVX_GX_VI_H

#include <stdint.h>

/* 4 bytes, passado por valor em registrador (ABI SysV) — casa com o GXColor
   do SDK ({u8 r,g,b,a}). */
typedef struct { uint8_t r, g, b, a; } GXColor;
typedef uint8_t GXBool;
typedef void GXFifoObj;

#ifdef __cplusplus
extern "C" {
#endif

/* --- GX: boot/clear/present (resto do GX fica stubado) --- */
GXFifoObj *GXInit(void *base, uint32_t size);
void GXSetCopyClear(GXColor clear, uint32_t zclear);
void GXCopyDisp(void *dest, GXBool clear);
void GXDrawDone(void);
void GXFlush(void);
void GXSetViewport(float x, float y, float w, float h, float zn, float zf);
void GXSetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void GXAbortFrame(void);

/* --- VI: apresentação, apoiada no SceVideoOut --- */
void VIInit(void);
void VIConfigure(const void *rm);
void VISetNextFrameBuffer(void *fb);
void VIFlush(void);
void VIWaitForRetrace(void);

#ifdef __cplusplus
}
#endif
#endif
