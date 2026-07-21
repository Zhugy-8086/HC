// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file test_sgn.c
 * @brief SGN 全模块自动化测试 - 数据驱动
 * Build: tcc -Isgn/include -Itests sgn/src/hc.c sgn/src/hc8.c sgn/src/hc16.c sgn/src/hc32.c sgn/src/hc64.c sgn/src/dc.c sgn/src/hc_simd.c tests/test_sgn.c -o tests/test_sgn.exe
 */

#include "sgn_test.h"
#include "hc/hc.h"
#include "hc/hc8.h"
#include "hc/hc16.h"
#include "hc/hc32.h"
#include "hc/hc64.h"
#include "hc/dc.h"
#include "hc/hc_simd.h"
#include "hc/sgn_macros.h"
#include "hc/hpdc_sandbox.h"
#include "hc/hpdc_trie.h"

/* ============================================================================
 * 测试数据定义（外部于测试函数，便于扩展）
 * ============================================================================ */

/* HC8 roundtrip 测试值 */
TEST_ARRAY(double, hc8_values,
    0.0, 1.0, 3.14159, 100.5, 255.999, 42.0, 0.001, 128.0
);

/* HC16 roundtrip 测试值 */
TEST_ARRAY(double, hc16_values,
    0.0, 1.0, 1000.5, 32768.0, 65535.999, 42.0, 0.001
);

/* HC32 roundtrip 测试值 */
TEST_ARRAY(double, hc32_values,
    0.0, 1.0, 1000000.0, 4294967295.0, 42.0
);

/* DC roundtrip 测试值 */
TEST_ARRAY(double, dc_values,
    0.0, 3.14, -2.5, 100.0, 0.001
);

/* 加法测试对 (a, b, expected_sum) */
typedef struct { double a; double b; double expected; } add_pair_t;
TEST_ARRAY(add_pair_t, hc8_add_pairs,
    {3.0, 2.0, 5.0},
    {0.0, 0.0, 0.0},
    {100.0, 155.0, 255.0},
    {200.0, 200.0, 255.999},  /* 饱和 */
    {0.5, 0.3, 0.8}
);

TEST_ARRAY(add_pair_t, hc16_add_pairs,
    {1000.0, 500.0, 1500.0},
    {0.0, 0.0, 0.0},
    {32000.0, 33000.0, 65000.0},
    {50000.0, 50000.0, 65535.999}  /* 饱和 */
);

TEST_ARRAY(add_pair_t, dc_add_pairs,
    {3.14, 2.86, 6.0},
    {0.0, 0.0, 0.0},
    {-1.5, 1.5, 0.0}
);

/* 减法测试对 */
TEST_ARRAY(add_pair_t, hc8_sub_pairs,
    {5.0, 3.0, 2.0},
    {3.0, 5.0, 0.0},   /* 下溢饱和到 0 */
    {100.0, 50.0, 50.0}
);

TEST_ARRAY(add_pair_t, hc16_sub_pairs,
    {1500.0, 500.0, 1000.0},
    {500.0, 1500.0, 0.0}
);

/* 软阈值测试对 (input, lambda, expected) */
TEST_ARRAY(add_pair_t, soft_thresh_pairs,
    {10.0, 3.0, 7.0},
    {2.0, 3.0, 0.0},
    {3.0, 3.0, 0.0},
    {100.0, 50.0, 50.0}
);

/* ============================================================================
 * hc.c 测试
 * ============================================================================ */

TEST(hc_meta) {
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_8)->layers, 6);
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_16)->layers, 4);
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_32)->layers, 3);
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_64)->layers, 2);
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_8)->bits, 8);
    ASSERT_EQ(hc_meta_get(SGN_HC_KIND_16)->bits, 16);
    ASSERT_NOT_NULL(hc_meta_get(SGN_HC_KIND_8)->name);
    ASSERT_NULL(hc_meta_get(99));
}

TEST(hc_precision) {
    ASSERT_EQ(precision_layers(SGN_PREC_ARCHIVE), 0);
    ASSERT_EQ(precision_layers(SGN_PREC_FAST), 2);
    ASSERT_EQ(precision_layers(SGN_PREC_STD), 4);
    ASSERT_EQ(precision_layers(SGN_PREC_PREC), 6);
    ASSERT_EQ(precision_to_hc_kind(SGN_PREC_PREC), SGN_HC_KIND_8);
    ASSERT_EQ(precision_to_hc_kind(SGN_PREC_STD), SGN_HC_KIND_16);
}

