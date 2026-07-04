/**
 * @file hpdc_storage.cpp
 * @brief HPDC 存储与可靠性 ABI 实现
 * @version 1.0.0
 */

#include "hc/hpdc_storage.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

/* ============================================================================
 * 文件 I/O（标准 C 实现）
 * ============================================================================ */

int file_write(const char* path, const file_header_t* hdr,
                   const file_record_t* records) {
    if (!path || !hdr || !records) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(hdr, sizeof(file_header_t), 1, f);
    if (n != 1) { fclose(f); return -1; }
    if (hdr->num_records > 0) {
        n = fwrite(records, sizeof(file_record_t), hdr->num_records, f);
        if (n != hdr->num_records) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

int file_read(const char* path, file_header_t* hdr,
                  file_record_t* records, uint32_t max_records) {
    if (!path || !hdr || !records) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(hdr, sizeof(file_header_t), 1, f);
    if (n != 1) { fclose(f); return -1; }
    uint32_t to_read = hdr->num_records;
    if (to_read > max_records) to_read = max_records;
    if (to_read > 0) {
        n = fread(records, sizeof(file_record_t), to_read, f);
        if (n != to_read) { fclose(f); return -1; }
    }
    fclose(f);
    return (int)to_read;
}

void file_grade_up(file_record_t* rec, precision_t from, precision_t to) {
    if (from >= to) return;
    /* 补零新的小数层 */
    for (int i = (int)from; i < (int)to && i < 6; ++i) {
        rec->frac[i] = 0;
    }
}

void file_grade_down(file_record_t* rec, precision_t from, precision_t to) {
    if (from <= to) return;
    /* 截断更高的小数层 */
    for (int i = (int)to; i < (int)from && i < 6; ++i) {
        rec->frac[i] = 0;
    }
}

/* RLE 编码（公共前缀压缩） */
uint32_t rle_encode(const hc8_t* templates, uint16_t N, rle_item_t* out) {
    if (!templates || !out || N == 0) return 0;
    uint32_t count = 0;
    for (uint16_t i = 0; i < N; ++i) {
        rle_item_t item;
        item.template_id = i;
        if (i == 0) {
            item.prefix_depth = 0;
            item.suffix_len = 6;
            memcpy(item.suffix, templates[i].v, 6);
        } else {
            uint8_t prefix = 0;
            for (int j = 0; j < 6; ++j) {
                if (templates[i].v[j] == templates[i-1].v[j]) prefix++;
                else break;
            }
            item.prefix_depth = prefix;
            item.suffix_len = 6 - prefix;
            memcpy(item.suffix, &templates[i].v[prefix], item.suffix_len);
        }
        out[count++] = item;
    }
    return count;
}

/* RLE 解码 */
uint32_t rle_decode(const rle_item_t* in, uint32_t n_items, hc8_t* out) {
    if (!in || !out || n_items == 0) return 0;
    for (uint32_t i = 0; i < n_items; ++i) {
        if (in[i].prefix_depth > 6 || in[i].suffix_len > 6) return 0;
        if (in[i].prefix_depth + in[i].suffix_len > 6) return 0;
        if (i == 0) {
            memset(out[i].v, 0, 6);
            memcpy(out[i].v, in[i].suffix, in[i].suffix_len);
        } else {
            memcpy(out[i].v, out[i-1].v, in[i].prefix_depth);
            memcpy(&out[i].v[in[i].prefix_depth], in[i].suffix, in[i].suffix_len);
        }
    }
    return n_items;
}

/* Merkle 树 */
void merkle_init(merkle_tree_t* tree, uint16_t n_leaves) {
    if (n_leaves == 0) { memset(tree, 0, sizeof(*tree)); return; }
    memset(tree, 0, sizeof(*tree));
    uint16_t pow2 = 1;
    while (pow2 < n_leaves) {
        if (pow2 > 32768) return;
        pow2 <<= 1;
    }
    tree->leaf_start = pow2 - 1;
}

static void merkle_hash_add(const merkle_hash_t* a, const merkle_hash_t* b,
                             merkle_hash_t* out) {
    uint32_t h1 = 0x811c9dc5, h2 = 0x174b2c15;
    for (int i = 0; i < 8; ++i) {
        h1 ^= a->bytes[i]; h1 *= 0x01000193;
        h2 ^= b->bytes[i]; h2 *= 0x01000193;
    }
    for (int i = 0; i < 4; ++i) {
        out->bytes[i]     = (uint8_t)(h1 >> (i * 8));
        out->bytes[i + 4] = (uint8_t)(h2 >> (i * 8));
    }
}

void merkle_update_leaf(merkle_tree_t* tree, uint16_t tid,
                            const merkle_hash_t* new_leaf) {
    uint16_t node_idx = tree->leaf_start + tid;
    tree->nodes[node_idx] = *new_leaf;
    while (node_idx > 0) {
        uint16_t parent_idx = (node_idx - 1) / 2;
        uint16_t left_idx = parent_idx * 2 + 1;
        uint16_t right_idx = parent_idx * 2 + 2;
        merkle_hash_add(&tree->nodes[left_idx], &tree->nodes[right_idx],
                        &tree->nodes[parent_idx]);
        node_idx = parent_idx;
    }
}

const merkle_hash_t* merkle_root(const merkle_tree_t* tree) {
    return tree ? &tree->nodes[0] : NULL;
}

/* Reed-Solomon GF(256) */
static uint8_t gf256_mul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1D;  /* 本原多项式 x^8 + x^4 + x^3 + x^2 + 1 */
        b >>= 1;
    }
    return p;
}

