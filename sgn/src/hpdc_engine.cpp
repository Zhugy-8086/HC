/**
 * @file hpdc_engine.cpp
 * @brief HPDC 引擎算法 ABI 实现 - WTA、LRU、形态学
 * @version 1.0.0
 *
 * 实现 hpdc_engine.h 中声明的所有引擎算法�? */

#include "hc/hpdc_engine.h"
#include <cstring>
#include <cstdlib>

/* ============================================================================
 * Hamming 匹配层数
 * ============================================================================ */

uint8_t match_bits_hamming(const hc8_t* a, const hc8_t* b) {
    uint8_t match = 0;
    for (int i = 0; i < 6; ++i) {
        if (a->v[i] == b->v[i]) match++;
    }
    return match;
}

/* ============================================================================
 * WTA 竞争
 * ============================================================================ */

void wta_compete(const trie_node_t* template_trie,
                     const hc8_t* template_sigs,
                     const hc8_t* query_sig,
                     uint8_t K,
                     uint16_t* winner_ids,
                     uint8_t* winner_sims) {
    uint16_t candidates[256];
    uint16_t cand_count = 0;
    trie_collect_candidates(template_trie, query_sig, 2, candidates, &cand_count);

    typedef struct { uint16_t tid; uint8_t sim; } entry_t;
    entry_t buf[256];
    uint16_t n = 0;
    for (uint16_t i = 0; i < cand_count && n < 256; ++i) {
        uint16_t tid = candidates[i];
        uint8_t sim = match_bits_hamming(query_sig, &template_sigs[tid]);
        buf[n++] = (entry_t){tid, sim};
    }

    for (uint16_t i = 0; i < K && i < n; ++i) {
        uint16_t best = i;
        for (uint16_t j = i + 1; j < n; ++j) {
            if (buf[j].sim > buf[best].sim) best = j;
        }
        entry_t tmp = buf[i];
        buf[i] = buf[best];
        buf[best] = tmp;
        winner_ids[i] = buf[i].tid;
        winner_sims[i] = buf[i].sim;
    }
}

/* ============================================================================
 * 软衰�? * ============================================================================ */

void soft_decay_all(hc8_t* hit_counters, uint16_t N, const hc8_t* Lambda) {
    for (uint16_t i = 0; i < N; ++i) {
        hit_counters[i] = hc8_soft_threshold(&hit_counters[i], Lambda);
    }
}

/* ============================================================================
 * LRU 计数与淘�? * ============================================================================ */

void lru_hit(hc8_t* counter, uint8_t delta_frac0) {
    hc8_t delta = SGN_HC8_ZERO;
    delta.v[0] = delta_frac0;
    *counter = hc8_add_sat(counter, &delta);
}

void lru_decay_all(hc8_t* counters, uint16_t N, uint8_t lambda_frac0) {
    hc8_t lambda = SGN_HC8_ZERO;
    lambda.v[0] = lambda_frac0;
    for (uint16_t i = 0; i < N; ++i) {
        counters[i] = hc8_soft_threshold(&counters[i], &lambda);
    }
}

uint16_t lru_find_evict(const hc8_t* counters, uint16_t N, const uint8_t* is_core) {
    uint16_t best = 0xFFFF;
    hc8_t min_val = SGN_HC8_MAX;
    for (uint16_t i = 0; i < N; ++i) {
        if (is_core && is_core[i]) continue;
        if (hc8_less(&counters[i], &min_val)) {
            min_val = counters[i];
            best = i;
        }
    }
    return best;
}

void lru_promote_core(const hc8_t* counters, uint8_t* is_core,
                          uint16_t N, uint8_t threshold_int) {
    for (uint16_t i = 0; i < N; ++i) {
        if (counters[i].v[0] >= threshold_int) {
            is_core[i] = 1;
        }
    }
}

/* ============================================================================
 * 模板合并
 * ============================================================================ */

void merge_or(uint8_t* mask_a, const uint8_t* mask_b, uint8_t D_bytes) {
    for (uint8_t i = 0; i < D_bytes; ++i) mask_a[i] |= mask_b[i];
}

