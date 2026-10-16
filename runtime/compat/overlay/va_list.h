/* @orig none — shim de compat (quovix), Apache-2.0.
 *
 * Overlay do <va_list.h> do MSL_C. O original tipa va_list como o struct do ABI
 * de varargs do PowerPC (gpr/fpr/input_arg_area/reg_save_area); no alvo x86_64 o
 * clang espera o seu próprio __builtin_va_list (ABI SysV). Tipar va_list como
 * __builtin_va_list casa com os builtins de varargs do clang (ver cstdarg shim)
 * e com o v*printf do host. Não há código do jogo aqui. */
#ifndef VA_LIST_H
#define VA_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef __builtin_va_list __va_list;   /* alguns headers do jogo usam __va_list direto */
typedef __builtin_va_list va_list;

#ifdef __cplusplus
}
#endif

#endif /* VA_LIST_H */
