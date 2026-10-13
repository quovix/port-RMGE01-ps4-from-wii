/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Backend DVD do Orbis: implementa os NOMES da API de disco do GC/Wii
   (DVDInit/DVDOpen/DVDRead/...) com código próprio sobre file I/O do host.
   Serve BYTES CRUS dos arquivos do dump do usuário (modelo BYO-dump). A
   interpretação/endianness dos formatos (Yaz0/RARC/TPL) é dos leitores em
   product/art; o DVD em si é só I/O e NÃO faz swap.

   Entrynums: construídos a partir do FST real do disco (sys/fst.bin), de modo
   que casem exatamente com os entrynums do disco. Ver dvd.c. */
#ifndef QVX_DVD_H
#define QVX_DVD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Tipos da ABI RVL (mínimos; layout casa com revolution/dvd.h) --------- */
/* Guardas para conviver com headers do SDK no mesmo TU, caso ocorra. */
#ifndef QVX_DVD_RVL_TYPES
#define QVX_DVD_RVL_TYPES
/* ABI RVL em LP64: no engine (revolution/types.h) s32/u32 são `long`/`unsigned
   long` = 8 bytes neste alvo x86_64. Usar int32_t/uint32_t aqui (4 bytes)
   desalinhava TODAS as structs (DVDFileInfo.length, DVDDirEntry.name) e fazia
   retorno -1 virar +4294967295 no lado do engine ("não achado" = sucesso).
   Casar a largura é obrigatório. Verificado em Petari/.../revolution/types.h. */
typedef long          s32;
typedef unsigned long u32;
typedef uint8_t  u8;
typedef int      BOOL;
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif /* QVX_DVD_RVL_TYPES */

struct DVDDiskID; /* não usado pelo nosso backend; só para casar o layout */

typedef struct DVDCommandBlock DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock *block);

struct DVDCommandBlock {
    DVDCommandBlock   *next;
    DVDCommandBlock   *prev;
    u32                command;
    s32                state;
    u32                offset;
    u32                length;
    void              *addr;
    u32                currTransferSize;
    u32                transferredSize;
    struct DVDDiskID  *id;
    DVDCBCallback      callback;
    void              *userData;
};

typedef struct DVDFileInfo DVDFileInfo;
typedef void (*DVDCallback)(s32 result, DVDFileInfo *fileInfo);

struct DVDFileInfo {
    DVDCommandBlock cb;
    u32             startAddr;
    u32             length;
    DVDCallback     callback;
};

typedef struct {
    u32 entryNum;
    u32 location;
    u32 next;
} DVDDir;

typedef struct {
    u32   entryNum;
    BOOL  isDir;
    char *name;
} DVDDirEntry;

/* Estados do command block (subset de revolution/dvd.h) */
#define DVD_STATE_FATAL_ERROR (-1)
#define DVD_STATE_END         0
#define DVD_STATE_BUSY        1
#define DVD_STATE_CANCELED    10

/* --- API do Wii (nomes RVL; implementação própria) ------------------------ */
void DVDInit(void);

BOOL DVDOpen(const char *fileName, DVDFileInfo *fileInfo);
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *fileInfo);
BOOL DVDClose(DVDFileInfo *fileInfo);

s32  DVDConvertPathToEntrynum(const char *pathPtr);
BOOL DVDChangeDir(const char *dirName);
BOOL DVDGetCurrentDir(char *path, u32 maxlen);

BOOL DVDOpenDir(const char *dirName, DVDDir *dir);
BOOL DVDReadDir(DVDDir *dir, DVDDirEntry *dirent);
BOOL DVDCloseDir(DVDDir *dir);

/* Leitura. offset/length em bytes. DVDReadPrio devolve bytes lidos (<0 erro). */
s32  DVDRead(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset);
s32  DVDReadPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset, s32 prio);
BOOL DVDReadAsync(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                  DVDCallback callback);
BOOL DVDReadAsyncPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio);

s32  DVDGetFileInfoStatus(const DVDFileInfo *fileInfo);
s32  DVDGetCommandBlockStatus(const DVDCommandBlock *block);
s32  DVDGetDriveStatus(void);
u32  DVDGetLength(const DVDFileInfo *fileInfo);
s32  DVDCancel(DVDCommandBlock *block);

/* --- Configuração do backend (próprio, fora da ABI RVL) ------------------- */
/* Define a raiz dos arquivos do dump (default no PS4: /data/quovix/RMGE01/files;
   override por env QVX_DVD_ROOT). O FST é lido de <root>/../sys/fst.bin, ou do
   path em QVX_DVD_FST se definido. Chamar ANTES de DVDInit (ou re-init). */
void qvx_dvd_set_root(const char *files_root);
void qvx_dvd_set_fst_path(const char *fst_path);

/* Nº de entries carregadas (0 se o FST não carregou). Para diagnóstico/teste. */
u32  qvx_dvd_entry_count(void);

/* 1 se a tabela de entries foi construída a partir do FST real do disco;
   0 se veio do fallback caminhando a árvore files/. Para diagnóstico/teste. */
int  qvx_dvd_from_fst(void);

/* Trava a ABI contra regressão de largura: estes offsets/tamanhos têm que bater
   com revolution/dvd.h do engine em LP64 (s32/u32 = 8 bytes). */
#include <stddef.h>
_Static_assert(sizeof(DVDCommandBlock) == 96, "DVDCommandBlock ABI");
_Static_assert(sizeof(DVDFileInfo) == 120, "DVDFileInfo ABI");
_Static_assert(offsetof(DVDFileInfo, length) == 104, "DVDFileInfo.length ABI");
_Static_assert(sizeof(DVDDirEntry) == 24, "DVDDirEntry ABI");
_Static_assert(offsetof(DVDDirEntry, name) == 16, "DVDDirEntry.name ABI");

#ifdef __cplusplus
}
#endif
#endif /* QVX_DVD_H */