TEST(hc_sandbox_validation) {
    ASSERT_TRUE(sandbox_int_bits_valid(SGN_HC_KIND_8, 8));
    ASSERT_FALSE(sandbox_int_bits_valid(SGN_HC_KIND_8, 16));
    ASSERT_TRUE(sandbox_int_bits_valid(SGN_HC_KIND_16, 16));
    ASSERT_FALSE(sandbox_int_bits_valid(SGN_HC_KIND_16, 32));
    ASSERT_TRUE(sandbox_int_bits_valid(SGN_HC_KIND_32, 32));
    ASSERT_TRUE(sandbox_int_bits_valid(SGN_HC_KIND_64, 64));
    ASSERT_EQ(sandbox_default_int_bits(SGN_HC_KIND_8), 8);
    ASSERT_EQ(sandbox_default_int_bits(SGN_HC_KIND_16), 16);
}

TEST(hc_trie_depth) {
    ASSERT_EQ(trie_depth_for_hc(SGN_HC_KIND_8), 6);
    ASSERT_EQ(trie_depth_for_hc(SGN_HC_KIND_16), 4);
    ASSERT_EQ(trie_depth_for_hc(SGN_HC_KIND_32), 3);
    ASSERT_EQ(trie_depth_for_hc(SGN_HC_KIND_64), 2);
}

TEST(hc_version) {
    uint32_t v = abi_version();
    ASSERT_EQ((v >> 16) & 0xFF, 2);  /* major */
    ASSERT_EQ((v >> 8) & 0xFF, 0);   /* minor */
    ASSERT_EQ(v & 0xFF, 0);          /* patch */
}

TEST(hc_error_string) {
    ASSERT_NOT_NULL(error_string(SGN_OK));
    ASSERT_NOT_NULL(error_string(SGN_ERR_NOMEM));
    ASSERT_NOT_NULL(error_string(SGN_ERR_RS_UNCORRECTABLE));
}

/* ============================================================================
 * hc8.c 测试
 * ============================================================================ */

TEST(hc8_roundtrip) {
    for (int i = 0; i < hc8_values_count; i++) {
        hc8_t h = hc8_from_double(hc8_values[i], SGN_OVERFLOW_SATURATE);
        ASSERT_NEAR(hc8_to_double(h), hc8_values[i], 0.01);
    }
}

TEST(hc8_add) {
    for (int i = 0; i < hc8_add_pairs_count; i++) {
        hc8_t a = hc8_from_double(hc8_add_pairs[i].a, SGN_OVERFLOW_SATURATE);
        hc8_t b = hc8_from_double(hc8_add_pairs[i].b, SGN_OVERFLOW_SATURATE);
        hc8_t c = hc8_add_sat(&a, &b);
        ASSERT_NEAR(hc8_to_double(c), hc8_add_pairs[i].expected, 0.1);
    }
}

TEST(hc8_sub) {
    for (int i = 0; i < hc8_sub_pairs_count; i++) {
        hc8_t a = hc8_from_double(hc8_sub_pairs[i].a, SGN_OVERFLOW_SATURATE);
        hc8_t b = hc8_from_double(hc8_sub_pairs[i].b, SGN_OVERFLOW_SATURATE);
        hc8_t c = hc8_sub(&a, &b);
        ASSERT_NEAR(hc8_to_double(c), hc8_sub_pairs[i].expected, 0.1);
    }
}

TEST(hc8_compare) {
    hc8_t a = hc8_from_double(1.0, SGN_OVERFLOW_SATURATE);
    hc8_t b = hc8_from_double(2.0, SGN_OVERFLOW_SATURATE);
    ASSERT_TRUE(hc8_less(&a, &b));
    ASSERT_FALSE(hc8_less(&b, &a));
    ASSERT_FALSE(hc8_less(&a, &a));
    ASSERT_TRUE(hc8_equal(&a, &a));
    ASSERT_FALSE(hc8_equal(&a, &b));
}

TEST(hc8_soft_threshold) {
    for (int i = 0; i < soft_thresh_pairs_count; i++) {
        hc8_t x = hc8_from_double(soft_thresh_pairs[i].a, SGN_OVERFLOW_SATURATE);
        hc8_t l = hc8_from_double(soft_thresh_pairs[i].b, SGN_OVERFLOW_SATURATE);
        hc8_t r = hc8_soft_threshold(&x, &l);
        ASSERT_NEAR(hc8_to_double(r), soft_thresh_pairs[i].expected, 0.1);
    }
}

