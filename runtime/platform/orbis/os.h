/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Camada OS do backend Orbis: implementa os NOMES da API de OS do GC/Wii (RVL)
   que o boot do Petari exercita, sobre primitivas do PS4/OpenOrbis (scePthread,
   sceKernel*) — ou pthread/libc no host test. Código nosso; não é o SDK do Wii.

   Fonte da superfície: os símbolos OS* que o link do motor deixa indefinidos
   (ver out/engine-link/stubs.S). O que o boot NÃO referencia (MEM1 arena, OSAlloc
   heaps, OSGetTick, conversões de tick, OSSleepThread/OSWakeupThread) é macro
   inline do RVL ou interno do motor; mesmo assim é oferecido aqui como API
   própria host-testável — não é linkado pelo eboot a menos que chamado.

   os.h é incluído só pela nossa os.c e pelos host tests, nunca por TU do motor;
   por isso os tipos podem ser definidos à vontade — só os símbolos (C linkage)
   e a ABI das funções precisam casar com o que o motor chama. A única exceção é
   OSCalendarTime, cujo layout o motor lê: 10 ints na ordem do RVL. */
#ifndef QVX_ORBIS_OS_H
#define QVX_ORBIS_OS_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- tipos base (conforme RVL) ------------------------------------------- */
typedef int64_t   OSTime;        /* ticks do timebase (64 bits) */
typedef uint32_t  OSTick;        /* 32 bits baixos do timebase */
typedef int32_t   OSPriority;    /* 0 (maior) .. 31 (menor) no Wii */
typedef int32_t   OSHeapHandle;  /* índice na tabela de heaps; -1 inválido */
typedef void     *OSMessage;     /* mensagem = ponteiro opaco */

/* Objetos de sync do jogo: opacos para nós, usados só como CHAVE numa side
   table (arquitetura estudada no OSThread.cpp do Dusklight — não embutir o
   objeto de sync nativo na struct que o jogo acessa). Nunca derreferenciados
   pela nossa camada. Têm uma definição-blob completa (nunca lida por nós) só
   para servir de storage nos host tests e no stand-in da main thread; o motor
   usa SUA própria definição RVL (não inclui este header), e no link só casam o
   nome do símbolo e a ABI (ponteiro). Os tamanhos são folgados em relação ao
   layout RVL, caso algum ponteiro seja usado como memória válida. */
typedef struct OSThread        { unsigned char _opaque[0x340]; } OSThread;
typedef struct OSThreadQueue   { unsigned char _opaque[0x10];  } OSThreadQueue;
typedef struct OSMutex         { unsigned char _opaque[0x20];  } OSMutex;
typedef struct OSMessageQueue  { unsigned char _opaque[0x40];  } OSMessageQueue;
typedef struct OSAlarm         { unsigned char _opaque[0x30];  } OSAlarm;

typedef void *(*OSThreadFunc)(void *param);
typedef void  (*OSAlarmHandler)(OSAlarm *alarm, void *context);

/* OSCalendarTime: layout LIDO pelo motor — 10 ints na ordem do RVL. int é 32
   bits tanto no Wii quanto no target x86_64, então o layout casa. */
typedef struct OSCalendarTime {
    int sec;   int min;  int hour; int mday; int mon;
    int year;  int wday; int yday; int msec; int usec;
} OSCalendarTime;

/* Flags de OSSendMessage/OSReceiveMessage/OSJamMessage. */
enum { OS_MESSAGE_NOBLOCK = 0, OS_MESSAGE_BLOCK = 1 };

/* Clock do Wii (RVL). O timebase (OSGetTime) corre a __OSBusClock/4. */
#define QVX_OS_BUS_CLOCK   243000000u   /* 243 MHz */
#define QVX_OS_CORE_CLOCK  729000000u   /* 729 MHz */
#define QVX_OS_TIMER_CLOCK (QVX_OS_BUS_CLOCK / 4u)  /* 60.75 MHz */

/* Globais __OSBusClock/__OSCoreClock: o motor normalmente os define por conta
   própria (não estão entre os símbolos indefinidos do link). Para evitar
   conflito de ODR, só os definimos se QVX_OS_DEFINE_CLOCKS for pedido. Aqui só
   os declaramos para uso interno/integração. */
extern uint32_t __OSBusClock;
extern uint32_t __OSCoreClock;

/* ---- inicialização da nossa camada (chamar uma vez no bring-up) ---------- */
void qvx_os_init(void);

/* Instala um sink de log para OSReport/OSPanic (além do klog/on-screen). Usado
   pelos host tests para capturar a saída formatada; NULL remove. */
void qvx_os_set_log_sink(void (*sink)(const char *line));

/* Fornece o bloco que respalda a arena MEM2 (ex.: direct memory mapeada). Se
   não for chamado, a arena cai num malloc lazy de QVX_OS_ARENA_DEFAULT. */
