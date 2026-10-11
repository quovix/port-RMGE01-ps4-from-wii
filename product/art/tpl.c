/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "tpl.h"

static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|p[1]<<16|p[2]<<8|p[3];}
static uint16_t be16(const uint8_t *p){return (uint16_t)(p[0]<<8|p[1]);}
#define E8(n)  ((uint32_t)(n)*0x11u)          /* 4-bit -> 8-bit */
#define ARGB(a,r,g,b) ((uint32_t)(a)<<24|(uint32_t)(r)<<16|(uint32_t)(g)<<8|(uint32_t)(b))

/* Localiza o header da 1ª textura. Retorna ponteiro pro header (0x__) ou NULL. */
static const uint8_t *first_tex_header(const uint8_t *d, size_t n) {
    if (n < 0x0C) return 0;
    if (be32(d) != 0x0020AF30) return 0;
    uint32_t num = be32(d + 0x04);
    uint32_t tbl = be32(d + 0x08);
    if (num == 0 || tbl + 8 > n) return 0;
    uint32_t th = be32(d + tbl + 0x00);   /* texture header offset */
    if (th + 0x20 > n) return 0;
    return d + th;
}

int qvx_tpl_info(const uint8_t *d, size_t n, uint32_t *w, uint32_t *h, uint32_t *fmt) {
    const uint8_t *t = first_tex_header(d, n);
    if (!t) return -1;
    if (h)   *h   = be16(t + 0x00);
    if (w)   *w   = be16(t + 0x02);
    if (fmt) *fmt = be32(t + 0x04);
    return 0;
}

static void put(uint32_t *o, size_t cap, uint32_t W, uint32_t H,
                uint32_t x, uint32_t y, uint32_t px) {
    if (x < W && y < H && (size_t)y * W + x < cap) o[(size_t)y * W + x] = px;
}

static uint32_t cvt_rgb565(uint16_t v) {
    uint32_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
    return ARGB(255, r * 255 / 31, g * 255 / 63, b * 255 / 31);
}
static uint32_t cvt_rgb5a3(uint16_t v) {
    if (v & 0x8000) {   /* opaco, RGB555 */
        uint32_t r = (v >> 10) & 0x1F, g = (v >> 5) & 0x1F, b = v & 0x1F;
        return ARGB(255, r * 255 / 31, g * 255 / 31, b * 255 / 31);
    }
    uint32_t a = (v >> 12) & 0x7, r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
    return ARGB(a * 255 / 7, E8(r), E8(g), E8(b));
}

