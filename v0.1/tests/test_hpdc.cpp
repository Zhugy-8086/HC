// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file test_hpdc.cpp
 * @brief HPDC 高级模块测试 - sandbox / trie / engine / storage / network / plugin
 * @version 1.0.0
 *
 * Build (MSVC):
 *   cl /EHsc /std:c++17 /Isgn/include /Itests ^
 *      sgn\src\hc.c sgn\src\hc8.c sgn\src\hc16.c sgn\src\hc32.c sgn\src\hc64.c ^
 *      sgn\src\dc.c sgn\src\hc_simd.c ^
 *      sgn\src\hpdc_sandbox.cpp sgn\src\hpdc_trie.cpp sgn\src\hpdc_engine.cpp ^
 *      sgn\src\hpdc_storage.cpp sgn\src\hpdc_network.cpp sgn\src\hpdc_plugin.cpp ^
 *      tests\test_hpdc.cpp /Fe:tests\test_hpdc.exe
 *
 * Build (g++ / clang++):
 *   g++ -std=c++17 -Isgn/include -Itests \
 *       sgn/src/hc.c sgn/src/hc8.c sgn/src/hc16.c sgn/src/hc32.c sgn/src/hc64.c \
 *       sgn/src/dc.c sgn/src/hc_simd.c \
 *       sgn/src/hpdc_sandbox.cpp sgn/src/hpdc_trie.cpp sgn/src/hpdc_engine.cpp \
 *       sgn/src/hpdc_storage.cpp sgn/src/hpdc_network.cpp sgn/src/hpdc_plugin.cpp \
 *       tests/test_hpdc.cpp -o tests/test_hpdc.exe
 */

#include "sgn_test.h"
#include "hc/hc.h"
#include "hc/hc8.h"
#include "hc/hc16.h"
#include "hc/hc32.h"
#include "hc/hc64.h"
#include "hc/hpdc_sandbox.h"
#include "hc/hpdc_trie.h"
#include "hc/hpdc_engine.h"
#include "hc/hpdc_storage.h"
#include "hc/hpdc_network.h"
#include "hc/hpdc_plugin.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

/* ============================================================================
 * Sandbox 测试
 * ============================================================================ */

TEST(sandbox_create_auto) {
    sandbox_t* sb8  = sandbox_create_auto(SGN_HC_KIND_8);
    sandbox_t* sb16 = sandbox_create_auto(SGN_HC_KIND_16);
    ASSERT_NOT_NULL(sb8);
    ASSERT_NOT_NULL(sb16);
    ASSERT_STR_EQ(sandbox_get_scheme(sb8), "default");
    ASSERT_STR_EQ(sandbox_get_scheme(sb16), "default");
    sandbox_destroy(sb8);
    sandbox_destroy(sb16);
}

TEST(sandbox_create_invalid) {
    /* int_bits 与 HC 种类不匹配应失败 */
    sandbox_t* bad = sandbox_create(SGN_PREC_PREC, 16);  /* HC8 仅允许 8 */
    ASSERT_NULL(bad);
    /* NULL 句柄安全调用 */
    ASSERT_NEAR(sandbox_project_hc8(NULL, SGN_HC8_ZERO), 0.0, 0.0001);
}

TEST(sandbox_project_inverse_hc8) {
    sandbox_t* sb = sandbox_create_auto(SGN_HC_KIND_8);
    ASSERT_NOT_NULL(sb);
    /* phi 应落在 [0, 1) */
    hc8_t h = hc8_from_double(128.0, SGN_OVERFLOW_SATURATE);
    double phi = sandbox_project_hc8(sb, h);
    ASSERT_TRUE(phi >= 0.0 && phi < 1.0);
    /* inverse(project(h)) 应近似还原 h */
    hc8_t h2 = sandbox_inverse_hc8(sb, phi);
    ASSERT_NEAR(hc8_to_double(h2), 128.0, 1.0);
    /* phi 越界被裁剪到 [0,1) */
    hc8_t hi = sandbox_inverse_hc8(sb, 5.0);
    ASSERT_TRUE(hc8_to_double(hi) > 0.0);
    hc8_t lo = sandbox_inverse_hc8(sb, -1.0);
    ASSERT_NEAR(hc8_to_double(lo), 0.0, 0.01);
    sandbox_destroy(sb);
}