TEST(hc8_checksum) {
    hc8_t h = hc8_from_double(42.0, SGN_OVERFLOW_SATURATE);
    uint8_t cs = hc8_checksum(&h);
    ASSERT_TRUE(cs != 0);
    /* 确定性：同样输入同样输出 */
    ASSERT_EQ(hc8_checksum(&h), cs);
}

TEST(hc8_constants) {
    ASSERT_NEAR(hc8_to_double(SGN_HC8_ZERO), 0.0, 0.001);
    ASSERT_TRUE(hc8_to_double(SGN_HC8_MAX) > 255.0);
    ASSERT_TRUE(hc8_less(&SGN_HC8_ZERO, &SGN_HC8_MAX));
}

TEST(hc8_shift) {
    hc8_t a = hc8_from_double(4.0, SGN_OVERFLOW_SATURATE);
    hc8_t shifted = hc8_shift_right(&a, 1);
    ASSERT_NEAR(hc8_to_double(shifted), 2.0, 0.01);
}

/* ============================================================================
 * hc16.c 测试
 * ============================================================================ */

TEST(hc16_roundtrip) {
    for (int i = 0; i < hc16_values_count; i++) {
        hc16_t h = hc16_from_double(hc16_values[i], SGN_OVERFLOW_SATURATE);
        ASSERT_NEAR(hc16_to_double(h), hc16_values[i], 0.01);
    }
}

TEST(hc16_add) {
    for (int i = 0; i < hc16_add_pairs_count; i++) {
        hc16_t a = hc16_from_double(hc16_add_pairs[i].a, SGN_OVERFLOW_SATURATE);
        hc16_t b = hc16_from_double(hc16_add_pairs[i].b, SGN_OVERFLOW_SATURATE);
        hc16_t c = hc16_add_sat(&a, &b);
        ASSERT_NEAR(hc16_to_double(c), hc16_add_pairs[i].expected, 0.1);
    }
}

TEST(hc16_sub) {
    for (int i = 0; i < hc16_sub_pairs_count; i++) {
        hc16_t a = hc16_from_double(hc16_sub_pairs[i].a, SGN_OVERFLOW_SATURATE);
        hc16_t b = hc16_from_double(hc16_sub_pairs[i].b, SGN_OVERFLOW_SATURATE);
        hc16_t c = hc16_sub(&a, &b);
        ASSERT_NEAR(hc16_to_double(c), hc16_sub_pairs[i].expected, 0.1);
    }
}

TEST(hc16_compare) {
    hc16_t a = hc16_from_double(1000.0, SGN_OVERFLOW_SATURATE);
    hc16_t b = hc16_from_double(2000.0, SGN_OVERFLOW_SATURATE);
    ASSERT_TRUE(hc16_less(&a, &b));
    ASSERT_FALSE(hc16_less(&b, &a));
    ASSERT_TRUE(hc16_equal(&a, &a));
}

TEST(hc16_soft_threshold) {
    hc16_t x = hc16_from_double(1000.0, SGN_OVERFLOW_SATURATE);
    hc16_t l = hc16_from_double(300.0, SGN_OVERFLOW_SATURATE);
    hc16_t r = hc16_soft_threshold(&x, &l);
    ASSERT_NEAR(hc16_to_double(r), 700.0, 0.1);
}

TEST(hc16_checksum) {
    hc16_t h = hc16_from_double(1234.5, SGN_OVERFLOW_SATURATE);
    ASSERT_TRUE(hc16_checksum(&h) != 0);
}

/* ============================================================================
 * hc32.c 测试
 * ============================================================================ */

TEST(hc32_roundtrip) {
    for (int i = 0; i < hc32_values_count; i++) {
        hc32_t h = hc32_from_double(hc32_values[i], SGN_OVERFLOW_SATURATE);
        ASSERT_NEAR(hc32_to_double(h), hc32_values[i], 1.0);
    }
}

TEST(hc32_add) {
    hc32_t a = hc32_from_double(1000000.0, SGN_OVERFLOW_SATURATE);
    hc32_t b = hc32_from_double(2000000.0, SGN_OVERFLOW_SATURATE);
    hc32_t c = hc32_add_sat(&a, &b);
    ASSERT_TRUE(hc32_less(&a, &c));
    ASSERT_NEAR(hc32_to_double(c), 3000000.0, 1.0);
}

