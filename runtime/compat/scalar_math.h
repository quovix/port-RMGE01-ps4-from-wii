/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — matemática escalar original (quovix), Apache-2.0 */
#ifndef QVX_SCALAR_MATH_H
#define QVX_SCALAR_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Perfil nativo: u16 é inteiro de 16 bits e float é IEEE binary32.
   Valores já decodificados em memória; não faz leitura big-endian do disco. */
float qvx_math_u16_to_f32(unsigned short value);
/* Trunca valores dentro da faixa e satura em [0, 65535]. NaN vira zero por
   política explícita do porte; equivalência desse caso no Wii é pendente. */
unsigned short qvx_math_f32_to_u16(float value);
float qvx_math_abs32(float value);
double qvx_math_abs64(double value);
/* Zero positivo ou negativo escolhe nonnegative; NaN escolhe negative.
   Retorna o operando escolhido; não interpola nem multiplica por uma máscara. */
float qvx_math_select32(float condition, float nonnegative, float negative);

/* Aliases para callers do compiler original; implementações próprias. */
float __fabsf(float value);
double __fabs(double value);

/* Fixtures originais, usados no host e no startup da casca. */
int qvx_math_smoke_test(void);

#ifdef __cplusplus
}
#endif
#endif
