/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Implementação da camada OS (ver os.h). Um único TU para facilitar a
   integração: a seção de BACKEND no topo isola as primitivas de plataforma
   (scePthread/sceKernel no Orbis, pthread/libc no host) atrás de wrappers
   estáticos; toda a lógica da API RVL abaixo é escrita uma vez só contra esses
   wrappers. */

#include "os.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ========================================================================== *
 *  Seleção de backend
 * ========================================================================== */
#if defined(QVX_OS_HOST)
#  define QVX_OS_BACKEND_ORBIS 0
#elif defined(__ORBIS__) || defined(__PROSPERO__) || defined(__FreeBSD__)
#  define QVX_OS_BACKEND_ORBIS 1
#else
#  define QVX_OS_BACKEND_ORBIS 0
#endif

/* Tipos de storage: OrbisPthread* são typedefs diretos de pthread_t/
   pthread_mutex_t/pthread_cond_t, então o mesmo tipo serve nos dois backends. */
#if QVX_OS_BACKEND_ORBIS
#  include <orbis/libkernel.h>
#  include <sys/mman.h>           /* mmap MAP_FIXED p/ os globais em 0x80000000 */
#  include "../../video/text.h"   /* qvx_log_line — só existe no build do alvo */
   typedef OrbisPthread       qos_thread_t;
   typedef OrbisPthreadMutex  qos_mutex_t;
   typedef OrbisPthreadCond   qos_cond_t;
#else
#  include <pthread.h>
#  include <time.h>
#  include <unistd.h>
   typedef pthread_t          qos_thread_t;
   typedef pthread_mutex_t    qos_mutex_t;
   typedef pthread_cond_t     qos_cond_t;
#endif

/* ---- wrappers de klog / tempo / sleep / yield ---------------------------- */
static void qos_klog(const char *s) {
#if QVX_OS_BACKEND_ORBIS
    sceKernelDebugOutText(0, s);
    qvx_log_line(s);
#else
    fputs(s, stdout);
#endif
}

/* microsegundos monotônicos desde o início do processo */
static uint64_t qos_now_usec(void) {
#if QVX_OS_BACKEND_ORBIS
    return sceKernelGetProcessTime();   /* já em microsegundos */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
#endif
}

static void qos_usleep(uint32_t us) {
#if QVX_OS_BACKEND_ORBIS
    sceKernelUsleep(us);
#else
    usleep(us);
#endif
}

static void qos_yield(void) {
#if QVX_OS_BACKEND_ORBIS
    sceKernelUsleep(0);
#else
    sched_yield();
#endif
}

/* ---- wrappers de mutex / cond -------------------------------------------- */
static void qos_mutex_init(qos_mutex_t *m, int recursive) {
#if QVX_OS_BACKEND_ORBIS
    OrbisPthreadMutexattr attr;
    scePthreadMutexattrInit(&attr);
    scePthreadMutexattrSettype(&attr, recursive ? 2 /*RECURSIVE*/ : 3 /*NORMAL*/);
    scePthreadMutexInit(m, &attr, "qos");
    scePthreadMutexattrDestroy(&attr);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,
        recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
}
static void qos_mutex_lock(qos_mutex_t *m) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadMutexLock(m);
#else
    pthread_mutex_lock(m);
#endif
}
static void qos_mutex_unlock(qos_mutex_t *m) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadMutexUnlock(m);
#else
    pthread_mutex_unlock(m);
#endif
}
static void qos_cond_init(qos_cond_t *c) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadCondInit(c, 0, "qos");
#else
    pthread_cond_init(c, 0);
#endif
}
static void qos_cond_wait(qos_cond_t *c, qos_mutex_t *m) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadCondWait(c, m);
#else
    pthread_cond_wait(c, m);
#endif
}
static void qos_cond_signal(qos_cond_t *c) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadCondSignal(c);
#else
    pthread_cond_signal(c);
#endif
}
static void qos_cond_broadcast(qos_cond_t *c) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadCondBroadcast(c);
#else
    pthread_cond_broadcast(c);
#endif
}
static void qos_cond_destroy(qos_cond_t *c) {
#if QVX_OS_BACKEND_ORBIS
    scePthreadCondDestroy(c);
#else
    pthread_cond_destroy(c);
#endif
}

/* ---- wrappers de thread -------------------------------------------------- */
static int qos_thread_create(qos_thread_t *t, void *(*f)(void *), void *arg) {
#if QVX_OS_BACKEND_ORBIS
    return scePthreadCreate(t, 0, f, arg, "qos");
#else
    return pthread_create(t, 0, f, arg);
#endif
}
static int qos_thread_join(qos_thread_t t, void **ret) {
#if QVX_OS_BACKEND_ORBIS
    return scePthreadJoin(t, ret);
#else
    return pthread_join(t, ret);
#endif
}
static int qos_thread_detach(qos_thread_t t) {
#if QVX_OS_BACKEND_ORBIS
    return scePthreadDetach(t);
#else
    return pthread_detach(t);
#endif
}
static qos_thread_t qos_thread_self(void) {
#if QVX_OS_BACKEND_ORBIS
    return scePthreadSelf();
#else
    return pthread_self();
#endif
}
static int qos_thread_equal(qos_thread_t a, qos_thread_t b) {
#if QVX_OS_BACKEND_ORBIS
    return scePthreadEqual(a, b);
#else
    return pthread_equal(a, b);
#endif
}

/* ========================================================================== *
 *  Mutex global da camada (protege side tables, log, arena, heaps)
 * ========================================================================== */
static qos_mutex_t  g_os_lock;
static int          g_os_ready = 0;

/* --- globais de baixa memória do Wii em OS_BASE_CACHED (0x80000000) ----------
   O engine (JKRHeap::initArena, OSPlayTime, etc.) lê a struct OSBootInfo e os
   globais do SO via a MACRO OSPhysicalToCached(p) = (void*)(p + 0x80000000),
   com o mapa de memória do Wii embutido em TODOS os objetos já compilados. No
   Orbis esse endereço não existe, então `code->memorySize` (initArena) derefa
   0x80000000 e dá SIGSEGV. Mapeamos uma página real e fixa nesse endereço — não
   é a arena (essa é separada), só serve os globais que o SDK lê. Zerado basta:
   o único consumidor no caminho de boot (mMemorySize) é write-only no jogo; se
   um global não-nulo for exigido depois, populamos sob demanda. */
