/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Teste de host do backend DVD do Orbis contra o dump real do usuário.
   Exercita: DVDConvertPathToEntrynum -> DVDFastOpen -> DVDRead, DVDOpen,
   leitura com offset, diretórios (DVDOpenDir/ReadDir), cd + path relativo,
   e um fluxo de boot ponta a ponta (DVD -> Yaz0 -> RARC) em TitleLogo.arc.

   Uso: dvd_test <caminho/para/.../files>
        (ou defina QVX_DVD_ROOT; o FST é lido de <files>/../sys/fst.bin)

   Só reporta estrutura/tamanhos/checksum — nenhum conteúdo do jogo é gravado. */
#include "../../runtime/platform/orbis/dvd.h"
#include "../art/yaz0.h"
#include "../art/rarc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_total;
static void chk(const char *what, int ok) {
    g_total++; if (ok) g_pass++;
    printf("  [%s] %s\n", ok ? "OK" : "--", what);
}

int main(int argc, char **argv) {
    if (argc >= 2) qvx_dvd_set_root(argv[1]);
    else if (!getenv("QVX_DVD_ROOT")) {
        fprintf(stderr, "uso: %s <.../files>  (ou defina QVX_DVD_ROOT)\n", argv[0]);
        return 2;
    }

    DVDInit();
    unsigned n = qvx_dvd_entry_count();
    printf("DVDInit: %u entries (%s)\n", n,
           qvx_dvd_from_fst() ? "FST real do disco" : "fallback tree-walk");
    if (n == 0) { fprintf(stderr, "sem mídia: FST/árvore não carregou\n"); return 1; }

    const char *PATH = "/LayoutData/TitleLogo.arc";

    /* 1) convert -> fastopen -> read -------------------------------------- */
    s32 e = DVDConvertPathToEntrynum(PATH);
    printf("entrynum(%s) = %d\n", PATH, e);
    chk("DVDConvertPathToEntrynum achou o arquivo", e >= 0);

    DVDFileInfo fi;
    chk("DVDFastOpen", DVDFastOpen(e, &fi) == TRUE);
    u32 len = DVDGetLength(&fi);
    printf("DVDGetLength = %u\n", len);
    chk("tamanho == 140448 (bate com o disco)", len == 140448u);

    uint8_t head[32];
    s32 got = DVDRead(&fi, head, 32, 0);
    chk("DVDRead 32 bytes no offset 0", got == 32);
    chk("magic 'Yaz0'", memcmp(head, "Yaz0", 4) == 0);
    chk("DVDGetFileInfoStatus == END", DVDGetFileInfoStatus(&fi) == DVD_STATE_END);

    /* leitura com offset no meio do arquivo */
    uint8_t mid[16];
    s32 gm = DVDRead(&fi, mid, 16, 0x100);
    chk("DVDRead 16 bytes no offset 0x100", gm == 16);

    /* ler além do fim retorna 0 */
    uint8_t tail[64];
    s32 gt = DVDRead(&fi, tail, 64, (s32)len);
    chk("DVDRead no EOF retorna 0", gt == 0);

    /* 2) carrega o arquivo inteiro e cruza DVDOpen vs DVDFastOpen ---------- */
    uint8_t *whole = (uint8_t *)malloc(len);
    s32 all = DVDRead(&fi, whole, (s32)len, 0);
    chk("DVDRead do arquivo inteiro", all == (s32)len);
    DVDClose(&fi);

    DVDFileInfo fo;
    chk("DVDOpen pelo path", DVDOpen(PATH, &fo) == TRUE);
    chk("DVDOpen e DVDFastOpen dão o mesmo tamanho", DVDGetLength(&fo) == len);
    uint8_t probe[32];
    DVDRead(&fo, probe, 32, 0);
    chk("primeiros 32 bytes idênticos (open vs fastopen)",
        memcmp(probe, head, 32) == 0);
    DVDClose(&fo);

    /* 3) fluxo de boot ponta a ponta: DVD -> Yaz0 -> RARC ----------------- */
    uint32_t usize = qvx_yaz0_uncompressed_size(whole, len);
    chk("Yaz0: header de tamanho descomprimido", usize > 0);
    if (usize) {
        uint8_t *dec = (uint8_t *)malloc(usize);
        size_t dl = qvx_yaz0_decompress(whole, len, dec, usize);
        chk("Yaz0: descompressão", dl == usize);
        qvx_rarc r;
        int ro = qvx_rarc_open(&r, dec, dl);
        chk("RARC: abre o archive descomprimido", ro == 0);
        if (ro == 0)
            printf("    RARC: %u nós, %u entries\n", r.num_nodes, r.num_entries);
        free(dec);
    }
    free(whole);

    /* 4) diretórios: DVDOpenDir / DVDReadDir ------------------------------ */
    DVDDir dir;
    chk("DVDOpenDir(/LayoutData)", DVDOpenDir("/LayoutData", &dir) == TRUE);
    DVDDirEntry de;
    int count = 0, found = 0;
    while (DVDReadDir(&dir, &de)) {
        count++;
        if (!de.isDir && strcmp(de.name, "TitleLogo.arc") == 0) found = 1;
    }
    printf("DVDReadDir: %d entradas em /LayoutData\n", count);
    chk("diretório não-vazio", count > 0);
    chk("TitleLogo.arc listado no diretório", found);
    DVDCloseDir(&dir);

    /* 5) cd + path relativo + ".." ---------------------------------------- */
    chk("DVDChangeDir(/LayoutData)", DVDChangeDir("/LayoutData") == TRUE);
    char cwd[256];
    DVDGetCurrentDir(cwd, sizeof cwd);
    printf("cwd = %s\n", cwd);
    s32 rel = DVDConvertPathToEntrynum("TitleLogo.arc");
    chk("path relativo resolve para o mesmo entrynum", rel == e);
    s32 up = DVDConvertPathToEntrynum("../LayoutData/TitleLogo.arc");
    chk("path com '..' resolve para o mesmo entrynum", up == e);
    DVDChangeDir("/");

    /* 6) caminhos inexistentes -------------------------------------------- */
    chk("path inexistente devolve -1",
        DVDConvertPathToEntrynum("/NaoExiste/foo.arc") < 0);
    DVDFileInfo fx;
    chk("DVDOpen de inexistente devolve FALSE",
        DVDOpen("/NaoExiste/foo.arc", &fx) == FALSE);

    printf("\nresultado: %d/%d checagens OK\n", g_pass, g_total);
    return g_pass == g_total ? 0 : 1;
}
