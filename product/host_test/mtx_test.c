/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Host test do backend de math (PSMTX/PSVEC/PSQUAT/C_MTX). Valida resultados
   conhecidos de álgebra linear. */
#include "../../runtime/platform/orbis/mtx.h"
#include <math.h>
#include <stdio.h>

static int fails = 0;
static void chk(const char *name, int ok) {
    printf("  %-24s %s\n", name, ok ? "OK" : "FALHA");
    if (!ok) fails++;
}
static int close1(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main(void) {
    /* identity * v = v */
    Mtx id; PSMTXIdentity(id);
    Vec v = {1,2,3}, r;
    PSMTXMultVec(id, &v, &r);
    chk("identity*v", close1(r.x,1) && close1(r.y,2) && close1(r.z,3));

    /* translação */
    Mtx tr; PSMTXTrans(tr, 10, 20, 30);
    PSMTXMultVec(tr, &v, &r);
    chk("trans*v", close1(r.x,11) && close1(r.y,22) && close1(r.z,33));
    PSMTXMultVecSR(tr, &v, &r);   /* SR ignora translação */
    chk("transSR*v", close1(r.x,1) && close1(r.y,2) && close1(r.z,3));

    /* concat: T(10,0,0) * S(2,2,2), aplicado a (1,1,1) => escala depois translada */
    Mtx s, c; PSMTXScale(s, 2,2,2);
    PSMTXConcat(tr, s, c);        /* c = tr * s */
    Vec one = {1,1,1};
    PSMTXMultVec(c, &one, &r);
    chk("concat T*S", close1(r.x,12) && close1(r.y,22) && close1(r.z,32));

    /* rotação Z de 90° leva (1,0,0) -> (0,1,0) */
    Mtx rz; PSMTXRotRad(rz, 'z', (float)(M_PI/2));
    Vec ex = {1,0,0}; PSMTXMultVec(rz, &ex, &r);
    chk("rotZ90 *x", close1(r.x,0) && close1(r.y,1) && close1(r.z,0));

    /* inverse: M * M^-1 = I (testa via concat) */
    Mtx inv, prod; unsigned ok = PSMTXInverse(c, inv);
    PSMTXConcat(c, inv, prod);
    chk("inverse (M*M^-1=I)", ok && close1(prod[0][0],1) && close1(prod[1][1],1)
        && close1(prod[2][2],1) && close1(prod[0][3],0) && close1(prod[1][3],0));

    /* cross / dot / mag / normalize */
    Vec a={1,0,0}, b={0,1,0}, axb;
    PSVECCrossProduct(&a,&b,&axb);
    chk("cross x,y=z", close1(axb.x,0)&&close1(axb.y,0)&&close1(axb.z,1));
    chk("dot", close1(PSVECDotProduct(&a,&a),1));
    Vec u={3,4,0}, un; PSVECNormalize(&u,&un);
    chk("mag(3,4,0)=5", close1(PSVECMag(&u),5));
    chk("normalize", close1(PSVECMag(&un),1));
    chk("distance", close1(PSVECDistance(&a,&b), sqrtf(2)));

    /* quaternion identidade: (0,0,0,1)*(0,0,0,1) = (0,0,0,1) */
    Quaternion qi={0,0,0,1}, qq; PSQUATMultiply(&qi,&qi,&qq);
    chk("quat id*id", close1(qq.w,1)&&close1(qq.x,0));
    chk("quat dot", close1(PSQUATDotProduct(&qi,&qi),1));

    /* ortho: centro do volume -> origem NDC */
    Mtx44 o; C_MTXOrtho(o, 1,-1,-1,1,0,1);   /* t,b,l,r,n,f */
    chk("ortho[0][0]=1", close1(o[0][0],1) && close1(o[1][1],1));

    printf(fails ? "FALHAS: %d\n" : "todos os testes de math passaram\n", fails);
    return fails ? 1 : 0;
}
