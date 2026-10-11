/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "mtx.h"
#include <math.h>
#include <string.h>

/* ---- Matriz 3x4 ---------------------------------------------------------- */

void PSMTXIdentity(Mtx m) {
    m[0][0]=1; m[0][1]=0; m[0][2]=0; m[0][3]=0;
    m[1][0]=0; m[1][1]=1; m[1][2]=0; m[1][3]=0;
    m[2][0]=0; m[2][1]=0; m[2][2]=1; m[2][3]=0;
}

void PSMTXCopy(const Mtx src, Mtx dst) {
    if (src != dst) memcpy(dst, src, sizeof(Mtx));
}

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) {
    Mtx t;  /* temp p/ suportar aliasing (ab == a ou b) */
    for (int i = 0; i < 3; i++) {
        t[i][0] = a[i][0]*b[0][0] + a[i][1]*b[1][0] + a[i][2]*b[2][0];
        t[i][1] = a[i][0]*b[0][1] + a[i][1]*b[1][1] + a[i][2]*b[2][1];
        t[i][2] = a[i][0]*b[0][2] + a[i][1]*b[1][2] + a[i][2]*b[2][2];
        t[i][3] = a[i][0]*b[0][3] + a[i][1]*b[1][3] + a[i][2]*b[2][3] + a[i][3];
    }
    memcpy(ab, t, sizeof(Mtx));
}

unsigned PSMTXInverse(const Mtx src, Mtx inv) {
    /* inverte a parte 3x3 (rot/scale) e ajusta a translação */
    float det =
        src[0][0]*(src[1][1]*src[2][2] - src[1][2]*src[2][1])
      - src[0][1]*(src[1][0]*src[2][2] - src[1][2]*src[2][0])
      + src[0][2]*(src[1][0]*src[2][1] - src[1][1]*src[2][0]);
    if (det == 0.0f) return 0;
    float id = 1.0f / det;
    Mtx t;
    t[0][0] =  (src[1][1]*src[2][2] - src[1][2]*src[2][1]) * id;
    t[0][1] = -(src[0][1]*src[2][2] - src[0][2]*src[2][1]) * id;
    t[0][2] =  (src[0][1]*src[1][2] - src[0][2]*src[1][1]) * id;
    t[1][0] = -(src[1][0]*src[2][2] - src[1][2]*src[2][0]) * id;
    t[1][1] =  (src[0][0]*src[2][2] - src[0][2]*src[2][0]) * id;
    t[1][2] = -(src[0][0]*src[1][2] - src[0][2]*src[1][0]) * id;
    t[2][0] =  (src[1][0]*src[2][1] - src[1][1]*src[2][0]) * id;
    t[2][1] = -(src[0][0]*src[2][1] - src[0][1]*src[2][0]) * id;
    t[2][2] =  (src[0][0]*src[1][1] - src[0][1]*src[1][0]) * id;
    /* translação: -R^-1 * T */
    t[0][3] = -(t[0][0]*src[0][3] + t[0][1]*src[1][3] + t[0][2]*src[2][3]);
    t[1][3] = -(t[1][0]*src[0][3] + t[1][1]*src[1][3] + t[1][2]*src[2][3]);
    t[2][3] = -(t[2][0]*src[0][3] + t[2][1]*src[1][3] + t[2][2]*src[2][3]);
    memcpy(inv, t, sizeof(Mtx));
    return 1;
}

void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst) {
    float x = src->x, y = src->y, z = src->z;
    float rx = m[0][0]*x + m[0][1]*y + m[0][2]*z + m[0][3];
    float ry = m[1][0]*x + m[1][1]*y + m[1][2]*z + m[1][3];
    float rz = m[2][0]*x + m[2][1]*y + m[2][2]*z + m[2][3];
    dst->x = rx; dst->y = ry; dst->z = rz;
}

void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst) {
    float x = src->x, y = src->y, z = src->z;
    float rx = m[0][0]*x + m[0][1]*y + m[0][2]*z;
    float ry = m[1][0]*x + m[1][1]*y + m[1][2]*z;
    float rz = m[2][0]*x + m[2][1]*y + m[2][2]*z;
    dst->x = rx; dst->y = ry; dst->z = rz;
}

void PSMTXTrans(Mtx m, float x, float y, float z) {
    PSMTXIdentity(m);
    m[0][3] = x; m[1][3] = y; m[2][3] = z;
}

void PSMTXScale(Mtx m, float x, float y, float z) {
    m[0][0]=x; m[0][1]=0; m[0][2]=0; m[0][3]=0;
    m[1][0]=0; m[1][1]=y; m[1][2]=0; m[1][3]=0;
    m[2][0]=0; m[2][1]=0; m[2][2]=z; m[2][3]=0;
}

void PSMTXScaleApply(const Mtx src, Mtx dst, float x, float y, float z) {
    for (int i = 0; i < 3; i++) {
        dst[i][0] = src[i][0]*x;
        dst[i][1] = src[i][1]*y;
        dst[i][2] = src[i][2]*z;
        dst[i][3] = src[i][3];
    }
}

