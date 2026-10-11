/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Backend DVD do Orbis — implementação própria da API de disco do GC/Wii sobre
   file I/O. Serve bytes crus dos arquivos do dump do usuário (BYO-dump).

   Modelo de entrynum (decisão (b)): parseamos o FST real do disco (sys/fst.bin)
   e construímos uma tabela entrynum<->path IDÊNTICA à do disco. O boot do
   Galaxy nunca hardcoda entrynum numérico: sempre deriva de
   DVDConvertPathToEntrynum e cacheia o resultado dentro da mesma sessão (ex.
   FileHolder), comparando entrynums ambos obtidos da nossa conversão. Logo
   bastaria consistência interna; parsear o FST real é um superset seguro e
   torna os números equivalentes ao disco. Se o FST não existir, há um fallback
   que caminha a árvore files/ (tabela auto-consistente, números != disco).

   Os BYTES são servidos da árvore extraída em <root>/<path> (ex.
   /data/quovix/RMGE01/files/LayoutData/TitleLogo.arc). O FST só fornece a
   bijeção entrynum<->path (e o tamanho, usado como sanidade). Nenhum dado do
   jogo é embarcado. */
#include "dvd.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
   Configuração de paths
   --------------------------------------------------------------------------- */
#ifndef QVX_DVD_DEFAULT_ROOT
#define QVX_DVD_DEFAULT_ROOT "/data/quovix/RMGE01/files"
#endif

static char g_files_root[1024];   /* raiz dos arquivos (sem barra final) */
static char g_fst_path[1024];     /* caminho do fst.bin (opcional override) */

/* ---------------------------------------------------------------------------
   Tabela de entries
   --------------------------------------------------------------------------- */
typedef struct {
    u8          is_dir;
    const char *name;        /* basename (persistente) */
    u32         parent;      /* entrynum do diretório pai (0 = root) */
    u32         end_or_len;  /* dir: índice-fim do subtree; file: tamanho bytes */
    char       *path;        /* path absoluto a partir da raiz, ex "/A/b.arc";
                                root = "" */
} qvx_dvd_entry;

static qvx_dvd_entry *g_ents;          /* g_num_ents entries; [0] = root */
static u32            g_num_ents;
static uint8_t       *g_fst_buf;       /* buffer do fst.bin (nomes apontam aqui) */
static char          *g_name_arena;    /* nomes do fallback (tree-walk) */
static int            g_from_fst;      /* 1 = tabela veio do FST real */
static int            g_inited;
static u32            g_cwd = 0;       /* diretório corrente (entrynum) */

/* Tabela lateral de arquivos abertos: mapeia DVDFileInfo* -> entrynum + FILE*.
   Evita repurposar campos da struct que o jogo possa ler. */
#define QVX_DVD_MAX_OPEN 64
typedef struct {
    DVDFileInfo *fi;
    u32          entrynum;
    FILE        *fp;
} qvx_dvd_open;
static qvx_dvd_open g_open[QVX_DVD_MAX_OPEN];

/* ---------------------------------------------------------------------------
   Utilidades
   --------------------------------------------------------------------------- */
/* valida um nome de entry vindo da string table do FST: precisa terminar em NUL
   DENTRO da strtab (senão over-read), e não pode conter separador de path nem ser
   componente de travessia. Fecha path-traversal via DVDFastOpen(entrynum) e o
   over-read de FST corrompido/malicioso de uma vez. */
static int fst_name_ok(const char *name, size_t avail) {
    size_t i = 0;
    for (; i < avail; i++) {
        char c = name[i];
        if (c == 0) break;
        if (c == '/' || c == '\\') return 0;
    }
    if (i == avail) return 0;                 /* sem NUL dentro da string table */
    if (i == 1 && name[0] == '.') return 0;   /* "."  */
    if (i == 2 && name[0] == '.' && name[1] == '.') return 0;  /* ".." */
    return 1;
}

/* compara nome `name` (NUL-terminado) com `seg` de tamanho seglen (sem NUL) */
static int name_eq_seg(const char *name, const char *seg, size_t seglen) {
    for (size_t i = 0; i < seglen; i++) {
        unsigned char ca = (unsigned char)name[i], cb = (unsigned char)seg[i];
        if (ca == 0) return 0;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
    }
    return name[seglen] == 0;
}