TEST(hc32_sub) {
    hc32_t a = hc32_from_double(3000000.0, SGN_OVERFLOW_SATURATE);
    hc32_t b = hc32_from_double(1000000.0, SGN_OVERFLOW_SATURATE);
    hc32_t c = hc32_sub(&a, &b);
    ASSERT_NEAR(hc32_to_double(c), 2000000.0, 1.0);
}

TEST(hc32_compare) {
    hc32_t a = hc32_from_double(1.0, SGN_OVERFLOW_SATURATE);
    hc32_t b = hc32_from_double(2.0, SGN_OVERFLOW_SATURATE);
    ASSERT_TRUE(hc32_less(&a, &b));
    ASSERT_TRUE(hc32_equal(&a, &a));
    ASSERT_FALSE(hc32_equal(&a, &b));
}

TEST(hc32_soft_threshold) {
    hc32_t x = hc32_from_double(100.0, SGN_OVERFLOW_SATURATE);
    hc32_t l = hc32_from_double(30.0, SGN_OVERFLOW_SATURATE);
    hc32_t r = hc32_soft_threshold(&x, &l);
    ASSERT_NEAR(hc32_to_double(r), 70.0, 0.1);
}

/* ============================================================================
 * hc64.c 测试
 * ============================================================================ */

TEST(hc64_roundtrip) {
    hc64_t h = hc64_from_double(1e15, SGN_OVERFLOW_SATURATE);
    ASSERT_NEAR(hc64_to_double(h), 1e15, 1.0);
}

TEST(hc64_add) {
    hc64_t a = hc64_from_double(1e15, SGN_OVERFLOW_SATURATE);
    hc64_t b = hc64_from_double(2e15, SGN_OVERFLOW_SATURATE);
    hc64_t c = hc64_add_sat(&a, &b);
    ASSERT_TRUE(hc64_less(&a, &c));
}

TEST(hc64_compare) {
    hc64_t a = hc64_from_double(1.0, SGN_OVERFLOW_SATURATE);
    hc64_t b = hc64_from_double(2.0, SGN_OVERFLOW_SATURATE);
    ASSERT_TRUE(hc64_less(&a, &b));
    ASSERT_TRUE(hc64_equal(&a, &a));
}

TEST(hc64_constants) {
    ASSERT_NEAR(hc64_to_double(SGN_HC64_ZERO), 0.0, 0.001);
    ASSERT_TRUE(hc64_less(&SGN_HC64_ZERO, &SGN_HC64_MAX));
}

TEST(hc64_sub) {
    hc64_t a = hc64_from_double(5e15, SGN_OVERFLOW_SATURATE);
    hc64_t b = hc64_from_double(2e15, SGN_OVERFLOW_SATURATE);
    hc64_t c = hc64_sub(&a, &b);
    ASSERT_NEAR(hc64_to_double(c), 3e15, 1.0);

    /* 下溢 */
    hc64_t d = hc64_from_double(1.0, SGN_OVERFLOW_SATURATE);
    hc64_t e = hc64_from_double(2.0, SGN_OVERFLOW_SATURATE);
    hc64_t f = hc64_sub(&d, &e);
    ASSERT_NEAR(hc64_to_double(f), 0.0, 0.001);
}

TEST(hc64_soft_threshold) {
    hc64_t x = hc64_from_double(100.0, SGN_OVERFLOW_SATURATE);
    hc64_t l = hc64_from_double(30.0, SGN_OVERFLOW_SATURATE);
    hc64_t r = hc64_soft_threshold(&x, &l);
    ASSERT_NEAR(hc64_to_double(r), 70.0, 0.1);

    /* X <= Lambda 时归零 */
    hc64_t x2 = hc64_from_double(10.0, SGN_OVERFLOW_SATURATE);
    hc64_t l2 = hc64_from_double(20.0, SGN_OVERFLOW_SATURATE);
    hc64_t r2 = hc64_soft_threshold(&x2, &l2);
    ASSERT_NEAR(hc64_to_double(r2), 0.0, 0.001);
}

