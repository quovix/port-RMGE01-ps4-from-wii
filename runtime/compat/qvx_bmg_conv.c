/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — conversor original (quovix), Apache-2.0.
 *
 * M3.5 / camada de dados: converte um arquivo BMG do Wii (MESGbmg1, BIG-ENDIAN,
 * campos de 4 bytes) para o layout que o motor LP64 espera (u32 = 8 bytes). É o
 * 2º incremento do M3.5, irmão do qvx_rarc_conv.c — mas com UMA diferença: ao
 * contrário do RARC, o BMG NÃO fecha só com o conversor. O motor lê o BMG por um
 * misto de (a) overlay de struct inflada nos CABEÇALHOS de bloco e (b) deref cru
 * `*(u32*)` + offsets de byte fixos no CONTEÚDO. Como `u32` = 8 bytes aqui, os
 * deref crus (getBlock: numBlocks@+0xC, blockSize@+4; getMessage: offset@pInfoTool)
 * leem 8 bytes onde o dado tem 4. Esses três pontos exigem um patch mínimo no
 * motor (leitura explícita de 4 bytes LE) — ver MessageHolder.cpp no overlay local.
 * Este conversor produz o buffer que casa com esse motor patchado.
 *
 * Convenções do buffer de destino (casando com o motor):
 *   - magic do bloco: `wr64` (8 bytes, 4 ASCII + 4 zeros) -> o compare de 8 bytes
 *     de getBlock (`*(u32*)pData == 'INF1'`) passa SEM patch (igual ao RARC).
 *   - cabeçalho de bloco INFLADO (as structs do motor leem campos em offsets de
 *     8-byte-stride): MessageInfoBlock 32B (mBlockSize@8, mItemCount@16, mItemSize@18),
 *     MessageDataBlock 16B (pool em +16), MessageFlowBlock 32B (mNodeCount@16, _A@18).
 *   - blockSize escrito em +8 (campo mBlockSize da struct) -> getBlock PATCHADO lê
 *     o size em +8 (não +4, que no destino cai dentro do magic de 8 bytes).
 *   - entradas INF1 EMPACOTADAS (entrySize verbatim, SMG=12): só offset@0 (u32) e
 *     mCameraSetID@4 (u16) byteswapados BE->LE; atributos @6..0xB são bytes (verbatim).
 *     O offset é relativo ao início do pool DAT1 (mDataBlock+1) -> PRESERVADO.
 *   - pool DAT1 (texto UTF-16BE) copiado verbatim; conteúdo tratado por outro
 *     conversor de M3.5 quando o render precisar de LE.
 *   - FLW1: TalkNode inflado 8B->16B (union move de +4 p/ +8), tabela de branch
 *     u16[_A] e resto copiados com byteswap/verbatim. FLI1/MID1: header inflado +
 *     conteúdo verbatim (não acessados por offset no caminho de boot).
 *
 * Header BMG (0x20): magic 8B ("MESGbmg1"), fileSize@8 (u32), numBlocks@0xC (u32),
 * encoding@0x10 (u8). fileSize/numBlocks ficam 4-byte (lidos crus, não por struct);
 * getBlock PATCHADO lê numBlocks como 4 bytes LE em +0xC, encoding preservado.
 */

typedef unsigned char      u8_;
typedef unsigned short     u16_;
typedef unsigned int       u32_;   /* exatamente 32 bits p/ aritmética de campo */
typedef unsigned long      usize_;

extern void* qvx_pool_alloc(unsigned long bytes, unsigned long align);

/* leitura big-endian da fonte */
static u32_ be32(const u8_* p) { return ((u32_)p[0]<<24)|((u32_)p[1]<<16)|((u32_)p[2]<<8)|p[3]; }
static u16_ be16(const u8_* p) { return (u16_)(((u16_)p[0]<<8)|p[1]); }
/* escrita little-endian no destino */
static void wr64(u8_* p, u32_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;p[4]=0;p[5]=0;p[6]=0;p[7]=0; }
static void wr32(u8_* p, u32_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF; }
static void wr16(u8_* p, u16_ v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF; }
static void cpy(u8_* d, const u8_* s, usize_ n) { usize_ i; for (i=0;i<n;i++) d[i]=s[i]; }
static usize_ al32(usize_ x) { return (x + 31u) & ~(usize_)31; }

/* magic de bloco (4 ASCII) como inteiro BE p/ comparar sem depender de endianness */
static u32_ magic4(const u8_* p) { return ((u32_)p[0]<<24)|((u32_)p[1]<<16)|((u32_)p[2]<<8)|p[3]; }
#define M_INF1 0x494E4631u
#define M_DAT1 0x44415431u
#define M_FLW1 0x464C5731u
#define M_FLI1 0x464C4931u
#define M_MID1 0x4D494431u

/* Tamanho do bloco no DESTINO (inflado), dado o bloco fonte (BE). */
static usize_ dst_block_size(const u8_* b) {
    u32_ m = magic4(b);
    u32_ srcSize = be32(b + 4);
    if (m == M_INF1) {
        u32_ n = be16(b + 8);
        u32_ es = be16(b + 0xA);
        return al32(32 + (usize_)n * es);
    }
    if (m == M_DAT1) {
        return al32(16 + (usize_)(srcSize - 8));
    }
    if (m == M_FLW1) {
        u32_ nodes = be16(b + 8);
        u32_ a = be16(b + 0xA);
        usize_ after = 0x10 + (usize_)nodes * 8 + (usize_)a * 2;
        usize_ rest = (usize_)srcSize - after;
        return al32(32 + (usize_)nodes * 16 + (usize_)a * 2 + rest);
    }
    /* FLI1 / MID1 / desconhecido: header inflado (16) + conteúdo verbatim */
    return al32(16 + (usize_)(srcSize - 8));
}