static u32 dir_end(u32 dir_idx) {
    if (dir_idx == 0) return g_num_ents;          /* root */
    return g_ents[dir_idx].end_or_len;            /* dir: índice-fim */
}

/* acha filho direto de `dir_idx` com nome == seg[0..seglen). -1 se não houver. */
static s32 find_child(u32 dir_idx, const char *seg, size_t seglen) {
    u32 end = dir_end(dir_idx);
    u32 i = dir_idx + 1;
    while (i < end) {
        if (name_eq_seg(g_ents[i].name, seg, seglen)) return (s32)i;
        if (g_ents[i].is_dir) i = g_ents[i].end_or_len; /* pula subtree */
        else                  i += 1;
    }
    return -1;
}

/* próximo filho direto de `dir_idx` a partir de `loc` (inclusive). -1 no fim. */
static s32 next_child(u32 dir_idx, u32 loc) {
    u32 end = dir_end(dir_idx);
    if (loc < dir_idx + 1) loc = dir_idx + 1;
    if (loc < end) return (s32)loc;
    return -1;
}
static u32 child_advance(u32 child_idx) {
    if (g_ents[child_idx].is_dir) return g_ents[child_idx].end_or_len;
    return child_idx + 1;
}

/* ---------------------------------------------------------------------------
   DVDConvertPathToEntrynum e resolução de path
   --------------------------------------------------------------------------- */
static s32 resolve_path(const char *path) {
    if (!g_num_ents || !path) return -1;
    u32 cur = (path[0] == '/' || path[0] == '\\') ? 0 : g_cwd;
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    while (*p) {
        const char *seg = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t seglen = (size_t)(p - seg);
        while (*p == '/' || *p == '\\') p++;
        if (seglen == 0) continue;
        if (seglen == 1 && seg[0] == '.') continue;           /* "." */
        if (seglen == 2 && seg[0] == '.' && seg[1] == '.') {  /* ".." */
            cur = g_ents[cur].parent;
            continue;
        }
        if (!g_ents[cur].is_dir && cur != 0) return -1;
        s32 ch = find_child(cur, seg, seglen);
        if (ch < 0) return -1;
        cur = (u32)ch;
    }
    return (s32)cur;
}

s32 DVDConvertPathToEntrynum(const char *pathPtr) {
    return resolve_path(pathPtr);
}

/* ---------------------------------------------------------------------------
   Carga do FST real (sys/fst.bin)
   --------------------------------------------------------------------------- */
