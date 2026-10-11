/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "video_out.h"
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>

#define QVX_W      1920
#define QVX_H      1080
#define QVX_NBUF   2
#define QVX_ALIGN  0x20000              /* 128 KB, alinhamento de display buffer */

static int        g_video = -1;
static uint32_t  *g_fb[QVX_NBUF];
static int        g_active;
static off_t      g_dm_off;
static size_t     g_dm_size;

int qvx_video_width(void)  { return QVX_W; }
int qvx_video_height(void) { return QVX_H; }

int qvx_video_init(void) {
    const size_t fb_bytes = (size_t)QVX_W * QVX_H * 4;
    size_t total = fb_bytes * QVX_NBUF;
    total = (total + QVX_ALIGN - 1) / QVX_ALIGN * QVX_ALIGN;

    g_video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
    if (g_video < 0) return -1;

    /* Direct memory tipo 3 (WB_ONION): coerente CPU/GPU, bom para fill por CPU. */
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(),
                                      total, QVX_ALIGN, 3, &g_dm_off) < 0)
        return -2;
    g_dm_size = total;

    void *base = 0;
    if (sceKernelMapDirectMemory(&base, total, 0x33 /* RW|GPU */, 0,
                                 g_dm_off, QVX_ALIGN) < 0)
        return -3;

    for (int i = 0; i < QVX_NBUF; i++)
        g_fb[i] = (uint32_t *)((char *)base + (size_t)i * fb_bytes);

    OrbisVideoOutBufferAttribute attr;
    /* formato 0x80000000 (A8R8G8B8 SRGB), tiling linear, aspect auto, pitch=W. */
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, QVX_W, QVX_H, QVX_W);
    if (sceVideoOutRegisterBuffers(g_video, 0, (void * const *)g_fb, QVX_NBUF, &attr) != 0)
        return -4;

    sceVideoOutSetFlipRate(g_video, 0);
    g_active = 0;
    return 0;
}

uint32_t *qvx_video_backbuffer(void) {
    return (g_video < 0) ? 0 : g_fb[g_active];
}

void qvx_video_clear(uint32_t argb) {
    if (g_video < 0) return;
    uint32_t *p = g_fb[g_active];
    const size_t n = (size_t)QVX_W * QVX_H;
    for (size_t i = 0; i < n; i++) p[i] = argb;
}

void qvx_video_present(void) {
    if (g_video < 0) return;
    sceVideoOutSubmitFlip(g_video, g_active, ORBIS_VIDEO_OUT_FLIP_VSYNC, 0);
    sceVideoOutWaitVblank();
    g_active ^= 1;
}

void qvx_video_blit_rgba(const uint32_t *src, int sw, int sh, int dx, int dy) {
    if (g_video < 0 || !src) return;
    uint32_t *fb = g_fb[g_active];
    for (int y = 0; y < sh; y++) {
        int oy = dy + y;
        if (oy < 0 || oy >= QVX_H) continue;
        for (int x = 0; x < sw; x++) {
            int ox = dx + x;
            if (ox < 0 || ox >= QVX_W) continue;
            uint32_t sp = src[(size_t)y * sw + x];
            uint32_t a = sp >> 24;
            if (a == 0) continue;
            if (a == 0xFF) { fb[(size_t)oy * QVX_W + ox] = sp; continue; }
            /* composição alpha sobre o destino (0xAARRGGBB) */
            uint32_t dp = fb[(size_t)oy * QVX_W + ox];
            uint32_t sr=(sp>>16)&0xFF, sg=(sp>>8)&0xFF, sb=sp&0xFF;
            uint32_t dr=(dp>>16)&0xFF, dg=(dp>>8)&0xFF, db=dp&0xFF;
            uint32_t ia = 255 - a;
            uint32_t rr=(sr*a + dr*ia)/255, rg=(sg*a + dg*ia)/255, rb=(sb*a + db*ia)/255;
            fb[(size_t)oy * QVX_W + ox] = 0xFF000000u | rr<<16 | rg<<8 | rb;
        }
    }
}

void qvx_video_blit_centered(const uint32_t *src, int sw, int sh) {
    qvx_video_blit_rgba(src, sw, sh, (QVX_W - sw) / 2, (QVX_H - sh) / 2);
}
