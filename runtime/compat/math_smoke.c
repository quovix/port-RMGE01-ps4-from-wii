/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — fixtures numéricos originais (quovix), Apache-2.0 */
#include "scalar_math.h"
#include <stdint.h>
#include <string.h>

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int qvx_math_smoke_test(void) {
    /* Todos os inteiros u16 são exatamente representáveis em binary32. */
    for (unsigned int value = 0; value < 65536; ++value) {
        float converted = qvx_math_u16_to_f32((unsigned short)value);
        if (converted != (float)value || qvx_math_f32_to_u16(converted) != value) return -1;
        if (qvx_math_f32_to_u16(converted + 0.75f) != value) return -2;
    }
    const float negative_zero = float_from_bits(UINT32_C(0x80000000));
    const float nan = float_from_bits(UINT32_C(0x7fc12345));
    const float positive_infinity = float_from_bits(UINT32_C(0x7f800000));
    const float negative_infinity = float_from_bits(UINT32_C(0xff800000));
    if (qvx_math_f32_to_u16(-1.0f) != 0 || qvx_math_f32_to_u16(0.99f) != 0 ||
        qvx_math_f32_to_u16(65536.0f) != 65535 || qvx_math_f32_to_u16(nan) != 0 ||
        qvx_math_f32_to_u16(positive_infinity) != 65535 ||
        qvx_math_f32_to_u16(negative_infinity) != 0) return -3;
    if (float_bits(qvx_math_abs32(negative_zero)) != 0 ||
        qvx_math_abs64(-0.0) != 0.0 || qvx_math_abs32(-3.5f) != 3.5f ||
        qvx_math_abs64(-7.25) != 7.25) return -4;
    if (qvx_math_select32(negative_zero, 11.0f, 22.0f) != 11.0f ||
        qvx_math_select32(nan, 11.0f, 22.0f) != 22.0f ||
        qvx_math_select32(-1.0f, 11.0f, 22.0f) != 22.0f ||
        float_bits(qvx_math_select32(1.0f, nan, 0.0f)) != UINT32_C(0x7fc12345) ||
        float_bits(qvx_math_select32(-1.0f, 0.0f, negative_zero)) != UINT32_C(0x80000000)) return -5;
    if (__fabsf(-2.0f) != 2.0f || __fabs(-4.0) != 4.0) return -6;
    return 0;
}
