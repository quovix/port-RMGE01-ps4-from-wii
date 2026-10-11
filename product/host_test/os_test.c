/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Host test da camada OS (runtime/platform/orbis/os.c). Compilar com
   -DQVX_OS_HOST para forçar o backend pthread/libc. Testa o que é verificável
   no mac: formatação do OSReport, monotonia/conversão de tempo, calendário,
   arena + heap (OSAlloc), interrupts, mutex, message queue e thread. */
#define QVX_OS_HOST 1
#include "../../runtime/platform/orbis/os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int fails = 0;
static void chk(const char *name, int ok) {
    printf("  %-34s %s\n", name, ok ? "OK" : "FALHA");
    if (!ok) fails++;
}

/* sink de captura para OSReport */
static char g_cap[2048];
static void cap_sink(const char *s) {
    size_t l = strlen(g_cap), n = strlen(s);
    if (l + n < sizeof g_cap) memcpy(g_cap + l, s, n + 1);
}

static void *thread_body(void *p) {
    int *shared = (int *)p;
    *shared = 0xBEEF;
    return (void *)0x1234;
}

static volatile int g_alarm_hits = 0;
static void alarm_handler(OSAlarm *a, void *ctx) { (void)a; (void)ctx; g_alarm_hits++; }

int main(void) {
    qvx_os_init();

    /* ---- OSReport formatação ---- */
    qvx_os_set_log_sink(cap_sink);
    g_cap[0] = 0;
    OSReport("boot %s %d/%d (%04x)", "stage", 3, 7, 0xabc);
    chk("OSReport formata", strstr(g_cap, "boot stage 3/7 (0abc)") != 0);
    chk("OSReport newline", g_cap[strlen(g_cap) - 1] == '\n');
    g_cap[0] = 0;
    OSError("falha codigo=%d", 42);
    chk("OSError prefixo", strstr(g_cap, "OSError: falha codigo=42") != 0);
    qvx_os_set_log_sink(0);

    /* ---- tempo: conversões ---- */
    chk("Sec->Ticks->Sec", OSTicksToSeconds(OSSecondsToTicks(5)) == 5);
    chk("Ms->Ticks->Ms", OSTicksToMilliseconds(OSMillisecondsToTicks(250)) == 250);
    chk("1s = TIMER_CLOCK ticks", OSSecondsToTicks(1) == (OSTime)QVX_OS_TIMER_CLOCK);

    /* ---- tempo: monotonia ---- */
    OSTime t0 = OSGetTime();
    volatile uint64_t spin = 0; for (uint64_t i = 0; i < 2000000; i++) spin += i;
    OSTime t1 = OSGetTime();
    chk("OSGetTime monotônico", t1 >= t0);
    (void)spin;

    /* ---- tempo: calendário (época RVL 2000-01-01) ---- */
    OSCalendarTime cal;
    OSTicksToCalendarTime(0, &cal);
    chk("epoch=2000-01-01", cal.year == 2000 && cal.mon == 1 && cal.mday == 1);
    chk("epoch wday=sabado(6)", cal.wday == 6);
    /* 2000-01-02 13:45:30.500 */
    OSTime secs = (OSTime)135930;  /* 1 dia + 13:45:30 */
    OSTime tk = OSSecondsToTicks(secs) + (OSTime)QVX_OS_TIMER_CLOCK / 2;
    OSTicksToCalendarTime(tk, &cal);
    chk("cal mday=2", cal.mday == 2 && cal.mon == 1 && cal.year == 2000);
    chk("cal 13:45:30", cal.hour == 13 && cal.min == 45 && cal.sec == 30);
    chk("cal msec~500", cal.msec >= 499 && cal.msec <= 500);
    chk("cal wday=domingo(0)", cal.wday == 0);
    chk("cal yday=1", cal.yday == 1);

    /* ---- arena MEM2 configurável ---- */
    static char arena_block[1 << 20];
    qvx_os_arena_configure(arena_block, sizeof arena_block);
    chk("MEM2 lo", (char *)OSGetMEM2ArenaLo() == arena_block);
    chk("MEM2 hi", (char *)OSGetMEM2ArenaHi() == arena_block + sizeof arena_block);
    OSSetMEM2ArenaLo(arena_block + 4096);
    chk("MEM2 setLo", (char *)OSGetMEM2ArenaLo() == arena_block + 4096);

    /* ---- heap (OSAlloc) ---- */
    static char heapmem[256 * 1024];
    void *alo = OSInitAlloc(heapmem, heapmem + sizeof heapmem, 4);
    chk("OSInitAlloc devolve lo", alo == heapmem);
    OSHeapHandle h = OSCreateHeap(heapmem, heapmem + sizeof heapmem);
    chk("OSCreateHeap", h >= 0);
    int32_t free0 = OSCheckHeap(h);
    chk("OSCheckHeap inicial", free0 > 0);

    void *a = OSAllocFromHeap(h, 1000);
    void *b = OSAllocFromHeap(h, 2000);
    void *c = OSAllocFromHeap(h, 500);
    chk("allocs não-nulos e distintos", a && b && c && a != b && b != c);
    chk("alinhamento 32", ((uintptr_t)a % 32) == 0 && ((uintptr_t)b % 32) == 0);
    memset(a, 0xAA, 1000); memset(b, 0xBB, 2000); memset(c, 0xCC, 500); /* escreve */
    int32_t free1 = OSCheckHeap(h);
    chk("free diminuiu após alloc", free1 >= 0 && free1 < free0);

    OSFreeToHeap(h, b);
    OSFreeToHeap(h, a);   /* a e b adjacentes devem coalescer */
    void *big = OSAllocFromHeap(h, 2900);
    chk("coalescência (realoc grande cabe)", big != 0);
    OSFreeToHeap(h, c);
    OSFreeToHeap(h, big);
    chk("OSCheckHeap íntegro no fim", OSCheckHeap(h) > 0);

    OSHeapHandle prev = OSSetCurrentHeap(h);
    void *cur = OSAllocFromCurrentHeap(128);
    chk("current heap alloc", cur != 0);
    OSFreeToCurrentHeap(cur);
    OSSetCurrentHeap(prev);

    /* ---- interrupts ---- */
    int was = OSDisableInterrupts();
    chk("DisableInterrupts prev=on", was == 1);
    int now = OSRestoreInterrupts(was);
    chk("RestoreInterrupts prev=off", now == 0);
    chk("estado restaurado", OSDisableInterrupts() == 1);
    OSEnableInterrupts();

    /* ---- mutex (reentrante) ---- */
    static OSMutex m;
    OSInitMutex(&m);
    OSLockMutex(&m); OSLockMutex(&m);   /* recursivo: não deve travar */
    OSUnlockMutex(&m); OSUnlockMutex(&m);
    chk("mutex reentrante", 1);

    /* ---- message queue ---- */
    static OSMessage slots[4];
    static OSMessageQueue q;
    OSInitMessageQueue(&q, slots, 4);
    chk("recv vazio noblock falha", OSReceiveMessage(&q, 0, OS_MESSAGE_NOBLOCK) == 0);
    OSSendMessage(&q, (OSMessage)0x11, OS_MESSAGE_NOBLOCK);
    OSSendMessage(&q, (OSMessage)0x22, OS_MESSAGE_NOBLOCK);
    OSJamMessage(&q, (OSMessage)0x33, OS_MESSAGE_NOBLOCK);  /* vai pra frente */
    OSMessage got = 0;
    OSReceiveMessage(&q, &got, OS_MESSAGE_NOBLOCK);
    chk("jam vai pra frente (0x33)", got == (OSMessage)0x33);
    OSReceiveMessage(&q, &got, OS_MESSAGE_NOBLOCK);
    chk("FIFO (0x11)", got == (OSMessage)0x11);
    OSReceiveMessage(&q, &got, OS_MESSAGE_NOBLOCK);
    chk("FIFO (0x22)", got == (OSMessage)0x22);

    /* ---- thread: create suspensa, resume, join ---- */
    static OSThread th;
    int shared = 0;
    int ok = OSCreateThread(&th, thread_body, &shared, 0, 0, 16, 0);
    chk("OSCreateThread", ok == 1);
    chk("fica suspensa", OSIsThreadSuspended(&th) == 1);
    OSResumeThread(&th);
    void *rv = 0;
    OSJoinThread(&th, &rv);
    chk("thread rodou (shared)", shared == 0xBEEF);
    chk("retval do join", rv == (void *)0x1234);
    chk("terminada", OSIsThreadTerminated(&th) == 1);
    chk("OSGetCurrentThread não-nulo", OSGetCurrentThread() != 0);

    /* ---- #A: recria thread no MESMO OSThread (objeto reusado) ---- */
    static OSThread th2;
    int sh1 = 0, sh2 = 0;
    OSCreateThread(&th2, thread_body, &sh1, 0, 0, 16, 0);
    OSResumeThread(&th2);
    OSJoinThread(&th2, 0);
    OSCreateThread(&th2, thread_body, &sh2, 0, 0, 16, 0);  /* dedup: reusa a chave */
    chk("recria: fica suspensa de novo", OSIsThreadSuspended(&th2) == 1);
    OSResumeThread(&th2);
    OSJoinThread(&th2, 0);
    chk("recria: 1ª rodou", sh1 == 0xBEEF);
    chk("recria: 2ª rodou (sem hang)", sh2 == 0xBEEF);

    /* ---- #B: alarme periódico DISPARA ---- */
    static OSAlarm al;
    g_alarm_hits = 0;
    OSCreateAlarm(&al);
    OSSetPeriodicAlarm(&al, 0, OSMillisecondsToTicks(5), alarm_handler);
    usleep(70000);                        /* ~70 ms => ~14 disparos esperados */
    int hits = g_alarm_hits;
    chk("alarme disparou (>=2)", hits >= 2);
    OSCancelAlarm(&al);
    usleep(30000);
    int after = g_alarm_hits;
    usleep(30000);
    chk("cancel parou o alarme", g_alarm_hits <= after + 1);  /* tolera 1 em voo */

    printf(fails ? "\nFALHAS: %d\n" : "\ntodos os testes de OS passaram\n", fails);
    return fails ? 1 : 0;
}