int qvx_tpl_decode(const uint8_t *d, size_t n, uint32_t *o, size_t cap) {
    const uint8_t *t = first_tex_header(d, n);
    if (!t) return -1;
    uint32_t H = be16(t + 0x00), W = be16(t + 0x02), fmt = be32(t + 0x04);
    uint32_t doff = be32(t + 0x08);
    if ((size_t)doff > n) return -2;
    const uint8_t *s = d + doff;
    size_t avail = n - doff;
    size_t p = 0;   /* cursor em s */

#define NEED(k) do{ if (p + (k) > avail) return -3; }while(0)

    switch (fmt) {
    case QVX_TPL_I4:
        for (uint32_t ty=0; ty<H; ty+=8) for (uint32_t tx=0; tx<W; tx+=8) {
            NEED(32);
            for (uint32_t py=0; py<8; py++) for (uint32_t px=0; px<8; px++) {
                uint8_t b = s[p + (py*8+px)/2];
                uint8_t i = (px & 1) ? (b & 0xF) : (b >> 4);
                put(o,cap,W,H, tx+px, ty+py, ARGB(255,E8(i),E8(i),E8(i)));
            }
            p += 32;
        } break;
    case QVX_TPL_I8:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=8) {
            NEED(32);
            for (uint32_t py=0; py<4; py++) for (uint32_t px=0; px<8; px++) {
                uint8_t i = s[p + py*8+px];
                put(o,cap,W,H, tx+px, ty+py, ARGB(255,i,i,i));
            }
            p += 32;
        } break;
    case QVX_TPL_IA4:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=8) {
            NEED(32);
            for (uint32_t py=0; py<4; py++) for (uint32_t px=0; px<8; px++) {
                uint8_t b = s[p + py*8+px];
                uint8_t a = E8(b >> 4), i = E8(b & 0xF);
                put(o,cap,W,H, tx+px, ty+py, ARGB(a,i,i,i));
            }
            p += 32;
        } break;
    case QVX_TPL_IA8:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=4) {
            NEED(32);
            for (uint32_t py=0; py<4; py++) for (uint32_t px=0; px<4; px++) {
                uint16_t v = be16(s + p + (py*4+px)*2);
                uint8_t i = v >> 8, a = v & 0xFF;
                put(o,cap,W,H, tx+px, ty+py, ARGB(a,i,i,i));
            }
            p += 32;
        } break;
    case QVX_TPL_RGB565:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=4) {
            NEED(32);
            for (uint32_t py=0; py<4; py++) for (uint32_t px=0; px<4; px++)
                put(o,cap,W,H, tx+px, ty+py, cvt_rgb565(be16(s+p+(py*4+px)*2)));
            p += 32;
        } break;
    case QVX_TPL_RGB5A3:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=4) {
            NEED(32);
            for (uint32_t py=0; py<4; py++) for (uint32_t px=0; px<4; px++)
                put(o,cap,W,H, tx+px, ty+py, cvt_rgb5a3(be16(s+p+(py*4+px)*2)));
            p += 32;
        } break;
    case QVX_TPL_RGBA8:
        for (uint32_t ty=0; ty<H; ty+=4) for (uint32_t tx=0; tx<W; tx+=4) {
            NEED(64);
            for (uint32_t k=0; k<16; k++) {
                uint8_t a = s[p + k*2], r = s[p + k*2 + 1];
                uint8_t g = s[p + 32 + k*2], b = s[p + 32 + k*2 + 1];
                put(o,cap,W,H, tx + (k&3), ty + (k>>2), ARGB(a,r,g,b));
            }
            p += 64;
        } break;
    case QVX_TPL_CMPR:
        for (uint32_t ty=0; ty<H; ty+=8) for (uint32_t tx=0; tx<W; tx+=8) {
            for (uint32_t sb=0; sb<4; sb++) {       /* 2x2 sub-blocos DXT1 4x4 */
                NEED(8);
                uint16_t c0 = be16(s+p), c1 = be16(s+p+2);
                uint32_t col[4]; col[0]=cvt_rgb565(c0); col[1]=cvt_rgb565(c1);
                uint8_t r0=(col[0]>>16)&0xFF,g0=(col[0]>>8)&0xFF,b0=col[0]&0xFF;
                uint8_t r1=(col[1]>>16)&0xFF,g1=(col[1]>>8)&0xFF,b1=col[1]&0xFF;
                if (c0 > c1) {
                    col[2]=ARGB(255,(2*r0+r1)/3,(2*g0+g1)/3,(2*b0+b1)/3);
                    col[3]=ARGB(255,(r0+2*r1)/3,(g0+2*g1)/3,(b0+2*b1)/3);
                } else {
                    col[2]=ARGB(255,(r0+r1)/2,(g0+g1)/2,(b0+b1)/2);
                    col[3]=ARGB(0,0,0,0);           /* transparente */
                }
                uint32_t sx = tx + (sb&1)*4, sy = ty + (sb>>1)*4;
                for (uint32_t py=0; py<4; py++) {
                    uint8_t idx = s[p+4+py];        /* 4 pixels, 2 bits cada, MSB primeiro */
                    for (uint32_t px=0; px<4; px++) {
                        uint32_t sel = (idx >> (6 - px*2)) & 3;
                        put(o,cap,W,H, sx+px, sy+py, col[sel]);
                    }
                }
                p += 8;
            }
        } break;
    default:
        return -10;   /* formato não suportado (ex. CI4/CI8 c/ paleta) */
    }
    return 0;
}