void merge_and(uint8_t* mask_a, const uint8_t* mask_b, uint8_t D_bytes) {
    for (uint8_t i = 0; i < D_bytes; ++i) mask_a[i] &= mask_b[i];
}

/* ============================================================================
 * 形态学算子（基�?Trie 的邻域操作）
 * ============================================================================ */

static void trie_collect_with_sig(const trie_node_t* node, uint8_t* path,
                                   int depth, hc8_t* out_sig,
                                   uint16_t* out_ids, uint16_t* out_count,
                                   uint16_t max_count) {
    if (!node || *out_count >= max_count) return;
    if (depth > 6) return;
    if (SGN_TRIE_IS_LEAF(node)) {
        hc8_t sig;
        memcpy(sig.v, path, 6);
        out_sig[*out_count] = sig;
        out_ids[*out_count] = node->template_id;
        (*out_count)++;
        return;
    }
    for (uint16_t i = 0; i < node->child_count && *out_count < max_count; ++i) {
        path[depth] = node->children[i].byte;
        trie_collect_with_sig(&node->children[i], path, depth + 1,
                              out_sig, out_ids, out_count, max_count);
    }
}

void dilate(const trie_node_t* trie, const uint16_t* active_ids,
                uint16_t N, int k, uint16_t* result_ids, uint16_t* result_count) {
    *result_count = 0;
    uint8_t path[6] = {0};
    hc8_t all_sigs[512];
    uint16_t all_ids[512];
    uint16_t total = 0;
    trie_collect_with_sig(trie, path, 0, all_sigs, all_ids, &total, 512);

    uint8_t active_map[512] = {0};
    for (uint16_t i = 0; i < N && active_ids[i] < 512; ++i)
        active_map[active_ids[i]] = 1;

    uint8_t added_map[512] = {0};
    for (uint16_t i = 0; i < N; ++i) {
        uint16_t aid = active_ids[i];
        if (aid >= total) continue;
        for (uint16_t j = 0; j < total; ++j) {
            if (all_ids[j] >= 512) continue;
            if (added_map[all_ids[j]]) continue;
            if (match_bits_hamming(&all_sigs[aid], &all_sigs[j]) >= (6 - k)) {
                result_ids[(*result_count)++] = all_ids[j];
                added_map[all_ids[j]] = 1;
            }
        }
    }
}

void erode(const trie_node_t* trie, const uint8_t* S_bitmap,
               int k, uint16_t* result_ids, uint16_t* result_count) {
    *result_count = 0;
    uint8_t path[6] = {0};
    hc8_t all_sigs[512];
    uint16_t all_ids[512];
    uint16_t total = 0;
    trie_collect_with_sig(trie, path, 0, all_sigs, all_ids, &total, 512);

    for (uint16_t i = 0; i < total; ++i) {
        uint16_t tid = all_ids[i];
        if (tid >= 512 || !S_bitmap[tid]) continue;
        uint8_t dominated = 1;
        for (uint16_t j = 0; j < total; ++j) {
            if (all_ids[j] >= 512) continue;
            if (match_bits_hamming(&all_sigs[i], &all_sigs[j]) >= (6 - k)) {
                if (!S_bitmap[all_ids[j]]) { dominated = 0; break; }
            }
        }
        if (dominated) result_ids[(*result_count)++] = tid;
    }
}

void morph_open(const trie_node_t* trie, uint8_t* S_bitmap, int k,
              uint16_t* result_ids, uint16_t* result_count) {
    uint16_t eroded[512], eroded_count = 0;
    erode(trie, S_bitmap, k, eroded, &eroded_count);
    memset(S_bitmap, 0, 512);
    for (uint16_t i = 0; i < eroded_count; ++i) S_bitmap[eroded[i]] = 1;
    dilate(trie, eroded, eroded_count, k, result_ids, result_count);
}

