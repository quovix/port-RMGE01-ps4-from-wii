/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — shim de declaracoes de helpers do MSL runtime (quovix), Apache-2.0.
   Declara os helpers de runtime (__div2u, __shr2u, cvt de double...) que o
   overlay do libs/Runtime/include/runtime.h expoe; corpo vem da libc/soft-float. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


u32 __cvt_fp2unsigned(double);
void __div2u(void);
void __div2i(void);
void __mod2u(void);
void __mod2i(void);
void __shl2i(void);
void __shr2u(void);
u64 __cvt_dbl_usll(double);
void __cvt_dbl_ull(void);

#ifdef __cplusplus
}
#endif
