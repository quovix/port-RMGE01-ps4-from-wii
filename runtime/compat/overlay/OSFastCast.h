/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — adapter FastCast original (quovix), Apache-2.0 */
#ifndef QVX_OS_FAST_CAST_H
#define QVX_OS_FAST_CAST_H
#include <quovix/scalar_math.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif
/* A CPU nativa não usa GQR; este perfil cobre somente u16 e escala zero. */
static inline void OSInitFastCast(void) {}
static inline u16 __OSf32tou16(f32 value) { return qvx_math_f32_to_u16(value); }
static inline void OSf32tou16(const f32* input, volatile u16* output) {
    *output = qvx_math_f32_to_u16(*input);
}
static inline void OSu16tof32(const u16* input, volatile f32* output) {
    *output = qvx_math_u16_to_f32(*input);
}
#ifdef __cplusplus
}
#endif
#endif
