/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — adapter escalar nw4r original (quovix), Apache-2.0 */
#ifndef QVX_NW4R_ARITHMETIC_H
#define QVX_NW4R_ARITHMETIC_H
#include <quovix/scalar_math.h>
#include <revolution/types.h>

namespace nw4r { namespace math {
inline f32 U16ToF32(u16 value) { return qvx_math_u16_to_f32(value); }
inline u16 F32ToU16(f32 value) { return qvx_math_f32_to_u16(value); }
inline f32 FAbs(f32 value) { return qvx_math_abs32(value); }
inline f32 FSelect(f32 condition, f32 nonnegative, f32 negative) {
    return qvx_math_select32(condition, nonnegative, negative);
}
} }
#endif