#define QVX_OS_LOWMEM_BASE 0x80000000UL
#define QVX_OS_LOWMEM_SIZE 0x4000u   /* 16 KB: cobre os globais do SO até ~0x3128 */
/* Bases FIXAS < 4GB das arenas (ver nota no bloco da arena). Definidas aqui
   porque qvx_os_init (abaixo) as atribui aos want_base. */
#define QVX_MEM1_BASE 0x81000000UL   /* 24 MB -> 0x82800000 */
#define QVX_MEM2_BASE 0x90000000UL   /* 64 MB -> 0x94000000 */
#ifndef MAP_ANON
#  ifdef MAP_ANONYMOUS
#    define MAP_ANON MAP_ANONYMOUS
#  endif
#endif
static int g_lowmem_ready = 0;
static void qvx_os_lowmem_init(void) {
    if (g_lowmem_ready) return;
    g_lowmem_ready = 1;   /* marca antes: evita reentrância se mmap logar via os_* */
#if QVX_OS_BACKEND_ORBIS
    void *want = (void *)QVX_OS_LOWMEM_BASE;
    void *got = mmap(want, QVX_OS_LOWMEM_SIZE, PROT_READ | PROT_WRITE,
                     MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
    char dbg[96];
    snprintf(dbg, sizeof dbg, "[quovix] lowmem mmap(0x80000000)=%p want=%p\n",
             got, want);
    sceKernelDebugOutText(0, dbg);
    if (got == MAP_FAILED || got != want) {
        qvx_set_phase("lowmem: FALHA ao mapear 0x80000000");
        return;
    }
    memset(want, 0, QVX_OS_LOWMEM_SIZE);
    qvx_set_phase("lowmem: 0x80000000 mapeado (globais do SO)");
#endif
}

void qvx_os_init(void) {
    if (g_os_ready) return;
    qos_mutex_init(&g_os_lock, 1 /*recursive*/);
    g_os_ready = 1;
    qvx_os_lowmem_init();
}
static void os_ensure(void) { if (!g_os_ready) qvx_os_init(); }
static void os_lock(void)   { os_ensure(); qos_mutex_lock(&g_os_lock); }
static void os_unlock(void) { qos_mutex_unlock(&g_os_lock); }

/* ========================================================================== *
 *  1) log / panic
 * ========================================================================== */
static void (*g_log_sink)(const char *) = 0;

void qvx_os_set_log_sink(void (*sink)(const char *)) { g_log_sink = sink; }

static void os_emit(const char *s) {
    qos_klog(s);
    if (g_log_sink) g_log_sink(s);
}

/* Garante que a linha termine em '\n' (klog do PS4 espera linha completa). */
static void os_emit_line(char *buf, size_t cap, size_t len) {
    if (len + 1 >= cap) len = cap - 2;
    if (len == 0 || buf[len - 1] != '\n') { buf[len] = '\n'; buf[len + 1] = 0; }
    os_emit(buf);
}

void OSVReport(const char *msg, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof buf - 1, msg ? msg : "(null)", ap);
    if (n < 0) n = 0;
#if QVX_OS_BACKEND_ORBIS
    /* o jogo narra o boot via OSReport; roteia o ULTIMO report pro g_qvx_phase
       (canal lido ao vivo), pra localizar onde ele para dentro do qvx_game_entry
       mesmo sem o handler de sinal (que o Orbis não entrega ao userland). */
    { char ph[96]; int i = 0;
      ph[i++]='O';ph[i++]='S';ph[i++]='R';ph[i++]=':';
      for (int j = 0; j < n && i < 95 && buf[j] && buf[j] != '\n'; j++) ph[i++] = buf[j];
      ph[i] = 0; qvx_set_phase(ph); }
#endif
    os_emit_line(buf, sizeof buf, (size_t)n);
}

void OSReport(const char *msg, ...) {
    va_list ap; va_start(ap, msg); OSVReport(msg, ap); va_end(ap);
}

static void os_halt_loop(void) {
    for (;;) qos_usleep(1000000);   /* trava controlada, sem busy-spin */
}

void OSPanic(const char *file, int line, const char *msg, ...) {
    char head[256];
    snprintf(head, sizeof head, "OSPanic: %s:%d: ", file ? file : "?", line);
    os_emit(head);
    char body[1024];
    va_list ap; va_start(ap, msg);
    int n = vsnprintf(body, sizeof body - 1, msg ? msg : "", ap);
    va_end(ap);
    if (n < 0) n = 0;
    os_emit_line(body, sizeof body, (size_t)n);
#if QVX_OS_BACKEND_ORBIS
    /* grava no canal lido ao vivo (o halt_loop não encerra o processo) */
    { char ph[96]; snprintf(ph, sizeof ph, "PANIC %s:%d %s", file ? file : "?", line, body);
      qvx_set_phase(ph); }
#endif
    os_halt_loop();
}

void OSHalt(const char *msg) {
    char buf[256];
    snprintf(buf, sizeof buf, "OSHalt: %s", msg ? msg : "");
    os_emit(buf); os_emit("\n");
#if QVX_OS_BACKEND_ORBIS
    { char ph[96]; snprintf(ph, sizeof ph, "HALT %s", msg ? msg : ""); qvx_set_phase(ph); }
#endif
    os_halt_loop();
}

void OSError(const char *msg, ...) {
    os_emit("OSError: ");
    char buf[1024];
    va_list ap; va_start(ap, msg);
    int n = vsnprintf(buf, sizeof buf - 1, msg ? msg : "", ap);
    va_end(ap);
    if (n < 0) n = 0;
    os_emit_line(buf, sizeof buf, (size_t)n);
}

/* ========================================================================== *
 *  2) tempo
 * ========================================================================== */
#ifdef QVX_OS_DEFINE_CLOCKS
uint32_t __OSBusClock  = QVX_OS_BUS_CLOCK;
uint32_t __OSCoreClock = QVX_OS_CORE_CLOCK;
#endif