static u32 be24(const uint8_t *b) { return ((u32)b[0] << 16) | ((u32)b[1] << 8) | b[2]; }
static u32 be32(const uint8_t *b) {
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

static void free_table(void) {
    if (g_ents) {
        /* No fallback (tree-walk) os nomes são strings próprias; no FST eles
           apontam para g_fst_buf e não devem ser liberados individualmente. */
        for (u32 i = 0; i < g_num_ents; i++) {
            free(g_ents[i].path);
            if (!g_from_fst) free((void *)g_ents[i].name);
        }
        free(g_ents);
        g_ents = NULL;
    }
    free(g_fst_buf);   g_fst_buf = NULL;
    free(g_name_arena); g_name_arena = NULL;
    g_num_ents = 0;
    g_from_fst = 0;
}

/* monta g_ents[i].path concatenando parent->path + "/" + name */
static int build_paths(void) {
    g_ents[0].path = strdup("");    /* root */
    if (!g_ents[0].path) return -1;
    for (u32 i = 1; i < g_num_ents; i++) {
        const char *pp = g_ents[g_ents[i].parent].path;
        size_t ln = strlen(pp) + 1 + strlen(g_ents[i].name) + 1;
        char *s = (char *)malloc(ln);
        if (!s) return -1;
        snprintf(s, ln, "%s/%s", pp, g_ents[i].name);
        g_ents[i].path = s;
    }
    return 0;
}

static int load_fst(const char *fst_path) {
    FILE *f = fopen(fst_path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 12) { fclose(f); return -1; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return -1; }
    fclose(f);

    /* root (entry 0): type=0x01 (dir), next = nº total de entries */
    if (buf[0] != 0x01) { free(buf); return -1; }
    u32 num = be32(buf + 8);
    if (num == 0 || (uint64_t)num * 12u > (uint64_t)n) { free(buf); return -1; }
    size_t strtab = (size_t)num * 12u;
    const char *strs = (const char *)(buf + strtab);
    size_t strtab_len = (size_t)n - strtab;

    qvx_dvd_entry *ents = (qvx_dvd_entry *)calloc(num, sizeof(*ents));
    if (!ents) { free(buf); return -1; }

    /* pilha de ranges de diretório para calcular o pai de cada entry */
    /* active[]: (end_index, dir_entrynum) */
    u32 *astk_end = (u32 *)malloc(sizeof(u32) * (num + 1));
    u32 *astk_dir = (u32 *)malloc(sizeof(u32) * (num + 1));
    if (!astk_end || !astk_dir) { free(astk_end); free(astk_dir); free(ents); free(buf); return -1; }
    int top = 0;
    astk_end[0] = num; astk_dir[0] = 0;

    ents[0].is_dir = 1; ents[0].name = ""; ents[0].parent = 0; ents[0].end_or_len = num;

    for (u32 i = 1; i < num; i++) {
        const uint8_t *e = buf + (size_t)i * 12u;
        u8  typ  = e[0];
        u32 noff = be24(e + 1);
        u32 b    = be32(e + 8);   /* dir: índice-fim; file: tamanho (e+4 = offset no disco, não usado) */
        if (noff >= strtab_len) { /* nome fora da string table */
            free(astk_end); free(astk_dir); free(ents); free(buf); return -1;
        }
        if (!fst_name_ok(strs + noff, strtab_len - noff)) { /* nome inseguro/sem NUL */
            free(astk_end); free(astk_dir); free(ents); free(buf); return -1;
        }
        while (top >= 0 && i >= astk_end[top]) top--;
        if (top < 0) { free(astk_end); free(astk_dir); free(ents); free(buf); return -1; }
        ents[i].name   = strs + noff;         /* aponta no buffer do FST */
        ents[i].parent = astk_dir[top];
        if (typ == 1) {                        /* diretório: b = índice-fim */
            if (b <= i || b > num) { free(astk_end); free(astk_dir); free(ents); free(buf); return -1; }
            ents[i].is_dir = 1;
            ents[i].end_or_len = b;
            top++;
            astk_end[top] = b; astk_dir[top] = i;
        } else {                               /* arquivo: b = tamanho */
            ents[i].is_dir = 0;
            ents[i].end_or_len = b;            /* (a = offset no disco; não usado) */
        }
    }
    free(astk_end); free(astk_dir);

    free_table();
    g_ents = ents;
    g_num_ents = num;
    g_fst_buf = buf;       /* mantido vivo: nomes apontam aqui */
    g_from_fst = 1;
    if (build_paths() != 0) { free_table(); return -1; }
    return 0;
}

/* ---------------------------------------------------------------------------
   Fallback: caminha a árvore files/ via readdir (tabela auto-consistente)
   --------------------------------------------------------------------------- */
/* Vetor dinâmico simples de entries + arena de nomes. */
typedef struct { qvx_dvd_entry *v; u32 n, cap; } ent_vec;
static int ev_push(ent_vec *ev, qvx_dvd_entry e) {
    if (ev->n == ev->cap) {
        u32 nc = ev->cap ? ev->cap * 2 : 256;
        qvx_dvd_entry *nv = (qvx_dvd_entry *)realloc(ev->v, nc * sizeof(*nv));
        if (!nv) return -1;
        ev->v = nv; ev->cap = nc;
    }
    ev->v[ev->n++] = e;
    return 0;
}
static int cmp_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* recursão: adiciona filhos do diretório físico `disk_path` (entrynum `self`),
   ajustando self.end_or_len (índice-fim do subtree) ao retornar. */
static int walk_dir(ent_vec *ev, u32 self, const char *disk_path, const char *abs_path) {
    DIR *d = opendir(disk_path);
    if (!d) return -1;
    /* coleta e ordena nomes para ser determinístico */
    char **names = NULL; u32 nn = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0)))
            continue;
        if (nn == cap) { cap = cap ? cap * 2 : 32;
            char **t = (char **)realloc(names, cap * sizeof(char *));
            if (!t) { closedir(d); for (u32 k=0;k<nn;k++) free(names[k]); free(names); return -1; }
            names = t; }
        names[nn] = strdup(de->d_name);
        if (!names[nn]) { closedir(d); for (u32 k=0;k<nn;k++) free(names[k]); free(names); return -1; }
        nn++;
    }
    closedir(d);
    qsort(names, nn, sizeof(char *), cmp_names);

    for (u32 k = 0; k < nn; k++) {
        char child_disk[1024], child_abs[1024];
        snprintf(child_disk, sizeof child_disk, "%s/%s", disk_path, names[k]);
        snprintf(child_abs, sizeof child_abs, "%s/%s", abs_path, names[k]);
        DIR *sub = opendir(child_disk);
        qvx_dvd_entry e; memset(&e, 0, sizeof e);
        e.parent = self;
        e.name = strdup(names[k]);     /* liberado em free_table via path? não:
                                          nomes do fallback são donos próprios */
        e.path = strdup(child_abs);
        if (!e.name || !e.path) { if (sub) closedir(sub); for (u32 x=0;x<nn;x++) free(names[x]); free(names); return -1; }
        if (sub) {
            closedir(sub);
            e.is_dir = 1;
            u32 idx = ev->n;
            if (ev_push(ev, e) != 0) { for (u32 x=0;x<nn;x++) free(names[x]); free(names); return -1; }
            if (walk_dir(ev, idx, child_disk, child_abs) != 0) { for (u32 x=0;x<nn;x++) free(names[x]); free(names); return -1; }
            ev->v[idx].end_or_len = ev->n;   /* índice-fim do subtree */
        } else {
            FILE *fp = fopen(child_disk, "rb");
            long sz = 0;
            if (fp) { fseek(fp, 0, SEEK_END); sz = ftell(fp); fclose(fp); }
            e.is_dir = 0;
            e.end_or_len = (u32)(sz > 0 ? sz : 0);
            if (ev_push(ev, e) != 0) { for (u32 x=0;x<nn;x++) free(names[x]); free(names); return -1; }
        }
    }
    for (u32 k = 0; k < nn; k++) free(names[k]);
    free(names);
    return 0;
}