void PSMTXRotRad(Mtx m, char axis, float rad) {
    float s = sinf(rad), c = cosf(rad);
    PSMTXIdentity(m);
    switch (axis) {
    case 'x': case 'X':
        m[1][1]=c; m[1][2]=-s; m[2][1]=s; m[2][2]=c; break;
    case 'y': case 'Y':
        m[0][0]=c; m[0][2]=s; m[2][0]=-s; m[2][2]=c; break;
    case 'z': case 'Z':
        m[0][0]=c; m[0][1]=-s; m[1][0]=s; m[1][1]=c; break;
    }
}

void PSMTXRotAxisRad(Mtx dst, const Vec *axis, float rad) {
    /* Rodrigues: eixo normalizado */
    float ax = axis->x, ay = axis->y, az = axis->z;
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    if (mag != 0.0f) { ax/=mag; ay/=mag; az/=mag; }
    float s = sinf(rad), c = cosf(rad), t = 1.0f - c;
    dst[0][0] = t*ax*ax + c;    dst[0][1] = t*ax*ay - s*az; dst[0][2] = t*ax*az + s*ay; dst[0][3] = 0;
    dst[1][0] = t*ax*ay + s*az; dst[1][1] = t*ay*ay + c;    dst[1][2] = t*ay*az - s*ax; dst[1][3] = 0;
    dst[2][0] = t*ax*az - s*ay; dst[2][1] = t*ay*az + s*ax; dst[2][2] = t*az*az + c;    dst[2][3] = 0;
}

/* ---- Projeção ------------------------------------------------------------ */

void C_MTXOrtho(Mtx44 m, float t, float b, float l, float r, float n, float f) {
    float rl = r - l, tb = t - b, fn = f - n;
    m[0][0]=2.0f/rl; m[0][1]=0;       m[0][2]=0;        m[0][3]=-(r+l)/rl;
    m[1][0]=0;       m[1][1]=2.0f/tb; m[1][2]=0;        m[1][3]=-(t+b)/tb;
    m[2][0]=0;       m[2][1]=0;       m[2][2]=-1.0f/fn; m[2][3]=-f/fn;
    m[3][0]=0;       m[3][1]=0;       m[3][2]=0;        m[3][3]=1.0f;
}

void C_MTXLightOrtho(Mtx m, float t, float b, float l, float r,
                     float scaleS, float scaleT, float transS, float transT) {
    float rl = r - l, tb = t - b;
    m[0][0]=(2.0f*scaleS)/rl; m[0][1]=0;                m[0][2]=0; m[0][3]=((-(r+l)/rl)*scaleS)+transS;
    m[1][0]=0;                m[1][1]=(2.0f*scaleT)/tb; m[1][2]=0; m[1][3]=((-(t+b)/tb)*scaleT)+transT;
    m[2][0]=0;                m[2][1]=0;                m[2][2]=0; m[2][3]=1.0f;
}

void C_MTXLightPerspective(Mtx m, float fovY, float aspect,
                           float scaleS, float scaleT, float transS, float transT) {
    float angle = fovY * 0.5f * (float)(M_PI / 180.0);
    float cot = cosf(angle) / sinf(angle);
    m[0][0]=(cot/aspect)*scaleS; m[0][1]=0;          m[0][2]=-transS; m[0][3]=0;
    m[1][0]=0;                   m[1][1]=cot*scaleT; m[1][2]=-transT; m[1][3]=0;
    m[2][0]=0;                   m[2][1]=0;          m[2][2]=-1.0f;   m[2][3]=0;
}

/* ---- Vetor --------------------------------------------------------------- */

void PSVECCrossProduct(const Vec *a, const Vec *b, Vec *axb) {
    float x = a->y*b->z - a->z*b->y;
    float y = a->z*b->x - a->x*b->z;
    float z = a->x*b->y - a->y*b->x;
    axb->x = x; axb->y = y; axb->z = z;
}

float PSVECDotProduct(const Vec *a, const Vec *b) {
    return a->x*b->x + a->y*b->y + a->z*b->z;
}

float PSVECMag(const Vec *v) { return sqrtf(v->x*v->x + v->y*v->y + v->z*v->z); }
float C_VECMag(const Vec *v) { return PSVECMag(v); }

void PSVECNormalize(const Vec *src, Vec *dst) {
    float m = PSVECMag(src);
    if (m != 0.0f) { float im = 1.0f/m; dst->x=src->x*im; dst->y=src->y*im; dst->z=src->z*im; }
    else { dst->x=dst->y=dst->z=0; }
}

float PSVECDistance(const Vec *a, const Vec *b) {
    float dx=a->x-b->x, dy=a->y-b->y, dz=a->z-b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/* ---- Quaternion ---------------------------------------------------------- */

void PSQUATMultiply(const Quaternion *p, const Quaternion *q, Quaternion *pq) {
    float x = p->w*q->x + p->x*q->w + p->y*q->z - p->z*q->y;
    float y = p->w*q->y - p->x*q->z + p->y*q->w + p->z*q->x;
    float z = p->w*q->z + p->x*q->y - p->y*q->x + p->z*q->w;
    float w = p->w*q->w - p->x*q->x - p->y*q->y - p->z*q->z;
    pq->x=x; pq->y=y; pq->z=z; pq->w=w;
}

float PSQUATDotProduct(const Quaternion *p, const Quaternion *q) {
    return p->x*q->x + p->y*q->y + p->z*q->z + p->w*q->w;
}