OSTime OSGetTime(void) {
    /* usec monotônico -> ticks de 60.75 MHz, mantendo a cadência do Wii.
       ticks = usec * TIMER_CLOCK / 1e6 = usec * 243 / 4 (exato). Fatorado como
       (us/4)*243 + (us%4)*243/4 para não estourar u64 (us*243e6 estouraria em
       ~21h de uptime; esta forma só estoura em milênios). */
    const uint64_t N = QVX_OS_BUS_CLOCK / 1000000ull;   /* 243 */
    uint64_t us = qos_now_usec();
    uint64_t ticks = (us / 4ull) * N + ((us % 4ull) * N) / 4ull;
    return (OSTime)ticks;
}

OSTick OSGetTick(void) { return (OSTick)OSGetTime(); }

OSTime OSSecondsToTicks(OSTime sec)   { return sec * (OSTime)QVX_OS_TIMER_CLOCK; }
OSTime OSTicksToSeconds(OSTime ticks) { return ticks / (OSTime)QVX_OS_TIMER_CLOCK; }
OSTime OSTicksToMilliseconds(OSTime t){ return t / (OSTime)(QVX_OS_TIMER_CLOCK/1000u); }
OSTime OSMillisecondsToTicks(OSTime m){ return m * (OSTime)(QVX_OS_TIMER_CLOCK/1000u); }
OSTime OSMicrosecondsToTicks(OSTime u){ return (u * (OSTime)QVX_OS_TIMER_CLOCK)/1000000; }
OSTime OSNanosecondsToTicks(OSTime n) { return (n * (OSTime)QVX_OS_TIMER_CLOCK)/1000000000; }

/* civil (ano/mês/dia) a partir de dias desde a época, algoritmo de Hinnant.
   z = dias desde 1970-01-01. Retorna por ponteiro y/m/d. */
static void civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);              /* [0,146096] */
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; /* [0,399] */
    int64_t yy = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);         /* [0,365] */
    unsigned mp = (5*doy + 2)/153;                            /* [0,11] */
    unsigned dd = doy - (153*mp + 2)/5 + 1;                   /* [1,31] */
    unsigned mm = mp < 10 ? mp + 3 : mp - 9;                  /* [1,12] */
    *y = (int)(yy + (mm <= 2));
    *m = (int)mm;
    *d = (int)dd;
}

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime *td) {
    if (!td) return;
    const int64_t TC = (int64_t)QVX_OS_TIMER_CLOCK;
    /* época do RVL é 2000-01-01 00:00:00. */
    int64_t total_us = (ticks / TC) * 1000000 + ((ticks % TC) * 1000000) / TC;
    int64_t secs = total_us / 1000000;
    int64_t rem_us = total_us % 1000000;
    if (rem_us < 0) { rem_us += 1000000; secs -= 1; }

    int64_t days = secs / 86400;
    int64_t tod  = secs % 86400;
    if (tod < 0) { tod += 86400; days -= 1; }

    td->hour = (int)(tod / 3600);
    td->min  = (int)((tod % 3600) / 60);
    td->sec  = (int)(tod % 60);
    td->msec = (int)(rem_us / 1000);
    td->usec = (int)(rem_us % 1000);

    /* dias desde 2000-01-01 -> desde 1970-01-01 (10957 dias entre as épocas). */
    int64_t days_1970 = days + 10957;
    int y, m, d;
    civil_from_days(days_1970, &y, &m, &d);
    td->year = y; td->mon = m; td->mday = d;

    /* wday: 1970-01-01 foi quinta (4, com domingo=0). */
    int wd = (int)((days_1970 % 7 + 4) % 7);
    if (wd < 0) wd += 7;
    td->wday = wd;

    /* yday: dias desde 1 de janeiro do ano corrente. */
    static const int cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
    td->yday = cum[m - 1] + (d - 1) + ((m > 2 && leap) ? 1 : 0);
}

/* ========================================================================== *
 *  3) arena (MEM1 / MEM2) + heap (OSAlloc)
 * ========================================================================== */
#ifndef QVX_OS_ARENA_DEFAULT
#  define QVX_OS_ARENA_DEFAULT (64u * 1024u * 1024u)  /* 64 MB (MEM2) */
#endif
#ifndef QVX_OS_MEM1_DEFAULT
#  define QVX_OS_MEM1_DEFAULT  (24u * 1024u * 1024u)  /* 24 MB (MEM1) */
#endif

typedef struct { char *lo, *hi, *base; size_t size; int from_malloc; unsigned long want_base; } os_arena;
static os_arena g_mem2, g_mem1;

/* Endereços FIXOS abaixo de 4GB para as arenas (mapa de memória estilo Wii).
   O engine faz masking de 32 bits em ponteiros (ex.: JKRHeap::initArena usa o
   LITERAL 0xFFFFFFE0, não a macro OSRoundDown32B), embutido em 1468 objetos.
   Uma arena acima de 4GB (o malloc dava 0x201000000) é TRUNCADA por essa máscara
   -> ponteiro inválido -> SIGSEGV no createRoot. Mantendo MEM1/MEM2 < 4GB o
   masking fica lossless. Não colide com os globais (0x80000000) nem com o eboot
   (0x400000-0xd20000). (diagnóstico 2026-10-13, emulador).
   As macros QVX_MEM1_BASE/QVX_MEM2_BASE estão definidas lá em cima (antes de
   qvx_os_init, que as atribui a want_base). */

void qvx_os_arena_configure(void *base, size_t size) {
    os_lock();
    g_mem2.base = (char *)base; g_mem2.size = size;
    g_mem2.lo = (char *)base;   g_mem2.hi = (char *)base + size;
    g_mem2.from_malloc = 0;
    os_unlock();
}