void rs86_encode_lfsr(const uint8_t info[6], uint8_t codeword[8]) {
    memcpy(codeword, info, 6);
    uint8_t remainder[2] = {0, 0};
    for (int i = 0; i < 6; ++i) {
        uint8_t feedback = remainder[1] ^ info[i];
        remainder[1] = remainder[0] ^ gf256_mul(feedback, 0x02);
        remainder[0] = feedback;
    }
    codeword[6] = remainder[0];
    codeword[7] = remainder[1];
}

bool rs86_decode_lfsr(uint8_t codeword[8]) {
    uint8_t s0 = 0, s1 = 0;
    uint8_t alpha_pow[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    for (int i = 0; i < 8; ++i) {
        s0 ^= gf256_mul(codeword[i], alpha_pow[i % 8]);
        s1 ^= gf256_mul(codeword[i], alpha_pow[(2*i) % 8]);
    }
    if (s0 == 0 && s1 == 0) return true;
    if (s0 == 0) return false;

    static const uint8_t inv_alpha_pow[8] = {1, 141, 203, 57, 224, 133, 195, 167};
    for (int pos = 0; pos < 8; ++pos) {
        if (gf256_mul(s0, alpha_pow[pos]) == s1) {
            uint8_t err_val = gf256_mul(s1, inv_alpha_pow[pos]);
            codeword[pos] ^= err_val;
            return true;
        }
    }
    return false;
}

/* CRC8 */
static const uint8_t CRC8_POLY = 0x31;  /* x^8 + x^5 + x^4 + 1 */

uint8_t crc8_compute(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 0x80) ? ((crc << 1) ^ CRC8_POLY) : (crc << 1);
        }
    }
    return crc;
}

/* HC8 校验和权重（供 checksum_delta 和 runtime_sniff 使用） */
static const uint8_t CS_WEIGHT[7] = {1, 2, 3, 4, 5, 6, 7};

uint8_t checksum_delta(uint8_t old_cs, uint8_t layer,
                            uint8_t old_byte, uint8_t new_byte) {
    int16_t delta = (int16_t)CS_WEIGHT[layer] * ((int16_t)new_byte - (int16_t)old_byte);
    return (uint8_t)((old_cs + delta) & 0xFF);
}

bool runtime_sniff(const hc8_t* counters, uint16_t N, uint8_t expected_global_cs) {
    uint8_t actual = 0;
    for (uint16_t i = 0; i < N; ++i) {
        actual ^= hc8_checksum(&counters[i]);
    }
    return actual == expected_global_cs;
}

/* TMR 投票 */
hc8_t tmr_vote(const hc8_t* a, const hc8_t* b, const hc8_t* c,
                        uint8_t* error_mask) {
    *error_mask = 0;
    hc8_t out;

    for (int layer = 0; layer < 6; ++layer) {
        uint8_t va = a->v[layer], vb = b->v[layer], vc = c->v[layer];
        if (va == vb) {
            out.v[layer] = va;
            if (va != vc) *error_mask |= (1 << layer);
        } else if (va == vc) {
            out.v[layer] = va;
            *error_mask |= (1 << layer);
        } else if (vb == vc) {
            out.v[layer] = vb;
            *error_mask |= (1 << layer);
        } else {
            /* 全不同，取中位数 */
            out.v[layer] = (va < vb) ? ((vb < vc) ? vb : (va < vc ? vc : va))
                                      : ((va < vc) ? va : (vb < vc ? vc : vb));
            *error_mask |= (1 << layer);
        }
    }
    return out;
}