static int load_tree(const char *root) {
    ent_vec ev; memset(&ev, 0, sizeof ev);
    qvx_dvd_entry rootE; memset(&rootE, 0, sizeof rootE);
    rootE.is_dir = 1; rootE.parent = 0;
    rootE.name = strdup(""); rootE.path = strdup("");
    if (!rootE.name || !rootE.path) { free((void*)rootE.name); free(rootE.path); return -1; }
    if (ev_push(&ev, rootE) != 0) return -1;
    if (walk_dir(&ev, 0, root, "") != 0) {
        for (u32 i = 0; i < ev.n; i++) { free((void *)ev.v[i].name); free(ev.v[i].path); }
        free(ev.v);
        return -1;
    }
    ev.v[0].end_or_len = ev.n;

    free_table();
    g_ents = ev.v;
    g_num_ents = ev.n;
    g_from_fst = 0;
    /* nomes do fallback são strings próprias; libere-os também em free_table:
       marcamos isso reaproveitando g_name_arena como sentinela não é trivial,
       então liberamos name junto de path aqui no fim de vida. Para simplicidade
       e já que a tabela vive toda a sessão, aceitamos liberar name no free_table
       adaptado abaixo. */
    return 0;
}

/* ---------------------------------------------------------------------------
   Derivação do path do FST a partir da raiz de arquivos
   --------------------------------------------------------------------------- */
static void derive_fst_path(char *out, size_t cap, const char *files_root) {
    /* <files_root>/../sys/fst.bin : troca o último componente "files" por "sys" */
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", files_root);
    size_t len = strlen(tmp);
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = 0;
    char *slash = strrchr(tmp, '/');
    if (slash) { *slash = 0; snprintf(out, cap, "%s/sys/fst.bin", tmp); }
    else       { snprintf(out, cap, "sys/fst.bin"); }
}

/* ---------------------------------------------------------------------------
   Config pública
   --------------------------------------------------------------------------- */