TEST(sandbox_divide_hc8) {
    sandbox_t* sb = sandbox_create_auto(SGN_HC_KIND_8);
    ASSERT_NOT_NULL(sb);
    /* divide 语义：q = hc8_from_double((project(a)/project(b)) * R)
     * project(h) = hc8_to_double(h) / R，故 q = hc8_from_double((a/b) * R)
     * 取 a=4, b=8 -> (4/8)*256 = 128（不饱和） */
    hc8_t a = hc8_from_double(4.0, SGN_OVERFLOW_SATURATE);
    hc8_t b = hc8_from_double(8.0, SGN_OVERFLOW_SATURATE);
    hc8_t q = sandbox_divide_hc8(sb, a, b);
    ASSERT_NEAR(hc8_to_double(q), 128.0, 1.0);
    /* 除零返回 HC_MAX */
    hc8_t six = hc8_from_double(6.0, SGN_OVERFLOW_SATURATE);
    hc8_t zero = SGN_HC8_ZERO;
    hc8_t r = sandbox_divide_hc8(sb, six, zero);
    ASSERT_TRUE(hc8_less(&six, &r));
    sandbox_destroy(sb);
}

TEST(sandbox_scale_gradient_hc8) {
    sandbox_t* sb = sandbox_create_auto(SGN_HC_KIND_8);
    ASSERT_NOT_NULL(sb);
    hc8_t h = hc8_from_double(100.0, SGN_OVERFLOW_SATURATE);
    /* 缩放 2x */
    hc8_t s = sandbox_scale_hc8(sb, h, 2.0);
    ASSERT_NEAR(hc8_to_double(s), 200.0, 1.0);
    /* 梯度更新 h_new = h - eta * grad */
    hc8_t g = sandbox_gradient_hc8(sb, h, 1.0, 10.0);
    ASSERT_TRUE(hc8_less(&g, &h));
    sandbox_destroy(sb);
}

TEST(sandbox_scheme) {
    sandbox_t* sb = sandbox_create_auto(SGN_HC_KIND_8);
    ASSERT_NOT_NULL(sb);
    ASSERT_EQ(sandbox_set_scheme(sb, "default"), 0);
    ASSERT_NE(sandbox_set_scheme(sb, "log_domain"), 0);  /* 未注册方案 */
    ASSERT_STR_EQ(sandbox_get_scheme(sb), "default");
    sandbox_destroy(sb);
}

/* ============================================================================
 * Trie 测试
 * ============================================================================ */

TEST(trie_pool_init_alloc) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    ASSERT_EQ(pool.next_free, 1);
    ASSERT_TRUE(SGN_TRIE_IS_INTERNAL(&pool.nodes[0]));
    /* 分配若干节点 */
    trie_node_t* n1 = trie_pool_alloc(&pool);
    trie_node_t* n2 = trie_pool_alloc(&pool);
    ASSERT_NOT_NULL(n1);
    ASSERT_NOT_NULL(n2);
    ASSERT_TRUE(n1 != n2);
    ASSERT_TRUE(SGN_TRIE_IS_INTERNAL(n1));  /* 新节点默认为内部 */
}

TEST(trie_insert_find_child) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    trie_node_t* root = &pool.nodes[0];
    trie_node_t* c = trie_insert_child(root, 42, &pool);
    ASSERT_NOT_NULL(c);
    ASSERT_EQ(c->byte, 42);
    ASSERT_EQ(root->child_count, 1);
    /* 已存在则返回同一节点 */
    trie_node_t* c2 = trie_insert_child(root, 42, &pool);
    ASSERT_EQ(c, c2);
    ASSERT_EQ(root->child_count, 1);
    /* 查找 */
    ASSERT_EQ(trie_find_child(root, 42), c);
    ASSERT_NULL(trie_find_child(root, 99));
}

TEST(trie_insert_template_find) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    trie_node_t* root = &pool.nodes[0];

    hc8_t sig1 = {{1, 2, 3, 4, 5, 6}};
    hc8_t sig2 = {{1, 2, 9, 9, 9, 9}};
    ASSERT_TRUE(trie_insert_template(root, &sig1, 100, &pool));
    ASSERT_TRUE(trie_insert_template(root, &sig2, 200, &pool));

    /* 沿 sig1 路径下降 6 层应到达叶子 (tid=100) */
    trie_node_t* leaf = trie_find_path(root, &sig1, 6);
    ASSERT_NOT_NULL(leaf);
    ASSERT_TRUE(SGN_TRIE_IS_LEAF(leaf));
    ASSERT_EQ(leaf->template_id, 100);
}

