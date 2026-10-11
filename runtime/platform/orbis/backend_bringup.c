/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "backend_bringup.h"
#include "mtx.h"
#include "gx_vi.h"
#include "../../video/text.h"
#include <math.h>
#include <stdio.h>

/* Contador de checagens do selftest MTX. */
static int g_pass, g_total;
static int close1(float a, float b) { return fabsf(a - b) < 1e-4f; }
static void chk(int ok) { g_total++; if (ok) g_pass++; }

/* Selftest do backend de math no hardware (mesmos casos do host test). CPU
   puro — prova PSMTX/PSVEC/PSQUAT linkados e corretos no Orbis. */
static void mtx_selftest(void) {
    Mtx id; PSMTXIdentity(id);
    Vec v = {1,2,3}, r;
    PSMTXMultVec(id, &v, &r);
    chk(close1(r.x,1) && close1(r.y,2) && close1(r.z,3));

    Mtx tr; PSMTXTrans(tr, 10, 20, 30);
    PSMTXMultVec(tr, &v, &r);
    chk(close1(r.x,11) && close1(r.y,22) && close1(r.z,33));
    PSMTXMultVecSR(tr, &v, &r);
    chk(close1(r.x,1) && close1(r.y,2) && close1(r.z,3));

    Mtx s, c; PSMTXScale(s, 2,2,2);
    PSMTXConcat(tr, s, c);                 /* c = tr * s */
    Vec one = {1,1,1};
    PSMTXMultVec(c, &one, &r);
    chk(close1(r.x,12) && close1(r.y,22) && close1(r.z,32));

    Mtx rz; PSMTXRotRad(rz, 'z', (float)(M_PI/2));
    Vec ex = {1,0,0}; PSMTXMultVec(rz, &ex, &r);
    chk(close1(r.x,0) && close1(r.y,1) && close1(r.z,0));

    Mtx inv, prod; unsigned ok = PSMTXInverse(c, inv);
    PSMTXConcat(c, inv, prod);
    chk(ok && close1(prod[0][0],1) && close1(prod[1][1],1) &&
        close1(prod[2][2],1) && close1(prod[0][3],0) && close1(prod[1][3],0));

    Vec a={1,0,0}, b={0,1,0}, axb;
    PSVECCrossProduct(&a,&b,&axb);
    chk(close1(axb.x,0) && close1(axb.y,0) && close1(axb.z,1));
    chk(close1(PSVECDotProduct(&a,&b), 0));
    Vec m3 = {3,4,0};
    chk(close1(PSVECMag(&m3), 5));
    Vec nrm; PSVECNormalize(&m3, &nrm);
    chk(close1(PSVECMag(&nrm), 1));

    Quaternion p={0,0,0,1}, q={0,0,0,1}, pq;   /* identidade * identidade */
    PSQUATMultiply(&p,&q,&pq);
    chk(close1(pq.w,1) && close1(pq.x,0) && close1(pq.y,0) && close1(pq.z,0));
}

int qvx_backend_bringup(void) {
    char line[64];

    /* 1) math backend ----------------------------------------------------- */
    qvx_log_line("backend: selftest MTX (PSMTX/PSVEC/PSQUAT)...");
    g_pass = g_total = 0;
    mtx_selftest();
    snprintf(line, sizeof line, "backend MTX: %d/%d casos OK", g_pass, g_total);
    qvx_log_line(line);

    /* 2) GX/VI bring-up (apoiado no SceVideoOut, já aberto pela casca) ----- */
    qvx_log_line("backend: init GX/VI (VIInit/GXInit/clear)...");
    VIInit();
    GXFifoObj *fifo = GXInit((void *)0, 0u);
    GXColor clear = { 10, 16, 32, 255 };      /* azul-petroleo (casa com a casca) */
    GXSetCopyClear(clear, 0);
    snprintf(line, sizeof line, "backend GX/VI: online (fifo=%p)", (void *)fifo);
    qvx_log_line(line);

    return g_total - g_pass;                  /* 0 = tudo passou */
}
