/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — teste numérico original (quovix), Apache-2.0 */
#include "../../runtime/compat/scalar_math.h"
#include <assert.h>
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static double double_from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int main(void) {
    assert(qvx_math_smoke_test() == 0);
    assert(double_bits(qvx_math_abs64(double_from_bits(UINT64_C(0x8000000000000000)))) == 0);
    assert(double_bits(qvx_math_abs64(double_from_bits(UINT64_C(0xfff8123456789abc)))) ==
           UINT64_C(0x7ff8123456789abc));
    assert(double_bits(qvx_math_abs64(double_from_bits(UINT64_C(0xfff0000000000000)))) ==
           UINT64_C(0x7ff0000000000000));
    assert(qvx_math_f32_to_u16(65534.75f) == 65534);
    assert(qvx_math_f32_to_u16(1.999f) == 1);

    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const int previous = fegetround();
    for (unsigned int mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        assert(fesetround(modes[mode]) == 0);
        for (unsigned int value = 0; value < 65536; ++value) {
            volatile float fractional = (float)value + 0.5f;
            assert(qvx_math_f32_to_u16(fractional) == value);
        }
    }
    assert(fesetround(previous) == 0);
    puts("math OK: 65536 u16, saturacao/truncamento, quatro modos de rounding, zeros/NaN/infinities");
    return 0;
}