static void arena_ensure(os_arena *a, size_t def) {
    if (a->base) return;
    if (!a->want_base)   /* bases fixas < 4GB: masking de 32b do engine fica lossless */
        a->want_base = (a == &g_mem1) ? QVX_MEM1_BASE : (a == &g_mem2) ? QVX_MEM2_BASE : 0;
#if QVX_OS_BACKEND_ORBIS
    if (a->want_base) {
        void *want = (void *)a->want_base;
        void *got = mmap(want, def, PROT_READ | PROT_WRITE,
                         MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
        char dbg[96];
        snprintf(dbg, sizeof dbg, "[quovix] arena mmap(%p,%zuMB)=%p\n",
                 want, def >> 20, got);
        sceKernelDebugOutText(0, dbg);
        if (got != MAP_FAILED && got == want) {
            a->base = (char *)want; a->from_malloc = 0;
            a->size = def; a->lo = a->base; a->hi = a->base + def;
            return;
        }
        /* fallback: malloc (pode truncar no masking, mas não deixa a arena nula) */
    }
#endif
    a->base = (char *)malloc(def);
    a->size = a->base ? def : 0;
    a->lo = a->base; a->hi = a->base + a->size;
    a->from_malloc = 1;
}

void *OSGetMEM2ArenaLo(void) { void *r; os_lock(); arena_ensure(&g_mem2, QVX_OS_ARENA_DEFAULT); r = g_mem2.lo; os_unlock(); return r; }
void *OSGetMEM2ArenaHi(void) { void *r; os_lock(); arena_ensure(&g_mem2, QVX_OS_ARENA_DEFAULT); r = g_mem2.hi; os_unlock(); return r; }
void  OSSetMEM2ArenaLo(void *lo) { os_lock(); g_mem2.lo = (char *)lo; os_unlock(); }
void  OSSetMEM2ArenaHi(void *hi) { os_lock(); g_mem2.hi = (char *)hi; os_unlock(); }

void *OSGetArenaLo(void) { void *r; os_lock(); arena_ensure(&g_mem1, QVX_OS_MEM1_DEFAULT); r = g_mem1.lo; os_unlock(); return r; }
void *OSGetArenaHi(void) { void *r; os_lock(); arena_ensure(&g_mem1, QVX_OS_MEM1_DEFAULT); r = g_mem1.hi; os_unlock(); return r; }
void  OSSetArenaLo(void *lo) { os_lock(); g_mem1.lo = (char *)lo; os_unlock(); }
void  OSSetArenaHi(void *hi) { os_lock(); g_mem1.hi = (char *)hi; os_unlock(); }

/* --- OSAlloc: free-list first-fit com coalescência, alinhamento de 32 ----- */
#define OS_HEAP_ALIGN 32u

typedef struct os_cell {
    size_t          size;   /* bytes do payload (exclui o header) */
    int             free;
    struct os_cell *next;   /* próximo cell na ordem de endereço */
    struct os_cell *prev;
} os_cell;

/* O payload começa logo após o header; para o alinhamento de 32 valer, o header
   precisa ser múltiplo de 32. Se mexerem nos campos, isto quebra o build em vez
   de devolver ponteiros desalinhados em silêncio. */
_Static_assert(sizeof(os_cell) == 32, "os_cell deve ter 32 bytes (alinhamento do heap)");

typedef struct { int used; os_cell *head; char *lo, *hi; } os_heap;

#ifndef QVX_OS_MAX_HEAPS
#  define QVX_OS_MAX_HEAPS 16
#endif
static os_heap     g_heaps[QVX_OS_MAX_HEAPS];
static int         g_num_heaps = 0;
static OSHeapHandle g_current_heap = -1;

static size_t align_up(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }
static char  *ptr_align_up(char *p, size_t a) {
    uintptr_t v = (uintptr_t)p; v = (v + a - 1) & ~(uintptr_t)(a - 1); return (char *)v;
}

void *OSInitAlloc(void *arenaLo, void *arenaHi, int maxHeaps) {
    os_lock();
    (void)arenaHi;
    g_num_heaps = (maxHeaps < QVX_OS_MAX_HEAPS) ? maxHeaps : QVX_OS_MAX_HEAPS;
    for (int i = 0; i < QVX_OS_MAX_HEAPS; i++) {
        g_heaps[i].used = 0; g_heaps[i].head = 0; g_heaps[i].lo = g_heaps[i].hi = 0;
    }
    g_current_heap = -1;
    /* RVL reserva a tabela de handles no início da arena; aqui a tabela é
       estática, então não consumimos a arena: devolvemos arenaLo inalterado. */
    void *r = arenaLo;
    os_unlock();
    return r;
}

OSHeapHandle OSCreateHeap(void *lo, void *hi) {
    os_lock();
    char *clo = ptr_align_up((char *)lo, OS_HEAP_ALIGN);
    char *chi = (char *)hi;
    OSHeapHandle h = -1;
    if (chi > clo && (size_t)(chi - clo) >= sizeof(os_cell) + OS_HEAP_ALIGN) {
        int n = g_num_heaps > 0 ? g_num_heaps : QVX_OS_MAX_HEAPS;
        for (int i = 0; i < n; i++) if (!g_heaps[i].used) {
            os_cell *c = (os_cell *)clo;
            c->size = (size_t)(chi - clo) - sizeof(os_cell);
            c->free = 1; c->next = 0; c->prev = 0;
            g_heaps[i].used = 1; g_heaps[i].head = c; g_heaps[i].lo = clo; g_heaps[i].hi = chi;
            h = (OSHeapHandle)i;
            break;
        }
    }
    if (g_current_heap < 0 && h >= 0) g_current_heap = h;
    os_unlock();
    return h;
}

void OSDestroyHeap(OSHeapHandle heap) {
    os_lock();
    if (heap >= 0 && heap < QVX_OS_MAX_HEAPS) {
        g_heaps[heap].used = 0; g_heaps[heap].head = 0;
        if (g_current_heap == heap) g_current_heap = -1;
    }
    os_unlock();
}

OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    os_lock();
    OSHeapHandle prev = g_current_heap;
    g_current_heap = heap;
    os_unlock();
    return prev;
}

void *OSAllocFromHeap(OSHeapHandle heap, uint32_t size) {
    void *ret = 0;
    os_lock();
    if (heap >= 0 && heap < QVX_OS_MAX_HEAPS && g_heaps[heap].used) {
        size_t need = align_up(size ? size : 1, OS_HEAP_ALIGN);
        for (os_cell *c = g_heaps[heap].head; c; c = c->next) {
            if (c->free && c->size >= need) {
                /* split se sobrar espaço para outro cell + alinhamento */
                if (c->size >= need + sizeof(os_cell) + OS_HEAP_ALIGN) {
                    char *np = (char *)(c + 1) + need;
                    os_cell *nc = (os_cell *)np;
                    nc->size = c->size - need - sizeof(os_cell);
                    nc->free = 1; nc->next = c->next; nc->prev = c;
                    if (nc->next) nc->next->prev = nc;
                    c->next = nc; c->size = need;
                }
                c->free = 0;
                ret = (void *)(c + 1);
                break;
            }
        }
    }
    os_unlock();
    return ret;
}