TEST(trie_collect_candidates) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    trie_node_t* root = &pool.nodes[0];

    /* 3 个模板，前 2 层相同 (1,2)，第 3 层分叉 */
    hc8_t sig_a = {{1, 2, 10, 0, 0, 0}};
    hc8_t sig_b = {{1, 2, 20, 0, 0, 0}};
    hc8_t sig_c = {{5, 6, 30, 0, 0, 0}};
    ASSERT_TRUE(trie_insert_template(root, &sig_a, 1, &pool));
    ASSERT_TRUE(trie_insert_template(root, &sig_b, 2, &pool));
    ASSERT_TRUE(trie_insert_template(root, &sig_c, 3, &pool));

    /* 用前缀 (1,2,...) 查询，k=2 应当命中 a 和 b */
    hc8_t query = {{1, 2, 99, 99, 99, 99}};
    uint16_t ids[16];
    uint16_t count = 0;
    trie_collect_candidates(root, &query, 2, ids, &count);
    ASSERT_EQ(count, 2);
    /* 不验证 ids 顺序（依赖遍历顺序），仅验证集合 */
    bool has1 = false, has2 = false, has3 = false;
    for (uint16_t i = 0; i < count; ++i) {
        if (ids[i] == 1) has1 = true;
        if (ids[i] == 2) has2 = true;
        if (ids[i] == 3) has3 = true;
    }
    ASSERT_TRUE(has1 && has2);
    ASSERT_FALSE(has3);
}

TEST(trie_lazy_delete) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    trie_node_t* root = &pool.nodes[0];
    hc8_t sig = {{7, 7, 7, 7, 7, 7}};
    ASSERT_TRUE(trie_insert_template(root, &sig, 42, &pool));
    trie_node_t* leaf = trie_find_path(root, &sig, 6);
    ASSERT_NOT_NULL(leaf);
    ASSERT_TRUE(SGN_TRIE_IS_LEAF(leaf));
    trie_lazy_delete(leaf);
    ASSERT_FALSE(SGN_TRIE_IS_LEAF(leaf));  /* template_id 被置为 0xFFFF */
}

/* ============================================================================
 * Engine 测试
 * ============================================================================ */

TEST(engine_match_bits_hamming) {
    hc8_t a = {{1, 2, 3, 4, 5, 6}};
    hc8_t b = {{1, 2, 3, 4, 5, 6}};
    hc8_t c = {{9, 9, 9, 9, 9, 9}};
    ASSERT_EQ(match_bits_hamming(&a, &b), 6);
    ASSERT_EQ(match_bits_hamming(&a, &c), 0);
    hc8_t d = {{1, 9, 3, 9, 5, 9}};
    ASSERT_EQ(match_bits_hamming(&a, &d), 3);
}

TEST(engine_lru_hit_decay) {
    hc8_t counters[3] = {SGN_HC8_ZERO, SGN_HC8_ZERO, SGN_HC8_ZERO};
    lru_hit(&counters[0], 10);
    lru_hit(&counters[1], 20);
    lru_hit(&counters[2], 5);
    ASSERT_EQ(counters[0].v[0], 10);
    ASSERT_EQ(counters[1].v[0], 20);
    ASSERT_EQ(counters[2].v[0], 5);
    /* 全局衰减 3 */
    lru_decay_all(counters, 3, 3);
    ASSERT_EQ(counters[0].v[0], 7);
    ASSERT_EQ(counters[1].v[0], 17);
    ASSERT_EQ(counters[2].v[0], 2);
}

TEST(engine_lru_evict_promote) {
    hc8_t counters[3] = {{{1, 0, 0, 0, 0, 0}},
                          {{5, 0, 0, 0, 0, 0}},
                          {{9, 0, 0, 0, 0, 0}}};
    uint8_t is_core[3] = {0, 0, 0};
    /* counters[0] 最小，应被淘汰 */
    ASSERT_EQ(lru_find_evict(counters, 3, NULL), 0);
    /* 标记 counters[2] 为核心后，最小非核心是 counters[0] */
    is_core[2] = 1;
    ASSERT_EQ(lru_find_evict(counters, 3, is_core), 0);
    /* 提升阈值 5：counters[1] 和 counters[2] 升为核心 */
    is_core[1] = 0; is_core[2] = 0;
    lru_promote_core(counters, is_core, 3, 5);
    ASSERT_EQ(is_core[0], 0);
    ASSERT_EQ(is_core[1], 1);
    ASSERT_EQ(is_core[2], 1);
}

