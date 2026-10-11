/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
#include "rarc.h"
#include <string.h>

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

#define ENTRY_SZ 0x14

int qvx_rarc_open(qvx_rarc *r, const uint8_t *data, size_t size) {
    if (!data || size < 0x40) return -1;
    if (memcmp(data, "RARC", 4) != 0) return -2;

    uint32_t data_start = be32(data + 0x0C);         /* relativo a 0x20 */
    const uint8_t *info = data + 0x20;
    uint32_t num_nodes   = be32(info + 0x00);
    uint32_t node_off    = be32(info + 0x04);
    uint32_t num_entries = be32(info + 0x08);
    uint32_t entry_off   = be32(info + 0x0C);
    uint32_t str_len     = be32(info + 0x10);
    uint32_t str_off     = be32(info + 0x14);

    /* bounds: tudo relativo a 0x20 */
    size_t base = 0x20;
    if (base + node_off  > size) return -3;
    if (base + entry_off + (size_t)num_entries * ENTRY_SZ > size) return -4;
    if (base + str_off + str_len > size) return -5;
    if (base + data_start > size) return -6;

    r->data = data; r->size = size;
    r->nodes = data + base + node_off; r->num_nodes = num_nodes;
    r->entries = data + base + entry_off; r->num_entries = num_entries;
    r->strings = (const char *)(data + base + str_off); r->strings_len = str_len;
    r->filedata = data + base + data_start;
    return 0;
}

uint32_t qvx_rarc_entry_count(const qvx_rarc *r) { return r->num_entries; }

int qvx_rarc_entry(const qvx_rarc *r, uint32_t i,
                   const char **name, int *is_dir,
                   const uint8_t **out_data, uint32_t *out_size) {
    if (i >= r->num_entries) return -1;
    const uint8_t *e = r->entries + (size_t)i * ENTRY_SZ;
    uint16_t id       = be16(e + 0x00);
    uint8_t  flags    = e[0x04];
    uint16_t name_off = be16(e + 0x06);
    uint32_t data_off = be32(e + 0x08);
    uint32_t sz       = be32(e + 0x0C);

    int dir = (id == 0xFFFF) || (flags & 0x02);
    if (name_off >= r->strings_len) return -2;
    if (name)   *name = r->strings + name_off;
    if (is_dir) *is_dir = dir;
    if (!dir) {
        if ((size_t)(r->filedata - r->data) + data_off + sz > r->size) return -3;
        if (out_data) *out_data = r->filedata + data_off;
        if (out_size) *out_size = sz;
    } else {
        if (out_data) *out_data = 0;
        if (out_size) *out_size = 0;
    }
    return 0;
}

const uint8_t *qvx_rarc_find(const qvx_rarc *r, const char *name, uint32_t *out_size) {
    for (uint32_t i = 0; i < r->num_entries; i++) {
        const char *n; int dir; const uint8_t *d; uint32_t s;
        if (qvx_rarc_entry(r, i, &n, &dir, &d, &s) != 0) continue;
        if (!dir && strcmp(n, name) == 0) { if (out_size) *out_size = s; return d; }
    }
    return 0;
}

const uint8_t *qvx_rarc_find_ext(const qvx_rarc *r, const char *ext,
                                 const char **out_name, uint32_t *out_size) {
    size_t el = strlen(ext);
    for (uint32_t i = 0; i < r->num_entries; i++) {
        const char *n; int dir; const uint8_t *d; uint32_t s;
        if (qvx_rarc_entry(r, i, &n, &dir, &d, &s) != 0) continue;
        if (dir) continue;
        size_t nl = strlen(n);
        if (nl > el + 1 && n[nl - el - 1] == '.' && strcmp(n + nl - el, ext) == 0) {
            if (out_name) *out_name = n;
            if (out_size) *out_size = s;
            return d;
        }
    }
    return 0;
}