void qvx_os_arena_configure(void *base, size_t size);

/* ---- 1) log / panic ------------------------------------------------------ */
void OSReport(const char *msg, ...);
void OSVReport(const char *msg, va_list ap);
void OSPanic(const char *file, int line, const char *msg, ...);
void OSHalt(const char *msg);
void OSError(const char *msg, ...);

/* ---- 2) tempo ------------------------------------------------------------ */
OSTime OSGetTime(void);
OSTick OSGetTick(void);
void   OSTicksToCalendarTime(OSTime ticks, OSCalendarTime *td);
OSTime OSSecondsToTicks(OSTime sec);
OSTime OSTicksToSeconds(OSTime ticks);
OSTime OSTicksToMilliseconds(OSTime ticks);
OSTime OSMillisecondsToTicks(OSTime ms);
OSTime OSMicrosecondsToTicks(OSTime us);
OSTime OSNanosecondsToTicks(OSTime ns);

/* ---- 3) arena + heap (OSAlloc) ------------------------------------------- */
void        *OSGetArenaLo(void);
void        *OSGetArenaHi(void);
void         OSSetArenaLo(void *lo);
void         OSSetArenaHi(void *hi);
void        *OSGetMEM2ArenaLo(void);
void        *OSGetMEM2ArenaHi(void);
void         OSSetMEM2ArenaLo(void *lo);
void         OSSetMEM2ArenaHi(void *hi);

void        *OSInitAlloc(void *arenaLo, void *arenaHi, int maxHeaps);
OSHeapHandle OSCreateHeap(void *lo, void *hi);
void         OSDestroyHeap(OSHeapHandle heap);
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);
void        *OSAllocFromHeap(OSHeapHandle heap, uint32_t size);
void        *OSAllocFixed(void **rangeStart, void **rangeEnd);
void         OSFreeToHeap(OSHeapHandle heap, void *ptr);
int32_t      OSCheckHeap(OSHeapHandle heap);  /* bytes livres, ou -1 corrompido */
void        *OSAllocFromCurrentHeap(uint32_t size);
void         OSFreeToCurrentHeap(void *ptr);

/* ---- 4) interrupts / scheduler (stubs seguros) --------------------------- */
int  OSDisableInterrupts(void);          /* retorna estado anterior (TRUE=on) */
int  OSEnableInterrupts(void);
int  OSRestoreInterrupts(int level);
int  OSDisableScheduler(void);
int  OSEnableScheduler(void);

/* ---- 5) threads / sync (real sobre scePthread/pthread) ------------------- */
int         OSCreateThread(OSThread *thread, OSThreadFunc func, void *param,
                           void *stackBase, uint32_t stackSize,
                           OSPriority prio, uint16_t attr);
int         OSResumeThread(OSThread *thread);   /* retorna suspend count antes */
int         OSSuspendThread(OSThread *thread);
int         OSJoinThread(OSThread *thread, void **val);
int         OSDetachThread(OSThread *thread);
void        OSCancelThread(OSThread *thread);
void        OSYieldThread(void);
OSThread   *OSGetCurrentThread(void);
OSPriority  OSGetThreadPriority(OSThread *thread);
int         OSSetThreadPriority(OSThread *thread, OSPriority prio);
int         OSIsThreadSuspended(OSThread *thread);
int         OSIsThreadTerminated(OSThread *thread);
void        OSSleepThread(OSThreadQueue *queue);
void        OSWakeupThread(OSThreadQueue *queue);
void        OSInitThreadQueue(OSThreadQueue *queue);

void OSInitMutex(OSMutex *mutex);
void OSLockMutex(OSMutex *mutex);
void OSUnlockMutex(OSMutex *mutex);
int  OSTryLockMutex(OSMutex *mutex);

void OSInitMessageQueue(OSMessageQueue *mq, OSMessage *msgArray, int32_t count);
int  OSSendMessage(OSMessageQueue *mq, OSMessage msg, int32_t flags);
int  OSReceiveMessage(OSMessageQueue *mq, OSMessage *out, int32_t flags);
int  OSJamMessage(OSMessageQueue *mq, OSMessage msg, int32_t flags);

/* ---- 6) alarm (stub seguro: registra, não dispara) ----------------------- */
void OSCreateAlarm(OSAlarm *alarm);
void OSSetPeriodicAlarm(OSAlarm *alarm, OSTime start, OSTime period,
                        OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm *alarm);

/* ---- 7) power / reset (terminal: loga + halt loop) ----------------------- */
int   OSGetResetButtonState(void);
void *OSSetPowerCallback(void *callback);
void  OSRebootSystem(void);
void  OSRestart(uint32_t resetCode);
void  OSReturnToMenu(void);
void  OSShutdownSystem(void);

#ifdef __cplusplus
}
#endif
#endif
