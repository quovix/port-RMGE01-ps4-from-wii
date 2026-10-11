/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Backend de matemática do GC/Wii (paired-single) — CPU puro, código próprio.
   Implementa os nomes da API (PSMTX, PSVEC, PSQUAT, C_MTX) com álgebra linear
   padrão. Tipos conforme o SDK: Mtx 3x4 row-major, Mtx44 4x4, Vec/Quaternion. */
#ifndef QVX_MTX_H
#define QVX_MTX_H

typedef float Mtx[3][4];
typedef float Mtx44[4][4];
typedef struct { float x, y, z; }    Vec;
typedef struct { float x, y, z, w; } Quaternion;

#ifdef __cplusplus
extern "C" {
#endif

/* Matriz 3x4 (afim) */
void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);        /* ab = a * b */
unsigned PSMTXInverse(const Mtx src, Mtx inv);             /* 0 se singular */
void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst);  /* com translação */
void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst);/* só scale+rot */
void PSMTXTrans(Mtx m, float x, float y, float z);
void PSMTXScale(Mtx m, float x, float y, float z);
void PSMTXScaleApply(const Mtx src, Mtx dst, float x, float y, float z);
void PSMTXRotRad(Mtx m, char axis, float rad);
void PSMTXRotAxisRad(Mtx dst, const Vec *axis, float rad);

/* Projeção / texturas projetadas */
void C_MTXOrtho(Mtx44 m, float t, float b, float l, float r, float n, float f);
void C_MTXLightOrtho(Mtx m, float t, float b, float l, float r,
                     float scaleS, float scaleT, float transS, float transT);
void C_MTXLightPerspective(Mtx m, float fovY, float aspect,
                           float scaleS, float scaleT, float transS, float transT);

/* Vetor */
void  PSVECCrossProduct(const Vec *a, const Vec *b, Vec *axb);
float PSVECDotProduct(const Vec *a, const Vec *b);
float PSVECMag(const Vec *v);
float C_VECMag(const Vec *v);
void  PSVECNormalize(const Vec *src, Vec *dst);
float PSVECDistance(const Vec *a, const Vec *b);

/* Quaternion */
void  PSQUATMultiply(const Quaternion *p, const Quaternion *q, Quaternion *pq);
float PSQUATDotProduct(const Quaternion *p, const Quaternion *q);

#ifdef __cplusplus
}
#endif
#endif