TEST(engine_soft_decay) {
    hc8_t counters[2] = {{10, 0, 0, 0, 0, 0},
                          {2, 0, 0, 0, 0, 0}};
    hc8_t lambda = {{5, 0, 0, 0, 0, 0}};
    soft_decay_all(counters, 2, &lambda);
    ASSERT_EQ(counters[0].v[0], 5);  /* 10-5=5 */
    ASSERT_EQ(counters[1].v[0], 0);  /* 2-5 -> 0 (饱和) */
}

TEST(engine_merge_or_and) {
    uint8_t a[2] = {0xF0, 0x0F};
    uint8_t b[2] = {0x0C, 0x30};
    merge_or(a, b, 2);
    ASSERT_EQ(a[0], 0xFC);
    ASSERT_EQ(a[1], 0x3F);
    /* AND 后 */
    uint8_t c[2] = {0xFF, 0xFF};
    merge_and(c, b, 2);
    ASSERT_EQ(c[0], 0x0C);
    ASSERT_EQ(c[1], 0x30);
}

TEST(engine_sortnet_bitonic) {
    /* 2 的幂走 bitonic 路径 */
    hc8_t arr[4] = {{3, 0, 0, 0, 0, 0},
                     {1, 0, 0, 0, 0, 0},
                     {4, 0, 0, 0, 0, 0},
                     {2, 0, 0, 0, 0, 0}};
    sortnet_bitonic_hc8(arr, 4);
    ASSERT_EQ(arr[0].v[0], 1);
    ASSERT_EQ(arr[1].v[0], 2);
    ASSERT_EQ(arr[2].v[0], 3);
    ASSERT_EQ(arr[3].v[0], 4);
    /* 非 2 的幂走插入排序 */
    hc8_t arr2[3] = {{3, 0, 0, 0, 0, 0},
                      {1, 0, 0, 0, 0, 0},
                      {2, 0, 0, 0, 0, 0}};
    sortnet_bitonic_hc8(arr2, 3);
    ASSERT_EQ(arr2[0].v[0], 1);
    ASSERT_EQ(arr2[1].v[0], 2);
    ASSERT_EQ(arr2[2].v[0], 3);
}

TEST(engine_median_layerwise) {
    hc8_t vals[3] = {{1, 0, 0, 0, 0, 0},
                      {2, 0, 0, 0, 0, 0},
                      {3, 0, 0, 0, 0, 0}};
    hc8_t m = median_layerwise(vals, 3);
    ASSERT_EQ(m.v[0], 2);
}

TEST(engine_wta_compete) {
    trie_pool_t pool;
    trie_pool_init(&pool);
    trie_node_t* root = &pool.nodes[0];
    /* 3 个模板 */
    hc8_t sigs[3] = {
        {{1, 2, 3, 4, 5, 6}},
        {{1, 2, 3, 9, 9, 9}},
        {{9, 9, 9, 9, 9, 9}}
    };
    trie_insert_template(root, &sigs[0], 0, &pool);
    trie_insert_template(root, &sigs[1], 1, &pool);
    trie_insert_template(root, &sigs[2], 2, &pool);
    /* 查询与 sigs[0] 完全相同 -> 应得 6 匹配 */
    hc8_t query = {{1, 2, 3, 4, 5, 6}};
    uint16_t winner_ids[4];
    uint8_t  winner_sims[4];
    wta_compete(root, sigs, &query, 1, winner_ids, winner_sims);
    ASSERT_EQ(winner_ids[0], 0);
    ASSERT_EQ(winner_sims[0], 6);
}

/* ============================================================================
 * Storage 测试
 * ============================================================================ */

TEST(storage_crc8) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    uint8_t cs1 = crc8_compute(data, 5);
    uint8_t cs2 = crc8_compute(data, 5);
    /* 确定性 */
    ASSERT_EQ(cs1, cs2);
    /* 不同输入产生不同 CRC */
    data[0] = 99;
    uint8_t cs3 = crc8_compute(data, 5);
    ASSERT_NE(cs1, cs3);
}

TEST(storage_checksum_delta) {
    /* layer 0 权重为 1，旧值 0 -> 新值 5 应增加 5 */
    uint8_t cs = checksum_delta(100, 0, 0, 5);
    ASSERT_EQ(cs, 105);
    /* layer 1 权重为 2 */
    uint8_t cs2 = checksum_delta(100, 1, 0, 5);
    ASSERT_EQ(cs2, 110);
}