void OSFreeToHeap(OSHeapHandle heap, void *ptr) {
    if (!ptr) return;
    os_lock();
    if (heap >= 0 && heap < QVX_OS_MAX_HEAPS && g_heaps[heap].used) {
        os_cell *c = ((os_cell *)ptr) - 1;
        c->free = 1;
        /* coalesce com o próximo e o anterior se livres e contíguos */
        if (c->next && c->next->free) {
            c->size += sizeof(os_cell) + c->next->size;
            c->next = c->next->next;
            if (c->next) c->next->prev = c;
        }
        if (c->prev && c->prev->free) {
            c->prev->size += sizeof(os_cell) + c->size;
            c->prev->next = c->next;
            if (c->next) c->next->prev = c->prev;
        }
    }
    os_unlock();
}

int32_t OSCheckHeap(OSHeapHandle heap) {
    int32_t freebytes = 0;
    os_lock();
    if (heap < 0 || heap >= QVX_OS_MAX_HEAPS || !g_heaps[heap].used) { os_unlock(); return -1; }
    for (os_cell *c = g_heaps[heap].head; c; c = c->next) {
        char *cp = (char *)c;
        if (cp < g_heaps[heap].lo || (cp + sizeof(os_cell) + c->size) > g_heaps[heap].hi) {
            os_unlock(); return -1;   /* cell fora dos limites: corrompido */
        }
        if (c->next && c->next->prev != c) { os_unlock(); return -1; }
        if (c->free) freebytes += (int32_t)c->size;
    }
    os_unlock();
    return freebytes;
}

void *OSAllocFromCurrentHeap(uint32_t size) { return OSAllocFromHeap(g_current_heap, size); }
void  OSFreeToCurrentHeap(void *ptr)        { OSFreeToHeap(g_current_heap, ptr); }

void *OSAllocFixed(void **rangeStart, void **rangeEnd) {
    /* reserva [*rangeStart, *rangeEnd) da arena MEM1, avançando o lo. */
    void *ret = 0;
    os_lock();
    arena_ensure(&g_mem1, QVX_OS_MEM1_DEFAULT);
    char *s = (char *)(*rangeStart), *e = (char *)(*rangeEnd);
    if (s >= g_mem1.lo && e <= g_mem1.hi && e > s) {
        g_mem1.lo = e;
        ret = s;
    }
    os_unlock();
    return ret;
}

/* ========================================================================== *
 *  4) interrupts / scheduler (stubs seguros)
 * ========================================================================== */
/* Estado fake coerente: um flag de "interrupts habilitados" por processo. Não
   há IRQs reais a mascarar aqui; o motor só precisa do protocolo
   disable->(seção crítica)->restore devolver o estado anterior certo. */
static int g_intr_enabled = 1;
static int g_sched_disabled = 0;

int OSDisableInterrupts(void)       { os_lock(); int prev = g_intr_enabled; g_intr_enabled = 0; os_unlock(); return prev; }
int OSEnableInterrupts(void)        { os_lock(); int prev = g_intr_enabled; g_intr_enabled = 1; os_unlock(); return prev; }
int OSRestoreInterrupts(int level)  { os_lock(); int prev = g_intr_enabled; g_intr_enabled = level ? 1 : 0; os_unlock(); return prev; }
int OSDisableScheduler(void)        { os_lock(); int prev = g_sched_disabled; g_sched_disabled++; os_unlock(); return prev; }
int OSEnableScheduler(void)         { os_lock(); int prev = g_sched_disabled; if (g_sched_disabled > 0) g_sched_disabled--; os_unlock(); return prev; }

/* ========================================================================== *
 *  5) threads / sync — real sobre scePthread/pthread, com side tables
 * ========================================================================== */
#ifndef QVX_OS_MAX_THREADS
#  define QVX_OS_MAX_THREADS 128
#endif
#ifndef QVX_OS_MAX_MUTEXES
#  define QVX_OS_MAX_MUTEXES 256
#endif
#ifndef QVX_OS_MAX_QUEUES
#  define QVX_OS_MAX_QUEUES  64
#endif
#ifndef QVX_OS_MAX_TQUEUES
#  define QVX_OS_MAX_TQUEUES 64
#endif

/* --- threads --- */
typedef struct {
    int           used;
    OSThread     *key;          /* OSThread* que o motor passou (chave) */
    qos_thread_t  handle;
    int           has_handle;   /* 0 p/ a main thread registrada à parte */
    OSThreadFunc  func;
    void         *param;
    void         *retval;
    OSPriority    prio;
    int           suspend;      /* suspend count; >0 = suspensa */
    int           started;      /* já passou do start gate */
    int           terminated;
    int           retired;      /* slot aposentado (chave reusada): some do lookup */
    qos_cond_t    gate;         /* libera a thread quando suspend chega a 0 */
} os_thread_slot;

static os_thread_slot g_threads[QVX_OS_MAX_THREADS];
static OSThread       g_main_cookie_storage;   /* stand-in da main thread */
static os_thread_slot *g_main_slot = 0;

static os_thread_slot *thr_find(OSThread *key) {
    for (int i = 0; i < QVX_OS_MAX_THREADS; i++)
        if (g_threads[i].used && !g_threads[i].retired && g_threads[i].key == key)
            return &g_threads[i];
    return 0;
}
/* Reaproveita a chave: um OSThread reusado (objeto estático comum) NÃO pode
   gerar um 2º slot — senão OSResume/OSJoin operariam no slot morto e a thread
   nova ficaria presa no start gate. Dedup antes de alocar. */
