/* @orig none — shim de compat (quovix), Apache-2.0.
 *
 * Overlay do <cstdarg> do MSL_C. O original usa os intrínsecos de varargs do
 * CodeWarrior (__builtin_va_info / __va_arg(void*,int)), que o clang não tem, e
 * por isso derruba a compilação de QUALQUER TU com função varargs (ex.:
 * StringUtil.cpp:addNumberFontTag -> vswprintf). Aqui mapeamos va_start/va_arg/
 * va_end/va_copy para os builtins do clang (ABI SysV x86_64), casando com o
 * va_list shim e com o v*printf do host. Não há código do jogo aqui. */
#ifndef __cstdarg__
#define __cstdarg__

#include <va_list.h>

#define va_start(ap, parm) __builtin_va_start(ap, parm)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)

#endif /* __cstdarg__ */
