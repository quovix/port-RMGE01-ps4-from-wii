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

/* Notificação on-screen (prova visível de boot). */
typedef struct {
    int type;
    int req_id;
    int priority;
    int msg_id;
    int target_id;
    int user_id;
    int unk1;
    int unk2;
    int app_id;
    int error_num;
    int unk3;
    char use_icon_image_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
} OrbisNotificationRequest;

static void notify(const char *msg) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof req);
    req.type = 0;
    req.unk3 = 0;
    req.use_icon_image_uri = 1;
    req.target_id = -1;
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
    notify("quovix — casca v0 iniciada (" QVX_GAME_ID ")");

    const char *media = find_user_media();
    if (!media) {
        notify("Mídia não encontrada. Coloque seu dump RMGE01 (BYO-dump).");
    } else {
        qvx_mv_result r = qvx_mv_verify_maindol(media);
        if (r == QVX_MV_OK)
            notify("Mídia verificada: RMGE01 OK. (motor ainda não carregado)");
        else
            notify("Mídia inválida: dump errado ou corrompido.");
    }

    /* Mount do motor = stub por enquanto. Mantém o processo vivo. */
    for (;;) {
        sceKernelUsleep(1000 * 1000);
    }
    return 0;
}