static os_thread_slot *thr_alloc(OSThread *key) {
    os_thread_slot *old = thr_find(key);
    if (old) {
        if (old->terminated) {
            /* thread anterior já encerrou: reaproveita a memória do slot.
               detach reclama a pthread zumbi (se não foi detached/joined). */
            if (old->has_handle) qos_thread_detach(old->handle);
            qos_cond_destroy(&old->gate);
            memset(old, 0, sizeof(*old));
            old->used = 1; old->key = key;
            return old;
        }
        /* thread anterior ainda viva na mesma chave (reuso indevido): aposenta
           o slot (some do lookup), força a velha a abortar/drenar e deixa o
           trampoline reclamar a memória dela ao sair. Não mexemos mais na
           memória do slot velho aqui — o trampoline ainda a referencia. */
        if (old->has_handle) qos_thread_detach(old->handle);
        old->retired = 1;
        old->terminated = 1;
        if (old->suspend > 0) { old->suspend = 0; qos_cond_broadcast(&old->gate); }
    }
    for (int i = 0; i < QVX_OS_MAX_THREADS; i++)
        if (!g_threads[i].used) { memset(&g_threads[i], 0, sizeof(os_thread_slot));
            g_threads[i].used = 1; g_threads[i].key = key; return &g_threads[i]; }
    return 0;
}
static os_thread_slot *thr_find_by_handle(qos_thread_t h) {
    for (int i = 0; i < QVX_OS_MAX_THREADS; i++)
        if (g_threads[i].used && !g_threads[i].retired && g_threads[i].has_handle &&
            qos_thread_equal(g_threads[i].handle, h)) return &g_threads[i];
    return 0;
}

static void *os_thread_trampoline(void *arg) {
    os_thread_slot *s = (os_thread_slot *)arg;
    /* start gate: espera até suspend count chegar a 0 (OSCreateThread deixa a
       thread "suspensa" conforme attr; OSResumeThread a libera). */
    os_lock();
    while (s->suspend > 0 && !s->terminated) qos_cond_wait(&s->gate, &g_os_lock);
    int aborted = s->terminated;   /* cancelada/aposentada antes de rodar */
    s->started = 1;
    OSThreadFunc f = s->func; void *p = s->param;
    os_unlock();

    void *r = 0;
    if (f && !aborted) r = f(p);

    os_lock();
    s->retval = r; s->terminated = 1;
    if (s->retired) {           /* slot aposentado por reuso: reclama a memória */
        qos_cond_destroy(&s->gate);
        memset(s, 0, sizeof(*s));
    }
    os_unlock();
    return r;
}

int OSCreateThread(OSThread *thread, OSThreadFunc func, void *param,
                   void *stackBase, uint32_t stackSize, OSPriority prio, uint16_t attr) {
    (void)stackBase; (void)stackSize;
    os_lock();
    os_thread_slot *s = thr_alloc(thread);
    if (!s) { os_unlock(); OSReport("OSCreateThread: side table cheia (%d)\n", QVX_OS_MAX_THREADS); return 0; }
    qos_cond_init(&s->gate);
    s->func = func; s->param = param; s->prio = prio; s->has_handle = 1;
    /* attr bit 0 (OS_THREAD_ATTR_DETACH) => detached; o jogo normalmente cria
       suspensa e chama OSResumeThread. suspend=1 segura no start gate. */
    s->suspend = 1;
    int rc = qos_thread_create(&s->handle, os_thread_trampoline, s);
    if (rc != 0) { s->used = 0; os_unlock(); OSReport("OSCreateThread: create falhou rc=%d\n", rc); return 0; }
    if (attr & 0x1) qos_thread_detach(s->handle);
    os_unlock();
    return 1;
}

int OSResumeThread(OSThread *thread) {
    os_lock();
    os_thread_slot *s = thr_find(thread);
    int prev = 0;
    if (s) { prev = s->suspend; if (s->suspend > 0) s->suspend--; if (s->suspend == 0) qos_cond_broadcast(&s->gate); }
    os_unlock();
    return prev;
}

int OSSuspendThread(OSThread *thread) {
    /* Suspensão cooperativa: incrementa o count. Só é honrada no start gate
       (antes de a thread rodar). Suspender uma thread JÁ rodando não é suportado
       de forma portável e preemptiva — ver RELATORIO (risco documentado). */
    os_lock();
    os_thread_slot *s = thr_find(thread);
    int prev = 0;
    if (s) { prev = s->suspend; s->suspend++; }
    os_unlock();
    return prev;
}

int OSJoinThread(OSThread *thread, void **val) {
    os_lock();
    os_thread_slot *s = thr_find(thread);
    if (!s) { os_unlock(); return 0; }
    /* se ainda suspensa no gate, libera para poder terminar e dar join */
    if (s->suspend > 0) { s->suspend = 0; qos_cond_broadcast(&s->gate); }
    qos_thread_t h = s->handle; int has = s->has_handle;
    os_unlock();
    void *r = 0;
    if (has) qos_thread_join(h, &r);
    if (val) *val = r;
    return 1;
}

int OSDetachThread(OSThread *thread) {
    os_lock();
    os_thread_slot *s = thr_find(thread);
    int ok = 0;
    if (s && s->has_handle) { qos_thread_detach(s->handle); ok = 1; }
    os_unlock();
    return ok;
}

void OSCancelThread(OSThread *thread) {
    /* Cancelamento assíncrono de threads nativas é arriscado (locks presos).
       Marcamos como terminada e liberamos o gate; não forçamos kill. */
    os_lock();
    os_thread_slot *s = thr_find(thread);
    if (s) { s->terminated = 1; if (s->suspend > 0) { s->suspend = 0; qos_cond_broadcast(&s->gate); } }
    os_unlock();
}

void OSYieldThread(void) { qos_yield(); }

OSThread *OSGetCurrentThread(void) {
    os_lock();
    os_thread_slot *s = thr_find_by_handle(qos_thread_self());
    if (!s) {
        /* main thread (não criada via OSCreateThread): registra um stand-in. */
        if (!g_main_slot) {
            g_main_slot = thr_alloc(&g_main_cookie_storage);
            if (g_main_slot) {
                g_main_slot->handle = qos_thread_self();
                g_main_slot->has_handle = 1; g_main_slot->started = 1; g_main_slot->prio = 16;
            }
        }
        s = g_main_slot;
    }
    OSThread *key = s ? s->key : &g_main_cookie_storage;
    os_unlock();
    return key;
}

