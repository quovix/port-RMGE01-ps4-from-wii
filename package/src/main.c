/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Casca v0 do produto: boota no PS4, anuncia-se e roda o fluxo de first-run
   (procurar + verificar a mídia do usuário). Sem motor ainda: mount = stub.
   Nenhum asset ou arte da Nintendo é incluído. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include "../../product/media/media_verify.h"
#include "../../product/media/manifest_rmge01.h"
#include "../../runtime/compat/heap_smoke.h"
#include "../../runtime/compat/scalar_math.h"
#include "../../runtime/video/video_out.h"
#include "../../runtime/video/text.h"
#include "../../product/art/art_splash.h"
#include "../../runtime/platform/orbis/backend_bringup.h"

/* --- Canal de fase lido AO VIVO via ps4debug (ler memória sem attach). --------
   Metodologia autônoma: não depende da tela (crash cobre), nem de fopen em /data
   (sandbox não deixa gravar onde o FTP lê), nem do klog(3232) (mudo p/ este app).
   g_qvx_phase é uma global em .data num endereço estável (0x400000 + offset do
   nm); um poller no host lê este buffer em loop enquanto o app roda e imprime a
   última fase antes do processo morrer -> é o passo que crashou. @orig none. */
volatile char g_qvx_phase[96] = "phase:boot";
void qvx_set_phase(const char *s) {   /* não-static: OSPanic/OSHalt (os.c) usam */
    unsigned i = 0;
    for (; i < sizeof(g_qvx_phase) - 1 && s[i]; i++) g_qvx_phase[i] = s[i];
    g_qvx_phase[i] = 0;
}

/* Beacon em ARQUIVO — canal NÃO-PROVADO (sandbox pode barrar o fopen). Mantido
   por redundância; a fonte de verdade agora é g_qvx_phase via ps4debug. */
static void qvx_beacon(const char *msg) {
    FILE *f = fopen("/data/quovix-boot.log", "a");
    if (!f) return;
    fputs(msg, f);
    fputc('\n', f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

/* --- Handler de crash: determinístico, roda DENTRO do processo no fault. ------
   O catch-live via ps4debug falha (processo reapado rápido demais) e o coredump
   é encriptado. Então instalamos um handler que, no SIGSEGV, varre a própria
   stack procurando return addresses no .text do eboot (0x400000..0xb74000) — o
   backtrace real — e dumpa via klog + beacon. Diz EXATAMENTE quem chamou o alvo
   corrompido 0x4043ef. @orig none. */
#define QVX_TEXT_LO 0x400000UL
#define QVX_TEXT_HI 0xbe0000UL   /* .text runtime vai ate ~0xbdc000 (ver mapas) */
static void qvx_dump(const char *s) {
    sceKernelDebugOutText(0, s);
    sceKernelDebugOutText(0, "\n");
    qvx_beacon(s);
}
/* Handler de crash: escreve o fault no g_qvx_phase (canal PROVADO via ps4debug)
   e SEGURA o processo vivo num spin, pra o poller ler com folga (sticky). Usa
   SA_SIGINFO pra pegar o endereco faltante (si_addr) + varre a stack pelos
   return addresses no .text (os callers). off = runtime - 0x400000 = vaddr do
   elf -> mapeia direto a um simbolo com nm/addr2line. @orig none. */
static volatile int g_in_fault = 0;
static char g_sigstk[65536];   /* pilha alternativa p/ pegar stack-overflow */
static void qvx_crash_handler(int sig, siginfo_t *info, void *uctx) {
    (void)uctx;
    /* primeira falha vence: colaterais não sobrescrevem o diagnóstico */
    if (__atomic_exchange_n(&g_in_fault, 1, __ATOMIC_SEQ_CST)) {
        for (;;) sceKernelUsleep(1000000);
    }
    unsigned long faddr = (unsigned long)(info ? (unsigned long)info->si_addr : 0UL);
    char b[96];
    /* 1) ESCREVE A FASE PRIMEIRO — antes de qualquer varredura que possa
       re-faultar (a varredura de 128KB numa stack rasa matava o processo
       ANTES de gravar; por isso o handler parecia "não disparar"). */
    snprintf(b, sizeof b, "FAULT s%d a=0x%lx", sig, faddr);
    qvx_set_phase(b);
    /* 2) varredura CURTA e limitada (512 words) pelos callers no .text */
    unsigned long bt[2]; int hits = 0;
    volatile unsigned long marker = 0;
    unsigned long *sp = (unsigned long *)((void *)&marker);
    for (int i = 0; i < 512 && hits < 2; i++) {
        unsigned long v = sp[i];
        if (v >= QVX_TEXT_LO && v < QVX_TEXT_HI) bt[hits++] = v;
    }
    snprintf(b, sizeof b, "FAULT s%d a=0x%lx c0=%lx c1=%lx", sig, faddr,
             hits > 0 ? bt[0] - QVX_TEXT_LO : 0UL,
             hits > 1 ? bt[1] - QVX_TEXT_LO : 0UL);
    qvx_set_phase(b);
    /* tambem no stdout: no emulador (shadPS4) o SIGSEGV do guest chega aqui e
       o fault address + callers (vaddr do elf) saem no log -> simbolizar. No
       hardware o canal e' mudo, entao isto e' inofensivo la. */
    sceKernelDebugOutText(0, "[quovix] ");
    sceKernelDebugOutText(0, b);
    sceKernelDebugOutText(0, "\n");
    /* 3) segura vivo ~20s pro poller ler (usleep, sem busy-spin; sem beacon/
       fopen que não são async-signal-safe) */
    for (int i = 0; i < 40; i++) sceKernelUsleep(500000);
    _exit(42);
}
static void qvx_install_crash_handler(void) {
    stack_t ss; memset(&ss, 0, sizeof ss);
    ss.ss_sp = g_sigstk; ss.ss_size = sizeof g_sigstk; ss.ss_flags = 0;
    sigaltstack(&ss, 0);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    /* macro sa_sigaction do OpenOrbis esta quebrada (aponta p/ membro inexistente
       .sa_sigaction); usa o campo real __sa_handler.__sa_sigaction. */
    sa.__sa_handler.__sa_sigaction = qvx_crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;   /* ONSTACK: pega stack-overflow */
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS,  &sa, 0);
    sigaction(SIGILL,  &sa, 0);
    sigaction(SIGABRT, &sa, 0);
    sigaction(SIGFPE,  &sa, 0);
}

/* Log no klog (capturável via GoldHEN:3232) + beacon em arquivo. Instrumenta
   cada passo pra localizar exatamente onde um eventual crash acontece. */
static void klog(const char *msg) {
    qvx_set_phase(msg);           /* FONTE DE VERDADE: lido ao vivo via ps4debug */
    sceKernelDebugOutText(0, "[quovix] ");
    sceKernelDebugOutText(0, msg);
    sceKernelDebugOutText(0, "\n");
    qvx_beacon(msg);              /* redundância em disco (não-provado) */
    qvx_log_line(msg);            /* também aparece na tela (se houver vídeo) */
}

/* Notificação on-screen (prova visível de boot).
   OrbisNotificationRequest vem de <orbis/_types/kernel.h> (via libkernel.h). */
static void notify(const char *msg) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof req);
    req.type = 0;
    req.targetId = -1;
    req.useIconImageUri = 0;   /* sem ícone custom por enquanto */
    strncpy(req.message, msg, sizeof(req.message) - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

/* Procura a mídia do usuário em locais típicos (USB/HDD).
   Retorna caminho do sys/main.dol encontrado, ou NULL. */
static const char *find_user_media(void) {
    static const char *candidates[] = {
        "/mnt/usb0/RMGE01/sys/main.dol",
        "/mnt/usb1/RMGE01/sys/main.dol",
        "/data/quovix/RMGE01/sys/main.dol",
        NULL,
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], R_OK) == 0) return candidates[i];
    }
    return NULL;
}

