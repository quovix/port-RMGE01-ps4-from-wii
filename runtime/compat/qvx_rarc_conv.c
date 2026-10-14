/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — conversor original (quovix), Apache-2.0.
 *
 * M3.5 / camada de dados: converte um archive RARC do Wii (BIG-ENDIAN, campos de
 * 4 bytes) para o layout EXATO que o motor LP64 espera (little-endian, u32 = 8
 * bytes). Resolve de uma vez largura (4->8) e endianness (BE->LE) para o CONTAINER
 * RARC, sem tocar nas structs/código do motor.
 *
 * Layouts do motor (confirmados via clang -fdump-record-layouts, u32=8):
 *   RarcHeader    64B: mMagic@0 mFileSize@8 mHeaderSize@16 mFileDataOffset@24
 *                      mTotalDataSize@32 mMRam@40 mARam@48 _1C@56
 *   RarcInfoBlock 64B: mNrDirs@0 mDirOffset@8 mNrFiles@16 mFileOffset@24
 *                      mStrTblSize@32 mStrTblOffset@40 mNextId(u16)@48
 *                      mIdIsIndex(u16)@50 _1C@56
 *   SDIDirEntry   32B: mID@0 mNameOffset@8 mHash(u16)@16 mNrFiles(u16)@18
 *                      mFirstFileIndex@24
 *   SDIFileEntry  32B: mFileID(u16)@0 mHash(u16)@2 bitfield{mFlag:8|mNameOffset:24}@4
 *                      {mDataOffset|mDirIndex}@8 mDataSize@16 mFileData(ptr)@24
 *
 * Layout fonte (Wii RARC, BE 4-byte): header 0x20, infoBlock 0x20, dir 0x10,
 * file 0x14. Offsets de dir/file/string relativos ao infoBlock; nome relativo à
 * string table; dataOffset relativo ao início do file-data.
 *
 * Offsets internos relativos (mNameOffset, mDataOffset) são PRESERVADOS; só os
 * top-level (headerSize/dir/file/string/fileData) são recomputados pros novos
 * strides. String table e file data copiados verbatim (seus formatos internos
 * são tratados por outros conversores de M3.5).
 */

typedef unsigned char      u8_;
typedef unsigned short     u16_;
typedef unsigned int       u32_;   /* exatamente 32 bits p/ aritmética de campo */
typedef unsigned long      usize_;

extern void* qvx_pool_alloc(unsigned long bytes, unsigned long align);

/* leitura big-endian da fonte */
static u32_ be32(const u8_* p) { return ((u32_)p[0]<<24)|((u32_)p[1]<<16)|((u32_)p[2]<<8)|p[3]; }
static u16_ be16(const u8_* p) { return (u16_)(((u16_)p[0]<<8)|p[1]); }
/* escrita little-endian no destino (campo de 8/4/2 bytes) */
static void wr64(u8_* p, u32_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;p[4]=0;p[5]=0;p[6]=0;p[7]=0; }
static void wr32(u8_* p, u32_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF; }
static void wr16(u8_* p, u16_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF; }
static void cpy(u8_* d, const u8_* s, usize_ n) { usize_ i; for (i=0;i<n;i++) d[i]=s[i]; }

/* Converte RARC Wii em 'src' -> buffer no layout do motor. Retorna o buffer novo
 * (do pool baixo) ou 0 se não for RARC / pool esgotado. */
