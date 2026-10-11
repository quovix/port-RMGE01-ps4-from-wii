/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "art_splash.h"
#include "yaz0.h"
#include "rarc.h"
#include "tpl.h"
#include <stdio.h>
#include <stdlib.h>

int qvx_art_load_tpl_from_arc(const char *arc_path, const char *tpl_name,
                              uint32_t *rgba, size_t rgba_cap,
                              uint32_t *out_w, uint32_t *out_h) {
    FILE *f = fopen(arc_path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return -1; }
    rewind(f);
    uint8_t *src = (uint8_t *)malloc((size_t)n);
    if (!src) { fclose(f); return -2; }
    if (fread(src, 1, (size_t)n, f) != (size_t)n) { free(src); fclose(f); return -1; }
    fclose(f);

    /* descomprime se for Yaz0, senão usa como está */
    uint8_t *buf; size_t blen; uint8_t *to_free2 = 0;
    uint32_t u = qvx_yaz0_uncompressed_size(src, (size_t)n);
    if (u) {
        buf = (uint8_t *)malloc(u);
        if (!buf) { free(src); return -2; }
        to_free2 = buf;
        blen = qvx_yaz0_decompress(src, (size_t)n, buf, u);
        if (blen == 0) { free(buf); free(src); return -3; }
    } else { buf = src; blen = (size_t)n; }

    int rc = -4;
    qvx_rarc r;
    if (qvx_rarc_open(&r, buf, blen) == 0) {
        uint32_t tsz = 0;
        const uint8_t *tpl = qvx_rarc_find(&r, tpl_name, &tsz);
        if (tpl) {
            uint32_t w, h, fmt;
            if (qvx_tpl_info(tpl, tsz, &w, &h, &fmt) == 0 &&
                (size_t)w * h <= rgba_cap) {
                if (qvx_tpl_decode(tpl, tsz, rgba, rgba_cap) == 0) {
                    if (out_w) *out_w = w;
                    if (out_h) *out_h = h;
                    rc = 0;
                } else rc = -6;
            } else rc = -5;
        }
    }
    if (to_free2) free(to_free2);
    free(src);
    return rc;
}