OSPriority OSGetThreadPriority(OSThread *thread) {
    os_lock(); os_thread_slot *s = thr_find(thread); OSPriority p = s ? s->prio : 16; os_unlock(); return p;
}
int OSSetThreadPriority(OSThread *thread, OSPriority prio) {
    os_lock(); os_thread_slot *s = thr_find(thread); if (s) s->prio = prio; int ok = s ? 1 : 0; os_unlock(); return ok;
}
int OSIsThreadSuspended(OSThread *thread) {
    os_lock(); os_thread_slot *s = thr_find(thread); int r = (s && s->suspend > 0); os_unlock(); return r;
}
int OSIsThreadTerminated(OSThread *thread) {
    os_lock(); os_thread_slot *s = thr_find(thread); int r = (s && s->terminated); os_unlock(); return r;
}

/* --- thread queues (OSSleepThread/OSWakeupThread) --- */
typedef struct { int used; OSThreadQueue *key; qos_cond_t cv; int waiters; } os_tqueue_slot;
static os_tqueue_slot g_tqueues[QVX_OS_MAX_TQUEUES];

static os_tqueue_slot *tq_get(OSThreadQueue *key) {
    for (int i = 0; i < QVX_OS_MAX_TQUEUES; i++) if (g_tqueues[i].used && g_tqueues[i].key == key) return &g_tqueues[i];
    for (int i = 0; i < QVX_OS_MAX_TQUEUES; i++) if (!g_tqueues[i].used) {
        g_tqueues[i].used = 1; g_tqueues[i].key = key; g_tqueues[i].waiters = 0;
        qos_cond_init(&g_tqueues[i].cv); return &g_tqueues[i];
    }
    return 0;
}

void OSInitThreadQueue(OSThreadQueue *queue) { os_lock(); tq_get(queue); os_unlock(); }

void OSSleepThread(OSThreadQueue *queue) {
    os_lock();
    os_tqueue_slot *q = tq_get(queue);
    if (q) { q->waiters++; qos_cond_wait(&q->cv, &g_os_lock); q->waiters--; }
    os_unlock();
}
void OSWakeupThread(OSThreadQueue *queue) {
    os_lock();
    os_tqueue_slot *q = tq_get(queue);
    if (q) qos_cond_broadcast(&q->cv);
    os_unlock();
}

/* --- mutex --- */
typedef struct { int used; OSMutex *key; qos_mutex_t m; } os_mutex_slot;
static os_mutex_slot g_mutexes[QVX_OS_MAX_MUTEXES];

static os_mutex_slot *mtx_get(OSMutex *key) {
    for (int i = 0; i < QVX_OS_MAX_MUTEXES; i++) if (g_mutexes[i].used && g_mutexes[i].key == key) return &g_mutexes[i];
    for (int i = 0; i < QVX_OS_MAX_MUTEXES; i++) if (!g_mutexes[i].used) {
        g_mutexes[i].used = 1; g_mutexes[i].key = key;
        qos_mutex_init(&g_mutexes[i].m, 1 /*RVL OSMutex é reentrante*/);
        return &g_mutexes[i];
    }
    return 0;
}

void OSInitMutex(OSMutex *mutex) { os_lock(); mtx_get(mutex); os_unlock(); }
void OSLockMutex(OSMutex *mutex) {
    os_lock(); os_mutex_slot *s = mtx_get(mutex); os_unlock();
    if (s) qos_mutex_lock(&s->m);
}
void OSUnlockMutex(OSMutex *mutex) {
    os_lock(); os_mutex_slot *s = mtx_get(mutex); os_unlock();
    if (s) qos_mutex_unlock(&s->m);
}
int OSTryLockMutex(OSMutex *mutex) {
    os_lock(); os_mutex_slot *s = mtx_get(mutex); os_unlock();
    if (!s) return 0;
#if QVX_OS_BACKEND_ORBIS
    return scePthreadMutexTrylock(&s->m) == 0;
#else
    return pthread_mutex_trylock(&s->m) == 0;
#endif
}

/* --- message queue: ring buffer bounded com mutex + cond --- */
typedef struct {
    int         used;
    OSMessageQueue *key;
    OSMessage  *buf;        /* array fornecido pelo jogo */
    int32_t     cap, count, head;
    qos_mutex_t m;
    qos_cond_t  not_empty, not_full;
} os_mq_slot;
static os_mq_slot g_queues[QVX_OS_MAX_QUEUES];

static os_mq_slot *mq_find(OSMessageQueue *key) {
    for (int i = 0; i < QVX_OS_MAX_QUEUES; i++) if (g_queues[i].used && g_queues[i].key == key) return &g_queues[i];
    return 0;
}

void OSInitMessageQueue(OSMessageQueue *mq, OSMessage *msgArray, int32_t count) {
    os_lock();
    os_mq_slot *q = mq_find(mq);
    if (!q) for (int i = 0; i < QVX_OS_MAX_QUEUES; i++) if (!g_queues[i].used) { q = &g_queues[i]; q->used = 1; q->key = mq;
        qos_mutex_init(&q->m, 0); qos_cond_init(&q->not_empty); qos_cond_init(&q->not_full); break; }
    if (q) { q->buf = msgArray; q->cap = count; q->count = 0; q->head = 0; }
    else OSReport("OSInitMessageQueue: side table cheia (%d)\n", QVX_OS_MAX_QUEUES);
    os_unlock();
}

int OSSendMessage(OSMessageQueue *mq, OSMessage msg, int32_t flags) {
    os_lock(); os_mq_slot *q = mq_find(mq); os_unlock();
    if (!q) return 0;
    qos_mutex_lock(&q->m);
    while (q->count >= q->cap) {
        if (!(flags & OS_MESSAGE_BLOCK)) { qos_mutex_unlock(&q->m); return 0; }
        qos_cond_wait(&q->not_full, &q->m);
    }
    q->buf[(q->head + q->count) % q->cap] = msg;
    q->count++;
    qos_cond_signal(&q->not_empty);
    qos_mutex_unlock(&q->m);
    return 1;
}

int OSJamMessage(OSMessageQueue *mq, OSMessage msg, int32_t flags) {
    os_lock(); os_mq_slot *q = mq_find(mq); os_unlock();
    if (!q) return 0;
    qos_mutex_lock(&q->m);
    while (q->count >= q->cap) {
        if (!(flags & OS_MESSAGE_BLOCK)) { qos_mutex_unlock(&q->m); return 0; }
        qos_cond_wait(&q->not_full, &q->m);
    }
    q->head = (q->head + q->cap - 1) % q->cap;   /* insere na frente */
    q->buf[q->head] = msg;
    q->count++;
    qos_cond_signal(&q->not_empty);
    qos_mutex_unlock(&q->m);
    return 1;
}

