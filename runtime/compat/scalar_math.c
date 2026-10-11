/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — operações nativas originais (quovix), Apache-2.0 */
#include "scalar_math.h"

typedef char qvx_require_u16[(sizeof(unsigned short) == 2) ? 1 : -1];
typedef char qvx_require_f32[(sizeof(float) == 4 && __FLT_MANT_DIG__ == 24) ? 1 : -1];
typedef char qvx_require_f64[(sizeof(double) == 8 && __DBL_MANT_DIG__ == 53) ? 1 : -1];

float qvx_math_u16_to_f32(unsigned short value) { return (float)value; }

unsigned short qvx_math_f32_to_u16(float value) {
    /* Comparações antes do cast evitam UB em NaN, infinities e overflow. */
    if (!(value > 0.0f)) return 0;
    if (value >= 65535.0f) return 65535;
    return (unsigned short)value;
}

float qvx_math_abs32(float value) { return __builtin_fabsf(value); }
double qvx_math_abs64(double value) { return __builtin_fabs(value); }

float qvx_math_select32(float condition, float nonnegative, float negative) {
    return condition >= 0.0f ? nonnegative : negative;
}

float __fabsf(float value) { return qvx_math_abs32(value); }
double __fabs(double value) { return qvx_math_abs64(value); }