void qvx_dvd_set_root(const char *files_root) {
    if (!files_root) return;
    snprintf(g_files_root, sizeof g_files_root, "%s", files_root);
    size_t len = strlen(g_files_root);
    while (len > 1 && (g_files_root[len - 1] == '/' || g_files_root[len - 1] == '\\'))
        g_files_root[--len] = 0;
    g_fst_path[0] = 0;   /* reseta override; será derivado em DVDInit */
    g_inited = 0;
}
void qvx_dvd_set_fst_path(const char *fst_path) {
    if (!fst_path) return;
    snprintf(g_fst_path, sizeof g_fst_path, "%s", fst_path);
    g_inited = 0;
}
u32 qvx_dvd_entry_count(void) { return g_num_ents; }
int qvx_dvd_from_fst(void)   { return g_from_fst; }

/* ---------------------------------------------------------------------------
   DVDInit
   --------------------------------------------------------------------------- */
void DVDInit(void) {
    if (g_inited) return;

    if (g_files_root[0] == 0) {
        const char *env = getenv("QVX_DVD_ROOT");
        snprintf(g_files_root, sizeof g_files_root, "%s",
                 env ? env : QVX_DVD_DEFAULT_ROOT);
        size_t len = strlen(g_files_root);
        while (len > 1 && (g_files_root[len - 1] == '/' || g_files_root[len - 1] == '\\'))
            g_files_root[--len] = 0;
    }

    char fstp[1024];
    const char *envfst = getenv("QVX_DVD_FST");
    if (g_fst_path[0]) snprintf(fstp, sizeof fstp, "%s", g_fst_path);
    else if (envfst)   snprintf(fstp, sizeof fstp, "%s", envfst);
    else               derive_fst_path(fstp, sizeof fstp, g_files_root);

    memset(g_open, 0, sizeof g_open);
    g_cwd = 0;

    if (load_fst(fstp) != 0) {
        /* fallback: caminha a árvore files/ (entrynums != disco, mas
           auto-consistentes). */
        if (load_tree(g_files_root) != 0) {
            free_table();   /* sem mídia: API responde "não existe" */
        }
    }
    g_inited = 1;
}

/* ---------------------------------------------------------------------------
   Open / Close
   --------------------------------------------------------------------------- */
static qvx_dvd_open *slot_for(DVDFileInfo *fi) {
    for (int i = 0; i < QVX_DVD_MAX_OPEN; i++)
        if (g_open[i].fi == fi) return &g_open[i];
    return NULL;
}
static qvx_dvd_open *slot_alloc(void) {
    for (int i = 0; i < QVX_DVD_MAX_OPEN; i++)
        if (g_open[i].fi == NULL) return &g_open[i];
    return NULL;
}

static void full_disk_path(char *out, size_t cap, u32 entrynum) {
    snprintf(out, cap, "%s%s", g_files_root, g_ents[entrynum].path);
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *fileInfo) {
    if (!fileInfo || entrynum < 0 || (u32)entrynum >= g_num_ents) return FALSE;
    if (g_ents[entrynum].is_dir) return FALSE;

    char disk[1024];
    full_disk_path(disk, sizeof disk, (u32)entrynum);
    FILE *fp = fopen(disk, "rb");
    if (!fp) return FALSE;

    qvx_dvd_open *s = slot_for(fileInfo);
    if (!s) s = slot_alloc();
    if (!s) { fclose(fp); return FALSE; }
    if (s->fp) fclose(s->fp);
    s->fi = fileInfo;
    s->entrynum = (u32)entrynum;
    s->fp = fp;

    memset(fileInfo, 0, sizeof *fileInfo);
    fileInfo->startAddr = 0;
    fileInfo->length    = g_ents[entrynum].end_or_len;
    fileInfo->cb.state  = DVD_STATE_END;
    return TRUE;
}

BOOL DVDOpen(const char *fileName, DVDFileInfo *fileInfo) {
    s32 e = resolve_path(fileName);
    if (e < 0) return FALSE;
    return DVDFastOpen(e, fileInfo);
}

BOOL DVDClose(DVDFileInfo *fileInfo) {
    if (!fileInfo) return FALSE;
    qvx_dvd_open *s = slot_for(fileInfo);
    if (s) {
        if (s->fp) fclose(s->fp);
        s->fp = NULL; s->fi = NULL; s->entrynum = 0;
    }
    fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
}

/* ---------------------------------------------------------------------------
   Read (síncrono; async roda síncrono por baixo e chama o callback)
   --------------------------------------------------------------------------- */