/* Buffer da splash (lida do dump do usuário em runtime; BYO-dump) e estado de
   render, em escopo de arquivo pra o helper de frame enxergar. */
static uint32_t g_splash[512 * 288];
static uint32_t g_sw = 0, g_sh = 0;
static int      g_have_splash = 0;

/* Desenha o frame padrão (fundo + splash se houver + título + log on-screen) e
   apresenta. A TELA é o único canal de diagnóstico PROVADO nesta plataforma:
   klog(3232) nunca capturou saída deste app, o beacon em /data é não-provado e
   o handler de sigaction provavelmente não dispara no Orbis. Então chamamos isto
   depois de CADA passo — a última linha visível na tela localiza o fault.
   (Revisão independente 2026-10-12.) */
static void qvx_present_frame(void) {
    qvx_video_clear(0xFF0A1020);                 /* azul-petróleo escuro */
    if (g_have_splash) qvx_video_blit_centered(g_splash, (int)g_sw, (int)g_sh);
    qvx_text_draw(20, 16, "QUOVIX  [PORT][RMGE01][PS4<-WII]", 0xFFFFFFFF, 2);
    qvx_log_render(20, 52, 0xFF80FF80, 2);       /* log on-screen (verde) */
    qvx_video_present();
}

/* Loga (tela + klog + beacon) e re-apresenta o frame. Só use depois do
   video-out estar ativo. Não-static: backend_bringup.c chama isto por sub-passo
   (declarado em backend_bringup.h). */
void qvx_boot_tick(const char *msg) {
    klog(msg);
    qvx_present_frame();
}

