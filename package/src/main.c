/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Casca v0 do produto: boota no PS4, anuncia-se e roda o fluxo de first-run
   (procurar + verificar a mídia do usuário). Sem motor ainda: mount = stub.
   Nenhum asset ou arte da Nintendo é incluído. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

#include "../../product/media/media_verify.h"
#include "../../product/media/manifest_rmge01.h"
#include "../../runtime/compat/heap_smoke.h"
#include "../../runtime/compat/scalar_math.h"
#include "../../runtime/video/video_out.h"
#include "../../runtime/video/text.h"
#include "../../product/art/art_splash.h"

/* Log no klog (capturável via GoldHEN:3232). Instrumenta cada passo pra
   localizar exatamente onde um eventual crash acontece. */
static void klog(const char *msg) {
    sceKernelDebugOutText(0, "[quovix] ");
    sceKernelDebugOutText(0, msg);
    sceKernelDebugOutText(0, "\n");
    qvx_log_line(msg);            /* também aparece na tela */
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

int main(void) {
    klog("boot: main entrou");

    klog("passo 1: hide splash screen...");
    sceSystemServiceHideSplashScreen();
    klog("passo 1 ok");

    klog("heap: autoteste de tamanho nativo e alinhamento...");
    if (qvx_heap_smoke_test() != 0) {
        klog("heap: FALHA no autoteste; inicializacao interrompida");
        notify("Falha na inicializacao da casca. Consulte o log.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    klog("heap: autoteste OK (scalar/array, 64/128 bytes de alinhamento)");

    klog("math: autoteste de conversoes e operacoes escalares...");
    if (qvx_math_smoke_test() != 0) {
        klog("math: FALHA no autoteste; inicializacao interrompida");
        notify("Falha na inicializacao da casca. Consulte o log.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    klog("math: autoteste OK (65536 valores u16 e casos de borda)");

    klog("passo 2: notify...");
    notify("quovix — casca v0 iniciada (" QVX_GAME_ID ")");
    klog("passo 2 ok");

    klog("passo 3: find media...");
    const char *media = find_user_media();
    klog(media ? "passo 3 ok: media encontrada" : "passo 3 ok: media ausente");

    if (!media) {
        notify("Mídia não encontrada. Coloque seu dump RMGE01 (BYO-dump).");
    } else {
        klog("passo 4: verify media (sha1 de 6MB)...");
        qvx_mv_result r = qvx_mv_verify_maindol(media);
        klog("passo 4 ok");
        if (r == QVX_MV_OK)
            notify("Mídia verificada: RMGE01 OK. (motor ainda não carregado)");
        else
            notify("Mídia inválida: dump errado ou corrompido.");
    }

    klog("passo 5: iniciando video-out (primeiro pixel)...");
    if (qvx_video_init() != 0) {
        klog("video: FALHA no init; caindo pro loop sem render");
        notify("Falha ao abrir video-out. Consulte o log.");
        for (;;) sceKernelUsleep(1000 * 1000);
    }
    klog("passo 5 ok: video-out ativo; present loop");

    /* Splash: textura de branding lida do DUMP DO USUÁRIO em runtime (BYO-dump).
       Nenhum asset embarcado; a arte vive só no console, vinda do dump do dono.
       Fallback para fundo cósmico próprio se o dump não estiver presente. */
    static uint32_t splash[512 * 288];
    uint32_t sw = 0, sh = 0;
    int have_splash = (qvx_art_load_tpl_from_arc(
        "/data/quovix/RMGE01/files/LayoutData/TitleLogo.arc", "mytitlelogo.tpl",
        splash, 512 * 288, &sw, &sh) == 0);
    if (have_splash) {
        klog("splash: carregado do dump; blit ativo");
    } else {
        klog("splash: nao encontrado no dump; fundo cosmico");
    }
    klog("casca pronta. motor: aguardando backend orbis.");

    /* Conteúdo estático: desenha a splash + o log UMA vez em AMBOS os
       framebuffers e depois só alterna buffers idênticos. Sem redesenho por
       frame -> sem o flicker do double-buffer. */
    for (int i = 0; i < 2; i++) {
        qvx_video_clear(0xFF0A1020);               /* azul-petróleo escuro (original) */
        if (have_splash) qvx_video_blit_centered(splash, (int)sw, (int)sh);
        qvx_text_draw(20, 16, "QUOVIX  [PORT][RMGE01][PS4<-WII]", 0xFFFFFFFF, 2);
        qvx_log_render(20, 52, 0xFF80FF80, 2);     /* log on-screen (verde) */
        qvx_video_present();
    }
    for (;;) {
        qvx_video_present();                        /* flip entre buffers idênticos */
    }
    return 0;
}