TEST(hc64_shift) {
    hc64_t a = hc64_from_double(4.0, SGN_OVERFLOW_SATURATE);
    hc64_t shifted = hc64_shift_right(&a, 1);
    ASSERT_NEAR(hc64_to_double(shifted), 2.0, 0.01);

    /* shift == 0 返回原值 */
    hc64_t same = hc64_shift_right(&a, 0);
    ASSERT_NEAR(hc64_to_double(same), 4.0, 0.001);

    /* shift >= 64 返回 ZERO */
    hc64_t zero = hc64_shift_right(&a, 64);
    ASSERT_NEAR(hc64_to_double(zero), 0.0, 0.001);
}

/* ============================================================================
 * dc.c 测试
 * ============================================================================ */

TEST(dc_roundtrip) {
    for (int i = 0; i < dc_values_count; i++) {
        dc_t d = dc_from_double(dc_values[i], 3);
        ASSERT_NEAR(dc_to_double(&d), dc_values[i], 0.01);
    }
}

TEST(dc_add) {
    for (int i = 0; i < dc_add_pairs_count; i++) {
        dc_t a = dc_from_double(dc_add_pairs[i].a, 3);
        dc_t b = dc_from_double(dc_add_pairs[i].b, 3);
        dc_t c = dc_add(&a, &b);
        ASSERT_NEAR(dc_to_double(&c), dc_add_pairs[i].expected, 0.01);
    }
}

TEST(dc_compare) {
    dc_t a = dc_from_double(1.0, 2);
    dc_t b = dc_from_double(2.0, 2);
    ASSERT_TRUE(dc_less(&a, &b));
    ASSERT_FALSE(dc_less(&b, &a));
    ASSERT_TRUE(dc_equal(&a, &a));
}

TEST(dc_mul) {
    dc_t a = dc_from_double(3.0, 2);
    dc_t b = dc_from_double(4.0, 2);
    dc_t c = dc_mul(&a, &b);
    ASSERT_NEAR(dc_to_double(&c), 12.0, 0.01);
}

TEST(dc_to_level) {
    dc_t a = dc_from_double(3.14, 2);
    dc_t b = dc_to_level(&a, 4);
    ASSERT_EQ(b.level, 4);
    ASSERT_NEAR(dc_to_double(&b), 3.14, 0.01);
}

/* ============================================================================
 * hc_simd.c 测试（标量回退路径）
 * ============================================================================ */

TEST(simd_batch_add) {
    hc16_t a[4], b[4], out[4];
    for (int i = 0; i < 4; i++) {
        a[i] = hc16_from_double(100.0 * (i + 1), SGN_OVERFLOW_SATURATE);
        b[i] = hc16_from_double(50.0, SGN_OVERFLOW_SATURATE);
    }
    hc16_add_sat_batch(a, b, out, 4);
    ASSERT_NEAR(hc16_to_double(out[0]), 150.0, 0.1);
    ASSERT_NEAR(hc16_to_double(out[3]), 450.0, 0.1);
}

TEST(simd_batch_threshold) {
    hc16_t a[4], l[4], out[4];
    for (int i = 0; i < 4; i++) {
        a[i] = hc16_from_double(100.0 * (i + 1), SGN_OVERFLOW_SATURATE);
        l[i] = hc16_from_double(150.0, SGN_OVERFLOW_SATURATE);
    }
    hc16_soft_threshold_batch(a, l, out, 4);
    ASSERT_NEAR(hc16_to_double(out[0]), 0.0, 0.1);     /* 100-150 = 0 (saturate) */
    ASSERT_NEAR(hc16_to_double(out[1]), 50.0, 0.1);    /* 200-150 = 50 */
    ASSERT_NEAR(hc16_to_double(out[3]), 250.0, 0.1);   /* 400-150 = 250 */
}

TEST(simd_batch_scale) {
    hc16_t a[2], out[2];
    a[0] = hc16_from_double(1000.0, SGN_OVERFLOW_SATURATE);
    a[1] = hc16_from_double(2000.0, SGN_OVERFLOW_SATURATE);
    hc16_scale_batch(a, 32768, out, 2);  /* 0.5 in Q16 */
    ASSERT_NEAR(hc16_to_double(out[0]), 500.0, 1.0);
}

TEST(simd_hc64_batch_add) {
    hc64_t a[2], b[2], out[2];
    a[0] = hc64_from_double(1e15, SGN_OVERFLOW_SATURATE);
    a[1] = hc64_from_double(2e15, SGN_OVERFLOW_SATURATE);
    b[0] = hc64_from_double(3e15, SGN_OVERFLOW_SATURATE);
    b[1] = hc64_from_double(4e15, SGN_OVERFLOW_SATURATE);
    hc64_add_sat_batch(a, b, out, 2);
    ASSERT_TRUE(hc64_less(&a[0], &out[0]));
    ASSERT_TRUE(hc64_less(&a[1], &out[1]));
}