void morph_close(const trie_node_t* trie, uint8_t* S_bitmap, int k,
               uint16_t* result_ids, uint16_t* result_count) {
    uint8_t path[6] = {0};
    hc8_t all_sigs[512];
    uint16_t all_ids[512];
    uint16_t total = 0;
    trie_collect_with_sig(trie, path, 0, all_sigs, all_ids, &total, 512);
    uint16_t active[512], active_count = 0;
    for (uint16_t i = 0; i < total; ++i) {
        if (S_bitmap[all_ids[i]]) active[active_count++] = all_ids[i];
    }
    uint16_t dilated[512], dilated_count = 0;
    dilate(trie, active, active_count, k, dilated, &dilated_count);
    memset(S_bitmap, 0, 512);
    for (uint16_t i = 0; i < dilated_count; ++i) S_bitmap[dilated[i]] = 1;
    erode(trie, S_bitmap, k, result_ids, result_count);
}

/* ============================================================================
 * 排序网络
 * ============================================================================ */

static inline void swap_hc8(hc8_t* a, hc8_t* b) {
    if (hc8_less(b, a)) {
        hc8_t tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

/* 递归 bitonic 合并 */
static void bitonic_merge(hc8_t* arr, uint16_t n, bool ascending) {
    if (n <= 1) return;
    uint16_t m = n / 2;
    for (uint16_t i = 0; i < m; ++i) {
        if (ascending) {
            if (hc8_less(&arr[i + m], &arr[i])) {
                hc8_t tmp = arr[i];
                arr[i] = arr[i + m];
                arr[i + m] = tmp;
            }
        } else {
            if (hc8_less(&arr[i], &arr[i + m])) {
                hc8_t tmp = arr[i];
                arr[i] = arr[i + m];
                arr[i + m] = tmp;
            }
        }
    }
    bitonic_merge(arr, m, ascending);
    bitonic_merge(arr + m, m, ascending);
}

static void bitonic_sort_rec(hc8_t* arr, uint16_t n, bool ascending) {
    if (n <= 1) return;
    uint16_t m = n / 2;
    bitonic_sort_rec(arr, m, true);
    bitonic_sort_rec(arr + m, m, false);
    bitonic_merge(arr, n, ascending);
}

void sortnet_bitonic_hc8(hc8_t* arr, uint16_t N) {
    /* 如果不是 2 的幂，退化为插入排序（简单起见） */
    bool is_pow2 = (N > 0) && ((N & (N - 1)) == 0);
    if (!is_pow2) {
        /* 插入排序 */
        for (uint16_t i = 1; i < N; ++i) {
            hc8_t key = arr[i];
            int16_t j = (int16_t)i - 1;
            while (j >= 0 && hc8_less(&key, &arr[j])) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }
    bitonic_sort_rec(arr, N, true);
}

/* ============================================================================
 * 逐层中位�? * ============================================================================ */

hc8_t median_layerwise(const hc8_t* values, uint16_t N) {
    if (N == 0) return SGN_HC8_ZERO;
    uint16_t* indices = (uint16_t*)malloc(N * sizeof(uint16_t));
    if (!indices) return SGN_HC8_ZERO;
    for (uint16_t i = 0; i < N; ++i) indices[i] = i;

    hc8_t median = SGN_HC8_ZERO;

    for (int layer = 0; layer < 6; ++layer) {
        uint16_t freq[256] = {0};
        for (uint16_t i = 0; i < N; ++i) {
            freq[values[indices[i]].v[layer]]++;
        }
        uint16_t half = N / 2 + 1;
        uint16_t cum = 0;
        uint8_t median_byte = 0;
        for (int b = 0; b < 256; ++b) {
            cum += freq[b];
            if (cum >= half) { median_byte = (uint8_t)b; break; }
        }
        median.v[layer] = median_byte;

        uint16_t new_count = 0;
        for (uint16_t i = 0; i < N; ++i) {
            if (values[indices[i]].v[layer] == median_byte) {
                indices[new_count++] = indices[i];
            }
        }
        N = new_count;
        if (N <= 1) break;
    }
    free(indices);
    return median;
}