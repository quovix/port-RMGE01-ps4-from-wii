/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — shim original (quovix), Apache-2.0.
 *
 * JUTException/JUTConsole do JSystem usam o idiom de varargs do CodeWarrior
 * (__builtin_va_info), que o clang não tem, então JUTException.cpp fica stubbed.
 * O stub deixa os statics nulos, e GameSystemException::init() escreve em
 * JUTException::sErrorManager->mPrintWaitTime* e em getConsole()->mPositionX...
 * → NULL deref (diagnosticado no emulador 2026-10-13).
 *
 * O sistema de exceção do Wii é infra de DEBUG (tela de crash/assert). Para o
 * boot seguir, só precisamos que os statics apontem para objetos válidos; não
 * reimplementamos a lógica. Buffers zerados do tamanho REAL (o header dá o
 * layout x86_64) fazem as escritas de campo caírem em memória válida. As
 * virtuais só seriam chamadas num exception real, que não ocorre no boot.
 *
 * Código próprio (não copia corpo do decomp): apenas aloca e guarda ponteiros.
 */
#include "JSystem/JUtility/JUTException.hpp"
#include "JSystem/JUtility/JUTConsole.hpp"
/* -nostdinc: sem <stdlib.h>. calloc vem da libc no link (Orbis/shadPS4). */
extern "C" void* calloc(unsigned long, unsigned long);

/* Definições dos membros estáticos que o boot escreve (JUTException.cpp não é
 * compilado; estas defs fortes vencem os data-stubs zerados). */
JUTException*            JUTException::sErrorManager    = 0;
JUTConsole*             JUTException::sConsole          = 0;
void*                   JUTException::sConsoleBuffer    = 0;
u32                     JUTException::sConsoleBufferSize = 0;
JUTExceptionUserCallback JUTException::sPreUserCallback  = 0;
JUTExceptionUserCallback JUTException::sPostUserCallback = 0;

JUTException* JUTException::create(JUTDirectPrint*) {
    if (!sErrorManager) {
        sErrorManager = (JUTException*)calloc(1, sizeof(JUTException));
    }
    return sErrorManager;
}

void JUTException::createConsole(void* buffer, u32 size) {
    if (!sConsole) {
        sConsole = (JUTConsole*)calloc(1, sizeof(JUTConsole));
    }
    sConsoleBuffer = buffer;
    sConsoleBufferSize = size;
}

JUTExceptionUserCallback JUTException::setPreUserCallback(JUTExceptionUserCallback cb) {
    JUTExceptionUserCallback old = sPreUserCallback;
    sPreUserCallback = cb;
    return old;
}