int OSReceiveMessage(OSMessageQueue *mq, OSMessage *out, int32_t flags) {
    os_lock(); os_mq_slot *q = mq_find(mq); os_unlock();
    if (!q) return 0;
    qos_mutex_lock(&q->m);
    while (q->count <= 0) {
        if (!(flags & OS_MESSAGE_BLOCK)) { qos_mutex_unlock(&q->m); return 0; }
        qos_cond_wait(&q->not_empty, &q->m);
    }
    if (out) *out = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    qos_cond_signal(&q->not_full);
    qos_mutex_unlock(&q->m);
    return 1;
}

/* ========================================================================== *
 *  6) alarm — stub seguro (registra, NÃO dispara). Ver RELATORIO (risco).
 * ========================================================================== */
/* OSSetPeriodicAlarm/OSCreateAlarm são linkados pelo motor de verdade (ver
   stubs.S). Um alarme periódico que o boot espere em laço (timing de áudio/DVD,
   watchdog) travaria se nunca disparasse — por isso DISPARA mesmo: cada alarme
   periódico ganha uma thread de timer que chama o handler na cadência pedida.

   Contexto: o handler RVL tem assinatura (OSAlarm*, OSContext*). Não temos o
   layout do OSContext (não o inventamos — cena limpa), então passamos um buffer
   zerado em vez de NULL: um handler que LEIA um campo pega 0 em vez de
   segfault. Ver RELATORIO (risco de o handler derreferenciar o contexto). */
typedef struct {
    int           used, active, gen;
    OSAlarm      *key;
    OSAlarmHandler handler;
    OSTime        start, period;
} os_alarm_slot;
#ifndef QVX_OS_MAX_ALARMS
#  define QVX_OS_MAX_ALARMS 64
#endif
static os_alarm_slot    g_alarms[QVX_OS_MAX_ALARMS];
static unsigned char    g_fake_ctx[256];   /* "OSContext" falso, zerado */

static os_alarm_slot *al_get(OSAlarm *key) {
    for (int i = 0; i < QVX_OS_MAX_ALARMS; i++) if (g_alarms[i].used && g_alarms[i].key == key) return &g_alarms[i];
    for (int i = 0; i < QVX_OS_MAX_ALARMS; i++) if (!g_alarms[i].used) { g_alarms[i].used = 1; g_alarms[i].key = key; return &g_alarms[i]; }
    return 0;
}

/* ticks de 60.75 MHz -> microsegundos (exato, sem estouro). */
static uint64_t ticks_to_usec(OSTime ticks) {
    if (ticks <= 0) return 0;
    uint64_t t = (uint64_t)ticks, tc = (uint64_t)QVX_OS_TIMER_CLOCK;
    return (t / tc) * 1000000ull + ((t % tc) * 1000000ull) / tc;
}

/* Uma thread por alarme periódico. O token `gen` aposenta timers antigos quando
   o alarme é reconfigurado/cancelado: o loop sai assim que gen muda. */
static void *alarm_timer(void *arg) {
    os_alarm_slot *a = (os_alarm_slot *)arg;
    os_lock();
    int mygen = a->gen;
    uint64_t us = ticks_to_usec(a->period);
    os_unlock();
    if (us == 0) return 0;              /* período inválido: não dispara */
    if (us > 0xFFFFFFFFull) us = 0xFFFFFFFFull;
    for (;;) {
        qos_usleep((uint32_t)us);
        os_lock();
        int live = a->active && a->gen == mygen;
        OSAlarmHandler h = a->handler; OSAlarm *k = a->key;
        us = ticks_to_usec(a->period);
        os_unlock();
        if (!live || us == 0) break;
        if (us > 0xFFFFFFFFull) us = 0xFFFFFFFFull;
        if (h) h(k, (void *)g_fake_ctx);
    }
    return 0;
}

static void alarm_start_timer(os_alarm_slot *a) {
    qos_thread_t t;
    if (qos_thread_create(&t, alarm_timer, a) == 0) qos_thread_detach(t);
    else OSReport("OSSetPeriodicAlarm: timer thread falhou\n");
}

void OSCreateAlarm(OSAlarm *alarm) {
    os_lock();
    os_alarm_slot *a = al_get(alarm);
    if (a) { a->handler = 0; a->period = 0; a->active = 0; a->gen++; }  /* aposenta timer anterior */
    os_unlock();
}

void OSSetPeriodicAlarm(OSAlarm *alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    int launch = 0;
    os_lock();
    os_alarm_slot *a = al_get(alarm);
    if (a) {
        a->start = start; a->period = period; a->handler = handler;
        a->active = 1; a->gen++;            /* aposenta qualquer timer antigo */
        if (period > 0 && handler) launch = 1;
    } else OSReport("OSSetPeriodicAlarm: side table cheia (%d)\n", QVX_OS_MAX_ALARMS);
    os_alarm_slot *slot = a;
    os_unlock();
    if (launch) alarm_start_timer(slot);
}

void OSCancelAlarm(OSAlarm *alarm) {
    os_lock();
    os_alarm_slot *a = al_get(alarm);
    if (a) { a->active = 0; a->gen++; }      /* timer sai no próximo ciclo */
    os_unlock();
}

/* ========================================================================== *
 *  7) power / reset — terminal: loga e entra em halt loop controlado
 * ========================================================================== */
static void *g_power_cb = 0;

int   OSGetResetButtonState(void) { return 0; }   /* nunca pressionado */
void *OSSetPowerCallback(void *callback) { void *prev = g_power_cb; g_power_cb = callback; return prev; }
void  OSRebootSystem(void)   { OSReport("OSRebootSystem chamado (bring-up: halt)\n"); os_halt_loop(); }
void  OSRestart(uint32_t r)  { (void)r; OSReport("OSRestart chamado (bring-up: halt)\n"); os_halt_loop(); }
void  OSReturnToMenu(void)   { OSReport("OSReturnToMenu chamado (bring-up: halt)\n"); os_halt_loop(); }
void  OSShutdownSystem(void) { OSReport("OSShutdownSystem chamado (bring-up: halt)\n"); os_halt_loop(); }