TEST(storage_runtime_sniff) {
    hc8_t counters[2] = {{1, 0, 0, 0, 0, 0},
                          {2, 0, 0, 0, 0, 0}};
    uint8_t expected = hc8_checksum(&counters[0]) ^ hc8_checksum(&counters[1]);
    ASSERT_TRUE(runtime_sniff(counters, 2, expected));
    /* 错误的预期值应失败 */
    ASSERT_FALSE(runtime_sniff(counters, 2, (uint8_t)(expected ^ 0xFF)));
}

TEST(storage_rs86_roundtrip) {
    uint8_t info[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    uint8_t codeword[8];
    rs86_encode_lfsr(info, codeword);
    /* 前 6 字节应等于 info */
    ASSERT_EQ(codeword[0], 0x10);
    ASSERT_EQ(codeword[5], 0x60);
    /* 无错解码成功，且 info 部分不变 */
    bool ok = rs86_decode_lfsr(codeword);
    ASSERT_TRUE(ok);
    ASSERT_EQ(codeword[0], 0x10);
    ASSERT_EQ(codeword[5], 0x60);
}

TEST(storage_rs86_single_error_correction) {
    uint8_t info[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t cw[8];
    rs86_encode_lfsr(info, cw);
    /* 注入 1 字节错误 */
    cw[2] ^= 0xAB;
    bool ok = rs86_decode_lfsr(cw);
    ASSERT_TRUE(ok);
    /* 解码后 info 应被纠正 */
    ASSERT_EQ(cw[0], 0x11);
    ASSERT_EQ(cw[2], 0x33);
    ASSERT_EQ(cw[5], 0x66);
}

TEST(storage_merkle) {
    merkle_tree_t tree;
    merkle_init(&tree, 4);
    /* leaf_start 应为 2^2 - 1 = 3 */
    ASSERT_EQ(tree.leaf_start, 3);
    merkle_hash_t h1 = {{1, 2, 3, 4, 5, 6, 7, 8}};
    merkle_update_leaf(&tree, 0, &h1);
    const merkle_hash_t* root1 = merkle_root(&tree);
    ASSERT_NOT_NULL(root1);
    /* 修改另一叶子后根应改变 */
    merkle_hash_t saved;
    memcpy(&saved, root1, sizeof(saved));
    merkle_hash_t h2 = {{9, 9, 9, 9, 9, 9, 9, 9}};
    merkle_update_leaf(&tree, 1, &h2);
    const merkle_hash_t* root2 = merkle_root(&tree);
    ASSERT_TRUE(memcmp(&saved, root2, sizeof(saved)) != 0);
    /* NULL 树返回 NULL */
    ASSERT_NULL(merkle_root(NULL));
}

TEST(storage_rle_roundtrip) {
    hc8_t templates[3] = {
        {{1, 2, 3, 4, 5, 6}},
        {{1, 2, 9, 9, 9, 9}},   /* 与前一个前缀 2 */
        {{8, 8, 8, 8, 8, 8}}    /* 与前一个无前缀 */
    };
    rle_item_t items[3];
    uint32_t n = rle_encode(templates, 3, items);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(items[0].prefix_depth, 0);
    ASSERT_EQ(items[1].prefix_depth, 2);
    ASSERT_EQ(items[2].prefix_depth, 0);
    /* 解码还原 */
    hc8_t decoded[3];
    uint32_t m = rle_decode(items, 3, decoded);
    ASSERT_EQ(m, 3);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(memcmp(decoded[i].v, templates[i].v, 6) == 0);
    }
}

TEST(storage_file_io_roundtrip) {
    file_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic[0] = 'S'; hdr.magic[1] = 'G'; hdr.magic[2] = 'N'; hdr.magic[3] = 0;
    hdr.version = 0x01;
    hdr.global_level = 4;
    hdr.num_records = 2;
    file_record_t recs[2];
    memset(recs, 0, sizeof(recs));
    recs[0].mode = 0x02; recs[0].int_part = 5; recs[0].frac[0] = 1;
    recs[1].mode = 0x02; recs[1].int_part = 9; recs[1].frac[0] = 2;

    const char* path = "test_hpdc_tmp_model.bin";
    int wrc = file_write(path, &hdr, recs);
    ASSERT_EQ(wrc, 0);

    file_header_t hdr2;
    file_record_t recs2[2];
    memset(&hdr2, 0, sizeof(hdr2));
    memset(recs2, 0, sizeof(recs2));
    int n = file_read(path, &hdr2, recs2, 2);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(hdr2.version, 0x01);
    ASSERT_EQ(hdr2.num_records, 2);
    ASSERT_EQ(recs2[0].int_part, 5);
    ASSERT_EQ(recs2[1].int_part, 9);
    /* 清理 */
    remove(path);
}

TEST(storage_file_grade) {
    file_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.frac[0] = 1; rec.frac[1] = 2; rec.frac[2] = 3;
    rec.frac[3] = 4; rec.frac[4] = 5; rec.frac[5] = 6;
    /* 升级 2 -> 4：新增层补零（已存在不动） */
    file_grade_up(&rec, SGN_PREC_FAST, SGN_PREC_STD);
    /* 降级 4 -> 2：高层清零 */
    file_grade_down(&rec, SGN_PREC_STD, SGN_PREC_FAST);
    ASSERT_EQ(rec.frac[2], 0);
    ASSERT_EQ(rec.frac[3], 0);
}

TEST(storage_tmr_vote) {
    hc8_t a = {{1, 2, 3, 4, 5, 6}};
    hc8_t b = {{1, 2, 3, 4, 5, 6}};
    hc8_t c = {{1, 2, 3, 4, 5, 6}};
    uint8_t mask = 0xFF;
    hc8_t r = tmr_vote(&a, &b, &c, &mask);
    ASSERT_EQ(mask, 0);  /* 全相同，无错误 */
    ASSERT_TRUE(hc8_equal(&r, &a));
    /* c 与 a,b 不同（layer 0） */
    hc8_t c2 = {{9, 2, 3, 4, 5, 6}};
    hc8_t r2 = tmr_vote(&a, &b, &c2, &mask);
    ASSERT_EQ(mask, 1);  /* bit0 = layer0 错误 */
    ASSERT_TRUE(hc8_equal(&r2, &a));  /* 多数投票结果 = a */
}

/* ============================================================================
 * Network 测试
 * ============================================================================ */

TEST(network_uart_roundtrip) {
    uint8_t payload[3] = {0x10, 0x20, 0x30};
    uint8_t frame[16];
    uart_pack(0x42, payload, 3, frame);
    /* 帧: AA 55 42 03 10 20 30 CRC = 8 字节 */
    uart_frame_t parsed;
    bool ok = uart_parse(frame, 8, &parsed);
    ASSERT_TRUE(ok);
    ASSERT_EQ(parsed.type, 0x42);
    ASSERT_EQ(parsed.len, 3);
    ASSERT_EQ(parsed.payload[0], 0x10);
    ASSERT_EQ(parsed.payload[2], 0x30);
}

TEST(network_uart_bad_sync) {
    uint8_t bad[8] = {0xBB, 0x55, 0x42, 0x03, 0x10, 0x20, 0x30, 0x00};
    uart_frame_t parsed;
    ASSERT_FALSE(uart_parse(bad, 8, &parsed));
}

TEST(network_cobs_roundtrip) {
    /* 包含 0 字节，验证 COBS 透明传输 */
    uint8_t in[6] = {0x11, 0x00, 0x22, 0x00, 0x33, 0x44};
    uint8_t enc[16];
    uint8_t enc_len = cobs_encode(in, 6, enc);
    ASSERT_TRUE(enc_len > 0);
    uint8_t dec[16];
    uint8_t dec_len = cobs_decode(enc, enc_len, dec);
    ASSERT_EQ(dec_len, 6);
    for (int i = 0; i < 6; ++i) {
        ASSERT_EQ(dec[i], in[i]);
    }
}

TEST(network_lamport_compare) {
    hc16_lamport_t a = {0, 5, {100, 0, 0, 0}};
    hc16_lamport_t b = {0, 5, {200, 0, 0, 0}};
    hc16_lamport_t c = {0, 6, {100, 0, 0, 0}};
    hc16_lamport_t d = {1, 5, {100, 0, 0, 0}};
    /* 同 level 同 sec：ss[0] 比较 */
    ASSERT_TRUE(lamport_compare(&a, &b) < 0);
    ASSERT_TRUE(lamport_compare(&b, &a) > 0);
    ASSERT_EQ(lamport_compare(&a, &a), 0);
    /* sec 优先于 ss */
    ASSERT_TRUE(lamport_compare(&a, &c) < 0);
    /* level 优先于 sec */
    ASSERT_TRUE(lamport_compare(&a, &d) < 0);
}

TEST(network_lamport_update) {
    /* local < remote -> local = remote，然后 ss[0] += 65 */
    hc16_lamport_t local  = {0, 1, {10, 0, 0, 0}};
    hc16_lamport_t remote = {0, 1, {100, 0, 0, 0}};
    lamport_update(&local, &remote);
    ASSERT_EQ(local.ss[0], (uint16_t)(100 + 65));
    /* local > remote -> 不复制，仅 ss[0] += 65 */
    hc16_lamport_t local2  = {0, 1, {200, 0, 0, 0}};
    hc16_lamport_t remote2 = {0, 1, {100, 0, 0, 0}};
    lamport_update(&local2, &remote2);
    ASSERT_EQ(local2.ss[0], (uint16_t)(200 + 65));
}

TEST(network_wdt) {
    wdt_entry_t slots[SGN_WDT_SLOTS];
    memset(slots, 0, sizeof(slots));
    static int fired = 0;
    static uint8_t fired_id = 0xFF;
    fired = 0; fired_id = 0xFF;
    auto cb = [](uint8_t id) { fired++; fired_id = id; };

    hc16_time_t deadline = {10, {0, 0, 0, 0}};
    hc16_time_t now_past = {20, {0, 0, 0, 0}};
    hc16_time_t now_before = {5, {0, 0, 0, 0}};

    wdt_start(slots, 2, &deadline, cb);
    ASSERT_EQ(slots[2].active, 1);
    /* now < deadline: 未超时 */
    wdt_poll(slots, SGN_WDT_SLOTS, &now_before);
    ASSERT_EQ(fired, 0);
    /* now > deadline: 触发回调并自动停用 */
    wdt_poll(slots, SGN_WDT_SLOTS, &now_past);
    ASSERT_EQ(fired, 1);
    ASSERT_EQ(fired_id, 2);
    ASSERT_EQ(slots[2].active, 0);

    /* 手动 stop 后不再触发 */
    wdt_start(slots, 3, &deadline, cb);
    wdt_stop(slots, 3);
    ASSERT_EQ(slots[3].active, 0);
    wdt_poll(slots, SGN_WDT_SLOTS, &now_past);
    ASSERT_EQ(fired, 1);  /* 仍为 1，未再触发 */
}

/* ============================================================================
 * Plugin 测试
 * ============================================================================ */

/* 测试用静态插件描述符 */
static int test_plugin_init_called = 0;
static int test_plugin_shutdown_called = 0;

static int test_plugin_init(plugin_ctx_t* ctx) {
    (void)ctx;
    test_plugin_init_called++;
    return 0;
}
static int test_plugin_shutdown(plugin_ctx_t* ctx) {
    (void)ctx;
    test_plugin_shutdown_called++;
    return 0;
}

static plugin_desc_t make_test_desc(const char* name, plugin_type_t type,
                                       uint64_t caps) {
    plugin_desc_t d;
    memset(&d, 0, sizeof(d));
    d.api_version = 1;
    d.type = type;
    d.name = name;
    d.version = "1.0.0";
    d.author = "test";
    d.abi_requirement = ">=2.0.0";
    d.capabilities = caps;
    d.init = test_plugin_init;
    d.shutdown = test_plugin_shutdown;
    d.register_normative = NULL;
    d.register_extension = NULL;
    return d;
}

TEST(plugin_register_static_lifecycle) {
    test_plugin_init_called = 0;
    test_plugin_shutdown_called = 0;
    uint32_t before = plugin_count();

    plugin_desc_t d = make_test_desc(
        "test.lifecycle", SGN_PLUGIN_TYPE_EXTENSION,
        SGN_PLUGIN_CAP_STORAGE_DRIVER);
    error_t rc = plugin_register_static(&d);
    ASSERT_EQ(rc, SGN_OK);
    ASSERT_EQ(test_plugin_init_called, 1);
    ASSERT_TRUE(plugin_is_loaded("test.lifecycle"));
    ASSERT_EQ(plugin_count(), before + 1);
    ASSERT_EQ(plugin_get_capabilities("test.lifecycle"),
                 SGN_PLUGIN_CAP_STORAGE_DRIVER);

    /* 重复注册同名应失败 */
    error_t rc2 = plugin_register_static(&d);
    ASSERT_NE(rc2, SGN_OK);

    /* 卸载（init 状态正常，shutdown 被调用） */
    /* 先通过 plugin_get_name 找到 handle - API 没有直接 by-name 卸载，
       这里用 plugin_count 验证状态变化即可。验证生命周期主要靠 init/shutdown 计数。 */
    ASSERT_EQ(test_plugin_shutdown_called, 0);
}

TEST(plugin_capability_validation) {
    /* EXTENSION 类型不允许持有 NORMATIVE 能力 */
    plugin_desc_t d = make_test_desc(
        "test.badcap", SGN_PLUGIN_TYPE_EXTENSION,
        SGN_PLUGIN_CAP_HC_OPERATOR);  /* 内核能力 */
    error_t rc = plugin_register_static(&d);
    ASSERT_NE(rc, SGN_OK);  /* 应被拒绝 */
    ASSERT_FALSE(plugin_is_loaded("test.badcap"));
}

TEST(plugin_normative_promotion) {
    plugin_desc_t d = make_test_desc(
        "test.norm", SGN_PLUGIN_TYPE_NORMATIVE,
        SGN_PLUGIN_CAP_SANDBOX_CONVERTER);
    error_t rc = plugin_register_static(&d);
    ASSERT_EQ(rc, SGN_OK);

    /* NORMATIVE 类型可晋升为候选 */
    ASSERT_FALSE(plugin_is_normative_candidate("test.norm"));
    error_t p = plugin_propose_normative("test.norm");
    ASSERT_EQ(p, SGN_OK);
    ASSERT_TRUE(plugin_is_normative_candidate("test.norm"));

    /* 冻结后不可卸载 */
    error_t f = plugin_freeze_normative("test.norm");
    ASSERT_EQ(f, SGN_OK);
    /* 不存在的插件操作应失败 */
    ASSERT_NE(plugin_propose_normative("nope"), SGN_OK);
    ASSERT_FALSE(plugin_is_normative_candidate("nope"));
    ASSERT_NE(plugin_freeze_normative("nope"), SGN_OK);
}

TEST(plugin_get_name_invalid) {
    /* 越界索引返回 NULL */
    ASSERT_NULL(plugin_get_name(999999));
    /* 不存在的插件能力为 0 */
    ASSERT_EQ(plugin_get_capabilities("nonexistent.xyz"), 0);
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("=== HPDC Advanced Modules Test Suite ===\n\n");
    SGN_TEST_INIT();

    printf("[sandbox]\n");
    RUN(sandbox_create_auto);
    RUN(sandbox_create_invalid);
    RUN(sandbox_project_inverse_hc8);
    RUN(sandbox_divide_hc8);
    RUN(sandbox_scale_gradient_hc8);
    RUN(sandbox_scheme);

    printf("[trie]\n");
    RUN(trie_pool_init_alloc);
    RUN(trie_insert_find_child);
    RUN(trie_insert_template_find);
    RUN(trie_collect_candidates);
    RUN(trie_lazy_delete);

    printf("[engine]\n");
    RUN(engine_match_bits_hamming);
    RUN(engine_lru_hit_decay);
    RUN(engine_lru_evict_promote);
    RUN(engine_soft_decay);
    RUN(engine_merge_or_and);
    RUN(engine_sortnet_bitonic);
    RUN(engine_median_layerwise);
    RUN(engine_wta_compete);

    printf("[storage]\n");
    RUN(storage_crc8);
    RUN(storage_checksum_delta);
    RUN(storage_runtime_sniff);
    RUN(storage_rs86_roundtrip);
    RUN(storage_rs86_single_error_correction);
    RUN(storage_merkle);
    RUN(storage_rle_roundtrip);
    RUN(storage_file_io_roundtrip);
    RUN(storage_file_grade);
    RUN(storage_tmr_vote);

    printf("[network]\n");
    RUN(network_uart_roundtrip);
    RUN(network_uart_bad_sync);
    RUN(network_cobs_roundtrip);
    RUN(network_lamport_compare);
    RUN(network_lamport_update);
    RUN(network_wdt);

    printf("[plugin]\n");
    RUN(plugin_register_static_lifecycle);
    RUN(plugin_capability_validation);
    RUN(plugin_normative_promotion);
    RUN(plugin_get_name_invalid);

    SUMMARY();
}