/* Escreve o bloco fonte 'b' (BE) no destino 'd' (já zerado). Retorna bytes escritos
 * (== dst_block_size). O magic vai em wr64; blockSize (dst) vai em +8. */
static usize_ write_block(u8_* d, const u8_* b) {
    u32_ m = magic4(b);
    u32_ srcSize = be32(b + 4);
    usize_ dstSize = dst_block_size(b);

    wr64(d + 0, m);              /* magic (compare de 8 bytes passa sem patch) */
    wr64(d + 8, (u32_)dstSize);  /* mBlockSize (getBlock patchado lê 4 bytes LE aqui) */

    if (m == M_INF1) {
        u32_ n  = be16(b + 8);
        u32_ es = be16(b + 0xA);
        wr16(d + 16, (u16_)n);   /* mItemCount */
        wr16(d + 18, (u16_)es);  /* mItemSize (stride das entradas, preservado) */
        /* entradas empacotadas em +32, stride es */
        { u32_ i; for (i = 0; i < n; i++) {
            const u8_* se = b + 0x10 + (usize_)i * es;
            u8_* de = d + 32 + (usize_)i * es;
            cpy(de, se, es);                 /* copia a entrada inteira... */
            wr32(de + 0, be32(se + 0));      /* ...e corrige offset@0 (BE->LE 4B) */
            if (es >= 6) wr16(de + 4, be16(se + 4)); /* mCameraSetID@4 (BE->LE) */
        } }
        return dstSize;
    }
    if (m == M_DAT1) {
        usize_ pool = (usize_)srcSize - 8;
        cpy(d + 16, b + 8, pool);            /* pool verbatim (offsets preservados) */
        return dstSize;
    }
    if (m == M_FLW1) {
        u32_ nodes = be16(b + 8);
        u32_ a = be16(b + 0xA);
        wr16(d + 16, (u16_)nodes);           /* mNodeCount */
        wr16(d + 18, (u16_)a);               /* _A */
        wr64(d + 24, be32(b + 0xC));         /* _C */
        /* TalkNode inflado 8B -> 16B (union move de +4 p/ +8) */
        { u32_ i; for (i = 0; i < nodes; i++) {
            const u8_* sn = b + 0x10 + (usize_)i * 8;
            u8_* dn = d + 32 + (usize_)i * 16;
            dn[0] = sn[0];                   /* mNodeType */
            dn[1] = sn[1];                   /* mGroupID */
            wr16(dn + 2, be16(sn + 2));      /* mIndex */
            wr16(dn + 8, be16(sn + 4));      /* union: mNextIdx (ou low do u32) */
            wr16(dn + 10, be16(sn + 6));     /* union: mNextGroup */
        } }
        /* tabela de branch u16[_A], logo após o array de TalkNode inflado */
        { u8_* bt = d + 32 + (usize_)nodes * 16;
          const u8_* sb = b + 0x10 + (usize_)nodes * 8;
          u32_ i; for (i = 0; i < a; i++) wr16(bt + (usize_)i * 2, be16(sb + (usize_)i * 2));
          /* resto (_18) verbatim */
          { usize_ after = 0x10 + (usize_)nodes * 8 + (usize_)a * 2;
            usize_ rest = (usize_)srcSize - after;
            cpy(bt + (usize_)a * 2, b + after, rest); } }
        return dstSize;
    }
    /* FLI1 / MID1 / desconhecido: conteúdo verbatim após header de 16 */
    cpy(d + 16, b + 8, (usize_)srcSize - 8);
    return dstSize;
}

/* Converte um BMG Wii em 'src' -> buffer no layout do motor (pool baixo < 4GB).
 * Retorna o buffer novo ou 0 se não for BMG / pool esgotado. */
void* qvx_bmg_beconv(const void* src) {
    const u8_* s = (const u8_*)src;
    if (!s) return 0;
    /* magic "MESGbmg1" (8 bytes ASCII) */
    if (!(s[0]=='M'&&s[1]=='E'&&s[2]=='S'&&s[3]=='G'&&
          s[4]=='b'&&s[5]=='m'&&s[6]=='g'&&s[7]=='1')) return 0;

    u32_ numBlocks = be32(s + 0x0C);

    /* passada 1: total do destino = header (0x20) + soma dos blocos inflados */
    usize_ total = 0x20;
    { const u8_* b = s + 0x20; u32_ i;
      for (i = 0; i < numBlocks; i++) {
          usize_ ds = dst_block_size(b);
          total += ds;
          b += be32(b + 4);                  /* avança pelo size FONTE (BE) */
      } }

    u8_* d = (u8_*)qvx_pool_alloc((unsigned long)total, 32);
    if (!d) return 0;
    { usize_ i; for (i = 0; i < total; i++) d[i] = 0; }

    /* header destino */
    cpy(d + 0, s + 0, 8);                     /* "MESGbmg1" verbatim */
    wr32(d + 8, (u32_)total);                 /* fileSize (dst) */
    wr32(d + 0xC, numBlocks);                 /* numBlocks (getBlock patchado lê 4B LE) */
    d[0x10] = s[0x10];                        /* encoding preservado */

    /* passada 2: escreve cada bloco contíguo em +0x20 */
    { const u8_* b = s + 0x20; u8_* o = d + 0x20; u32_ i;
      for (i = 0; i < numBlocks; i++) {
          usize_ ds = write_block(o, b);
          o += ds;
          b += be32(b + 4);
      } }
    return d;
}