/* ============================================================================
 * 性能基准（可选）
 * ============================================================================ */

TEST(bench_hc8_add) {
    hc8_t a = hc8_from_double(100.0, SGN_OVERFLOW_SATURATE);
    hc8_t b = hc8_from_double(50.0, SGN_OVERFLOW_SATURATE);
    BENCH_START();
    volatile hc8_t c;
    for (int i = 0; i < 100000; i++) {
        c = hc8_add_sat(&a, &b);
    }
    BENCH_END("hc8_add x100k", 100000);
}

TEST(bench_hc16_add) {
    hc16_t a = hc16_from_double(1000.0, SGN_OVERFLOW_SATURATE);
    hc16_t b = hc16_from_double(500.0, SGN_OVERFLOW_SATURATE);
    BENCH_START();
    volatile hc16_t c;
    for (int i = 0; i < 100000; i++) {
        c = hc16_add_sat(&a, &b);
    }
    BENCH_END("hc16_add x100k", 100000);
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

TEST(shc8_add) {
    shc8_t zero = {0, 0, {{0,0,0,0,0,0}}};
    shc8_t pos3 = {0, 3, {{0,0,0,0,0,0}}};
    shc8_t pos2 = {0, 2, {{0,0,0,0,0,0}}};
    shc8_t neg3 = {1, 3, {{0,0,0,0,0,0}}};
    shc8_t neg2 = {1, 2, {{0,0,0,0,0,0}}};

    /* 同号相加：正 + 正 */
    shc8_t r1 = shc8_add(&pos3, &pos2);
    ASSERT_EQ(r1.sign, 0);
    ASSERT_EQ(r1.int_part, 5);

    /* 同号相加：负 + 负 */
    shc8_t r2 = shc8_add(&neg3, &neg2);
    ASSERT_EQ(r2.sign, 1);
    ASSERT_EQ(r2.int_part, 5);

    /* 异号相加：正 + 负，|3| > |2|，结果正 1 */
    shc8_t r3 = shc8_add(&pos3, &neg2);
    ASSERT_EQ(r3.sign, 0);
    ASSERT_EQ(r3.int_part, 1);

    /* 异号相加：正 + 负，|2| < |3|，结果负 1 */
    shc8_t r4 = shc8_add(&pos2, &neg3);
    ASSERT_EQ(r4.sign, 1);
    ASSERT_EQ(r4.int_part, 1);

    /* 异号相加：|a| == |b|，归零 */
    shc8_t r5 = shc8_add(&pos3, &neg3);
    ASSERT_EQ(r5.sign, 0);
    ASSERT_EQ(r5.int_part, 0);

    /* 加零 */
    shc8_t r6 = shc8_add(&pos3, &zero);
    ASSERT_EQ(r6.sign, 0);
    ASSERT_EQ(r6.int_part, 3);
}

TEST(shc8_sub) {
    shc8_t pos5 = {0, 5, {{0,0,0,0,0,0}}};
    shc8_t pos3 = {0, 3, {{0,0,0,0,0,0}}};
    shc8_t neg3 = {1, 3, {{0,0,0,0,0,0}}};

    /* 5 - 3 = 2 */
    shc8_t r1 = shc8_sub(&pos5, &pos3);
    ASSERT_EQ(r1.sign, 0);
    ASSERT_EQ(r1.int_part, 2);

    /* 5 - (-3) = 8 */
    shc8_t r2 = shc8_sub(&pos5, &neg3);
    ASSERT_EQ(r2.sign, 0);
    ASSERT_EQ(r2.int_part, 8);

    /* 3 - 5 = -2 */
    shc8_t r3 = shc8_sub(&pos3, &pos5);
    ASSERT_EQ(r3.sign, 1);
    ASSERT_EQ(r3.int_part, 2);
}

TEST(shc8_macros) {
    shc8_t a = {0, 10, {{0,0,0,0,0,0}}};
    shc8_t b = {1, 3, {{0,0,0,0,0,0}}};

    shc8_t r = SHC8_ADD(a, b);
    ASSERT_EQ(r.sign, 0);
    ASSERT_EQ(r.int_part, 7);

    shc8_t d = SHC8_SUB(a, b);
    ASSERT_EQ(d.sign, 0);
    ASSERT_EQ(d.int_part, 13);

    ASSERT_TRUE(SHC8_LESS(b, a));
}

TEST(macro_convenience) {
    /* 转换宏 */
    hc8_t a8 = HC8_FROM_DOUBLE(3.14);
    hc16_t a16 = HC16_FROM_DOUBLE(1000.5);
    hc32_t a32 = HC32_FROM_DOUBLE(123456.0);
    hc64_t a64 = HC64_FROM_DOUBLE(1e9);

    ASSERT_NEAR(HC8_TO_DOUBLE(a8), 3.14, 0.01);
    ASSERT_NEAR(HC16_TO_DOUBLE(a16), 1000.5, 0.1);
    ASSERT_NEAR(HC32_TO_DOUBLE(a32), 123456.0, 1.0);
    ASSERT_NEAR(HC64_TO_DOUBLE(a64), 1e9, 100.0);

    /* 加法宏 */
    hc8_t b8 = HC8_FROM_DOUBLE(2.0);
    hc8_t sum8 = HC8_ADD(a8, b8);
    ASSERT_NEAR(HC8_TO_DOUBLE(sum8), 5.14, 0.01);

    hc16_t b16 = HC16_FROM_DOUBLE(500.0);
    hc16_t sum16 = HC16_ADD(a16, b16);
    ASSERT_NEAR(HC16_TO_DOUBLE(sum16), 1500.5, 0.1);

    /* 减法宏 */
    hc8_t sub8 = HC8_SUB(sum8, b8);
    ASSERT_NEAR(HC8_TO_DOUBLE(sub8), 3.14, 0.01);

    /* 比较宏 */
    ASSERT_TRUE(HC8_LESS(b8, a8));
    ASSERT_TRUE(HC8_EQUAL(a8, a8));

    ASSERT_TRUE(HC16_LESS(b16, a16));
    ASSERT_TRUE(HC16_EQUAL(a16, a16));

    /* 软阈值宏 */
    hc8_t X8 = HC8_FROM_DOUBLE(10.0);
    hc8_t L8 = HC8_FROM_DOUBLE(3.0);
    hc8_t st8 = HC8_SOFT_THRESH(X8, L8);
    ASSERT_NEAR(HC8_TO_DOUBLE(st8), 7.0, 0.01);
}

int main(void) {
    printf("=== SGN Automated Test Suite ===\n\n");

    printf("[hc.c]\n");
    RUN(hc_meta);
    RUN(hc_precision);
    RUN(hc_sandbox_validation);
    RUN(hc_trie_depth);
    RUN(hc_version);
    RUN(hc_error_string);

    printf("[hc8.c]\n");
    RUN(hc8_roundtrip);
    RUN(hc8_add);
    RUN(hc8_sub);
    RUN(hc8_compare);
    RUN(hc8_soft_threshold);
    RUN(hc8_checksum);
    RUN(hc8_constants);
    RUN(hc8_shift);

    printf("[shc8.c]\n");
    RUN(shc8_add);
    RUN(shc8_sub);
    RUN(shc8_macros);

    printf("[hc16.c]\n");
    RUN(hc16_roundtrip);
    RUN(hc16_add);
    RUN(hc16_sub);
    RUN(hc16_compare);
    RUN(hc16_soft_threshold);
    RUN(hc16_checksum);

    printf("[hc32.c]\n");
    RUN(hc32_roundtrip);
    RUN(hc32_add);
    RUN(hc32_sub);
    RUN(hc32_compare);
    RUN(hc32_soft_threshold);

    printf("[hc64.c]\n");
    RUN(hc64_roundtrip);
    RUN(hc64_add);
    RUN(hc64_compare);
    RUN(hc64_constants);
    RUN(hc64_sub);
    RUN(hc64_soft_threshold);
    RUN(hc64_shift);

    printf("[dc.c]\n");
    RUN(dc_roundtrip);
    RUN(dc_add);
    RUN(dc_compare);
    RUN(dc_mul);
    RUN(dc_to_level);

    printf("[hc_simd.c]\n");
    RUN(simd_batch_add);
    RUN(simd_batch_threshold);
    RUN(simd_batch_scale);
    RUN(simd_hc64_batch_add);

    printf("[bench]\n");
    RUN(bench_hc8_add);
    RUN(bench_hc16_add);

    printf("[macros]\n");
    RUN(macro_convenience);

    SUMMARY();
}
