/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "text.h"
#include "video_out.h"
#include "font8x8.h"
#include <string.h>

void qvx_text_draw(int x, int y, const char *s, uint32_t color, int scale) {
    uint32_t *fb = qvx_video_backbuffer();
    if (!fb || scale < 1) return;
    int W = qvx_video_width(), H = qvx_video_height();
    int cx = x;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') { cx = x; y += 8 * scale + 2; continue; }
        if (c >= 'a' && c <= 'z') c -= 0x20;         /* fonte é uppercase */
        int gi = (c >= 0x20 && c <= 0x5F) ? (c - 0x20) : 0;
        const uint8_t *g = qvx_font8x8[gi];
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            for (int col = 0; col < 8; col++) {
                if (!(bits & (0x80 >> col))) continue;
                for (int sy = 0; sy < scale; sy++) for (int sx = 0; sx < scale; sx++) {
                    int px = cx + col*scale + sx, py = y + row*scale + sy;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        fb[(size_t)py * W + px] = color;
                }
            }
        }
        cx += 8 * scale + scale;   /* 1px de espaço proporcional à escala */
    }
}

/* --- log on-screen ------------------------------------------------------- */
#define QVX_LOG_LINES 28
#define QVX_LOG_COLS  96
static char g_log[QVX_LOG_LINES][QVX_LOG_COLS + 1];
static int  g_count;

void qvx_log_line(const char *s) {
    if (g_count >= QVX_LOG_LINES) {               /* scroll: descarta a 1ª */
        for (int i = 1; i < QVX_LOG_LINES; i++)
            memcpy(g_log[i-1], g_log[i], QVX_LOG_COLS + 1);
        g_count = QVX_LOG_LINES - 1;
    }
    strncpy(g_log[g_count], s, QVX_LOG_COLS);
    g_log[g_count][QVX_LOG_COLS] = 0;
    g_count++;
}

void qvx_log_render(int x, int y, uint32_t color, int scale) {
    int lh = 8 * scale + 2;
    for (int i = 0; i < g_count; i++)
        qvx_text_draw(x, y + i * lh, g_log[i], color, scale);
}