int main(void) {
    /* 1ª coisa: marca a fase ANTES de qualquer chamada que possa crashar
       (handler, fopen, video). O poller via ps4debug lê g_qvx_phase ao vivo. */
    qvx_set_phase("phase:main-top");
    /* Handler de crash mantido (provavelmente não dispara no Orbis; inofensivo).
       Beacon truncado por garantia — canal NÃO-PROVADO, não dependemos dele. */
    qvx_install_crash_handler();
    { FILE *f = fopen("/data/quovix-boot.log", "w");
      if (f) { fputs("=== quovix boot beacon ===\n", f); fflush(f); fsync(fileno(f)); fclose(f); } }
    klog("boot: main entrou");
    sceSystemServiceHideSplashScreen();

    /* VÍDEO PRIMEIRO. É o único canal de diagnóstico que funciona aqui, então
       subimos antes de qualquer trabalho que possa crashar e usamos a tela como
       log a partir daqui. Sem vídeo não há como mostrar nada: sinaliza e para. */
    if (qvx_video_init() != 0) {
        klog("video: FALHA no init; sem canal de tela");
        notify("Falha ao abrir video-out. Consulte o log.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    klog("video: ativo");
    qvx_present_frame();                          /* PRIMEIRO FRAME visível já aqui */

    /* Splash do dump do usuário (BYO-dump); fallback = fundo cósmico próprio.
       Carga de disco é opcional e não bloqueia o frame já mostrado. */
    g_have_splash = (qvx_art_load_tpl_from_arc(
        "/data/quovix/RMGE01/files/LayoutData/TitleLogo.arc", "mytitlelogo.tpl",
        g_splash, 512 * 288, &g_sw, &g_sh) == 0);
    qvx_boot_tick(g_have_splash ? "splash: carregada do dump" : "splash: fundo cosmico (sem dump)");

    qvx_boot_tick("heap: autoteste (tamanho nativo e alinhamento)...");
    if (qvx_heap_smoke_test() != 0) {
        qvx_boot_tick("heap: FALHA no autoteste");
        notify("Falha na inicializacao da casca.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    qvx_boot_tick("heap: OK");

    qvx_boot_tick("math: autoteste escalar...");
    if (qvx_math_smoke_test() != 0) {
        qvx_boot_tick("math: FALHA no autoteste");
        notify("Falha na inicializacao da casca.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    qvx_boot_tick("math: OK");

    qvx_boot_tick("media: procurando dump do usuario...");
    const char *media = find_user_media();
    qvx_boot_tick(media ? "media: encontrada" : "media: ausente (BYO-dump)");
    if (media) {
        qvx_boot_tick("media: verificando (sha1 de 6MB)...");
        qvx_mv_result r = qvx_mv_verify_maindol(media);
        qvx_boot_tick(r == QVX_MV_OK ? "media: RMGE01 OK" : "media: invalida");
    }

    /* Backend bring-up DEPOIS da splash já estar na tela. DVDInit/walk_dir são
       defensivos p/ dump ausente; se mesmo assim travar, a última linha
       on-screen ("backend: ...") aponta o passo exato. */
    qvx_boot_tick("backend: bring-up orbis (MTX + GX/VI + OS + DVD)...");
    int bk = qvx_backend_bringup();
    qvx_boot_tick(bk == 0 ? "backend: ONLINE" : "backend: bring-up com falhas");

#ifdef QVX_ENGINE_JUMP
    /* Os construtores estáticos globais do MOTOR foram separados do DT_INIT_ARRAY
       pelo link script (.engine_init_array), então o loader NÃO os roda antes do
       main — nós rodamos aqui, logando CADA um na TELA antes de chamar. Se um
       crashar, a última linha '[ctor #i]' visível é o culpado. (Log por-ctor é
       só on-screen + klog, sem fsync de beacon, pra não martelar o FS 627x.) */
    extern void qvx_game_entry(void) __asm__("_Z14qvx_game_entryv");
    extern char __qvx_einit_start[], __qvx_einit_end[];
    typedef void (*qvx_ctor)(void);
    qvx_ctor *cb = (qvx_ctor *)(void *)__qvx_einit_start;
    qvx_ctor *ce = (qvx_ctor *)(void *)__qvx_einit_end;
    int nctor = (int)(ce - cb);
    char el[112];
    snprintf(el, sizeof el, "engine: %d ctors; rodando com log on-screen...", nctor);
    qvx_boot_tick(el);
    /* Vincula o alocador do jogo ANTES de rodar ctors/qvx_game_entry: o smoke
       test deixou a ponte desligada, e o 1º `new (pHeap,align)` do motor
       abortaria (SIGABRT) sem backend. */
    if (qvx_heap_bind_game() != 0)
        qvx_boot_tick("heap: FALHA ao vincular alocador do jogo");
    else
        qvx_boot_tick("heap: alocador do jogo vinculado (persistente)");
    for (int i = 0; i < nctor; i++) {
        snprintf(el, sizeof el, "phase:ctor #%d/%d fn=%p", i, nctor, (void *)cb[i]);
        qvx_set_phase(el);                /* FONTE DE VERDADE (ps4debug): culpado = último i */
        qvx_log_line(el);
        sceKernelDebugOutText(0, el);     /* klog (sem fsync) */
        sceKernelDebugOutText(0, "\n");
        qvx_present_frame();
        cb[i]();
    }
    qvx_boot_tick("engine: ctors TODOS OK; saltando p/ qvx_game_entry");
    qvx_game_entry();
    qvx_boot_tick("engine-jump: qvx_game_entry RETORNOU (inesperado nesta fase)");
#endif

    for (;;) {
        qvx_video_present();                        /* flip entre buffers idênticos */
    }
    return 0;
}