void* qvx_rarc_beconv(const void* src) {
    const u8_* s = (const u8_*)src;
    if (!s) return 0;
    if (!(s[0]=='R'&&s[1]=='A'&&s[2]=='R'&&s[3]=='C')) return 0; /* só RARC */

    /* --- header fonte (BE 4-byte) --- */
    u32_ sHeaderSize = be32(s + 0x08);
    u32_ sFileDataOff = be32(s + 0x0C);
    u32_ sFileSize   = be32(s + 0x04);
    const u8_* sInfo = s + sHeaderSize;               /* infoBlock fonte */
    u32_ nrDirs      = be32(sInfo + 0x00);
    u32_ sDirOff     = be32(sInfo + 0x04);
    u32_ nrFiles     = be32(sInfo + 0x08);
    u32_ sFileOff    = be32(sInfo + 0x0C);
    u32_ sStrSize    = be32(sInfo + 0x10);
    u32_ sStrOff     = be32(sInfo + 0x14);
    u16_ sNextId     = be16(sInfo + 0x18);
    u16_ sIdIsIndex  = be16(sInfo + 0x1A);

    const u8_* sDirs = sInfo + sDirOff;
    const u8_* sFiles = sInfo + sFileOff;
    const u8_* sStr   = sInfo + sStrOff;
    const u8_* sData  = s + sHeaderSize + sFileDataOff;
    u32_ dataLen = sFileSize - (sHeaderSize + sFileDataOff);

    /* --- layout do destino (motor) --- */
    usize_ HDR = 64, INFO = 64, DIRE = 32, FILEE = 32;
    usize_ off_hdr  = 0;
    usize_ off_info = off_hdr + HDR;                           /* 64 */
    usize_ off_dirs = off_info + INFO;                         /* 128 */
    usize_ off_files = off_dirs + (usize_)nrDirs * DIRE;
    usize_ off_str  = off_files + (usize_)nrFiles * FILEE;
    usize_ off_str_a = (off_str + 31) & ~(usize_)31;           /* alinha */
    usize_ off_data = (off_str_a + sStrSize + 31) & ~(usize_)31;
    usize_ total    = off_data + dataLen;

    u8_* d = (u8_*)qvx_pool_alloc((unsigned long)total, 32);
    if (!d) return 0;
    { usize_ i; for (i=0;i<total;i++) d[i]=0; }

    /* header destino */
    wr64(d + off_hdr + 0,  0x52415243u);                       /* 'RARC' (low 4) */
    wr64(d + off_hdr + 8,  (u32_)total);                       /* mFileSize */
    wr64(d + off_hdr + 16, (u32_)HDR);                         /* mHeaderSize=64 */
    wr64(d + off_hdr + 24, (u32_)(off_data - HDR));            /* mFileDataOffset */
    /* infoBlock destino (em off_info=64) */
    wr64(d + off_info + 0,  nrDirs);
    wr64(d + off_info + 8,  (u32_)(off_dirs - off_info));      /* mDirOffset rel infoBlock */
    wr64(d + off_info + 16, nrFiles);
    wr64(d + off_info + 24, (u32_)(off_files - off_info));     /* mFileOffset */
    wr64(d + off_info + 32, sStrSize);                         /* mStringTableSize */
    wr64(d + off_info + 40, (u32_)(off_str_a - off_info));     /* mStringTableOffset */
    wr16(d + off_info + 48, sNextId);
    wr16(d + off_info + 50, sIdIsIndex);

    /* dir entries */
    { u32_ i; for (i=0;i<nrDirs;i++) {
        const u8_* se = sDirs + (usize_)i*0x10;
        u8_* de = d + off_dirs + (usize_)i*DIRE;
        wr64(de + 0,  be32(se + 0x0));     /* mID */
        wr64(de + 8,  be32(se + 0x4));     /* mNameOffset (rel string table, preservado) */
        wr16(de + 16, be16(se + 0x8));     /* mHash */
        wr16(de + 18, be16(se + 0xA));     /* mNrFiles */
        wr64(de + 24, be32(se + 0xC));     /* mFirstFileIndex */
    } }
    /* file entries */
    { u32_ i; for (i=0;i<nrFiles;i++) {
        const u8_* se = sFiles + (usize_)i*0x14;
        u8_* fe = d + off_files + (usize_)i*FILEE;
        u16_ fid = be16(se + 0x0);
        u16_ hash = be16(se + 0x2);
        u32_ flag = se[0x4];                              /* mFlag:8 */
        u32_ nameOff = ((u32_)se[0x5]<<16)|((u32_)se[0x6]<<8)|se[0x7]; /* mNameOffset:24 BE */
        u32_ dataOff = be32(se + 0x8);
        u32_ dataSize = be32(se + 0xC);
        wr16(fe + 0, fid);
        wr16(fe + 2, hash);
        wr32(fe + 4, (flag & 0xFFu) | ((nameOff & 0xFFFFFFu) << 8)); /* bitfield word LE */
        wr64(fe + 8, dataOff);            /* mDataOffset (rel file-data, preservado) */
        wr64(fe + 16, dataSize);          /* mDataSize */
        /* fe+24 mFileData = 0 (runtime) */
    } }
    /* string table + file data verbatim */
    cpy(d + off_str_a, sStr, sStrSize);
    cpy(d + off_data, sData, dataLen);
    return d;
}
