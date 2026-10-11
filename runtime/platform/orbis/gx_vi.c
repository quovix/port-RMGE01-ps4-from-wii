/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "gx_vi.h"
#include "../../video/video_out.h"

/* Estado mínimo do backend. */
static int      g_inited;
static uint32_t g_clear_argb = 0xFF000000u;   /* preto opaco por padrão */
static int      g_fifo_dummy;                 /* handle opaco p/ o motor */

static void ensure_video(void) {
    if (!g_inited) { qvx_video_init(); g_inited = 1; }
}

/* --- GX ------------------------------------------------------------------ */

GXFifoObj *GXInit(void *base, uint32_t size) {
    (void)base; (void)size;
    ensure_video();
    return (GXFifoObj *)&g_fifo_dummy;
}

void GXSetCopyClear(GXColor clear, uint32_t zclear) {
    (void)zclear;
    g_clear_argb = ((uint32_t)clear.a << 24) | ((uint32_t)clear.r << 16) |
                   ((uint32_t)clear.g << 8)  |  (uint32_t)clear.b;
}

/* Copia o EFB para o framebuffer de saída, limpando. Sem backend de geometria
   (GNM/TEV ainda stubado), só honramos a cor de clear — a geometria desenhada
   pelo jogo ainda não aparece. É o primeiro pixel GX-backed: tela na cor que o
   jogo pediu. */
void GXCopyDisp(void *dest, GXBool clear) {
    (void)dest; (void)clear;
    ensure_video();
    qvx_video_clear(g_clear_argb);
}

void GXDrawDone(void)  { /* sem fila real por enquanto */ }
void GXFlush(void)     { }
void GXAbortFrame(void){ }
void GXSetViewport(float x, float y, float w, float h, float zn, float zf) {
    (void)x; (void)y; (void)w; (void)h; (void)zn; (void)zf;
}
void GXSetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    (void)x; (void)y; (void)w; (void)h;
}

/* --- VI ------------------------------------------------------------------ */

void VIInit(void)                     { ensure_video(); }
void VIConfigure(const void *rm)      { (void)rm; ensure_video(); }
void VISetNextFrameBuffer(void *fb)   { (void)fb; /* XFB do jogo ignorado por ora */ }
void VIFlush(void)                    { ensure_video(); qvx_video_present(); }
void VIWaitForRetrace(void)           { ensure_video(); qvx_video_present(); }