s32 DVDReadPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset, s32 prio) {
    (void)prio;
    if (!fileInfo || !addr || length < 0 || offset < 0) return -1;
    qvx_dvd_open *s = slot_for(fileInfo);
    if (!s || !s->fp) return -1;

    u32 flen = fileInfo->length;
    if ((u32)offset >= flen) {              /* nada a ler além do fim */
        fileInfo->cb.state = DVD_STATE_END;
        fileInfo->cb.transferredSize = 0;
        return 0;
    }
    u32 avail = flen - (u32)offset;
    u32 want  = (u32)length;
    if (want > avail) want = avail;

    if (fseek(s->fp, offset, SEEK_SET) != 0) return -1;
    size_t got = fread(addr, 1, want, s->fp);

    fileInfo->cb.offset          = (u32)offset;
    fileInfo->cb.length          = want;
    fileInfo->cb.transferredSize = (u32)got;
    fileInfo->cb.state           = DVD_STATE_END;
    return (s32)got;
}

s32 DVDRead(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset) {
    return DVDReadPrio(fileInfo, addr, length, offset, 2);
}

BOOL DVDReadAsyncPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio) {
    s32 r = DVDReadPrio(fileInfo, addr, length, offset, prio);
    if (r < 0) return FALSE;
    if (callback) callback(r, fileInfo);
    return TRUE;
}

BOOL DVDReadAsync(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                  DVDCallback callback) {
    return DVDReadAsyncPrio(fileInfo, addr, length, offset, callback, 2);
}

/* ---------------------------------------------------------------------------
   Status / misc
   --------------------------------------------------------------------------- */
s32 DVDGetFileInfoStatus(const DVDFileInfo *fileInfo) {
    if (!fileInfo) return DVD_STATE_FATAL_ERROR;
    return fileInfo->cb.state;
}
s32 DVDGetCommandBlockStatus(const DVDCommandBlock *block) {
    if (!block) return DVD_STATE_FATAL_ERROR;
    return block->state;
}
s32 DVDGetDriveStatus(void) { return DVD_STATE_END; }   /* drive ocioso */
u32 DVDGetLength(const DVDFileInfo *fileInfo) {
    return fileInfo ? fileInfo->length : 0;
}
s32 DVDCancel(DVDCommandBlock *block) {   /* nada a cancelar (I/O síncrono) */
    if (block) block->state = DVD_STATE_END;
    return 0;
}

/* ---------------------------------------------------------------------------
   Diretórios
   --------------------------------------------------------------------------- */
BOOL DVDChangeDir(const char *dirName) {
    s32 e = resolve_path(dirName);
    if (e < 0 || !g_ents[e].is_dir) return FALSE;
    g_cwd = (u32)e;
    return TRUE;
}

BOOL DVDGetCurrentDir(char *path, u32 maxlen) {
    if (!path || maxlen == 0) return FALSE;
    if (!g_num_ents || g_cwd >= g_num_ents) return FALSE;  /* sem mídia montada */
    const char *p = g_ents[g_cwd].path;      /* "" para root */
    /* SDK escreve com barra inicial e final: "/", "/AudioRes/" */
    int need = snprintf(path, maxlen, "%s/", p[0] ? p : "");
    if (need < 0 || (u32)need >= maxlen) return FALSE;
    return TRUE;
}

BOOL DVDOpenDir(const char *dirName, DVDDir *dir) {
    if (!dir) return FALSE;
    s32 e = resolve_path(dirName);
    if (e < 0 || !g_ents[e].is_dir) return FALSE;
    dir->entryNum = (u32)e;
    dir->location = (u32)e + 1;
    dir->next     = dir_end((u32)e);
    return TRUE;
}

BOOL DVDReadDir(DVDDir *dir, DVDDirEntry *dirent) {
    if (!dir || !dirent) return FALSE;
    s32 c = next_child(dir->entryNum, dir->location);
    if (c < 0) return FALSE;
    dirent->entryNum = (u32)c;
    dirent->isDir    = g_ents[c].is_dir ? TRUE : FALSE;
    dirent->name     = (char *)g_ents[c].name;
    dir->location    = child_advance((u32)c);
    return TRUE;
}

BOOL DVDCloseDir(DVDDir *dir) {
    if (!dir) return FALSE;
    dir->entryNum = 0; dir->location = 0; dir->next = 0;
    return TRUE;
}
