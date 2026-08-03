// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc64.c
 * @brief SGN HC64 全部运算实现
 * @version 2.0.0
 *
 * HC64: 2层 × 64位，基数 2^64。
 * 注意：double 尾数仅 53 位，from_double 采用拆分策略避免精度丢失。
 *
 * 依赖：hc64.h, hc.h
 */

#include "hc/hc64.h"
#include "hc/hc.h"
#include <string.h>

/* ============================================================================
 * 常量
 * ============================================================================ */

hc64_t SGN_HC64_ZERO = {{0, 0}};
hc64_t SGN_HC64_MAX  = {{0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};

/* ============================================================================
 * 比较
 * ============================================================================ */

bool hc64_less(const hc64_t* SGN_RESTRICT a, const hc64_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    /* 多字节层：不能直接用 memcmp（小端序下字节序与数值序不一致） */
    for (int i = 0; i < 2; ++i) {
        if (a->v[i] != b->v[i]) return a->v[i] < b->v[i];
    }
    return false;
}

bool hc64_equal(const hc64_t* SGN_RESTRICT a, const hc64_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    return memcmp(a, b, sizeof(hc64_t)) == 0;
}

/* ============================================================================
 * 加法（使用 __uint128_t 或拆分为两个 32 位部分）
 * ============================================================================ */

hc64_t hc64_add_sat(const hc64_t* SGN_RESTRICT a, const hc64_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC64_ZERO;
    hc64_t c;
    /* 从 v[1]（最低层）开始 */
    uint64_t sum1 = a->v[1] + b->v[1];
    uint64_t carry = (sum1 < a->v[1]) ? 1 : 0;
    c.v[1] = sum1;

    uint64_t sum0 = a->v[0] + b->v[0] + carry;
    /* 检测溢出：若 carry=1 且 sum0 < b->v[0]，或 carry=0 且 sum0 < a->v[0] */
    if (carry ? (sum0 <= b->v[0]) : (sum0 < a->v[0])) {
        c = SGN_HC64_MAX;
    } else {
        c.v[0] = sum0;
    }
    return c;
}

hc64_t hc64_add_wrap(const hc64_t* SGN_RESTRICT a, const hc64_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC64_ZERO;
    hc64_t c;
    uint64_t sum1 = a->v[1] + b->v[1];
    uint64_t carry = (sum1 < a->v[1]) ? 1 : 0;
    c.v[1] = sum1;
    c.v[0] = a->v[0] + b->v[0] + carry;
    return c;
}

/* ============================================================================
 * 减法
 * ============================================================================ */

hc64_t hc64_sub(const hc64_t* SGN_RESTRICT a, const hc64_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC64_ZERO;
    hc64_t c;
    uint64_t diff1 = a->v[1] - b->v[1];
    uint64_t borrow = (a->v[1] < b->v[1]) ? 1 : 0;
    c.v[1] = diff1;

    if (a->v[0] < b->v[0] || (a->v[0] == b->v[0] && borrow)) {
        c = SGN_HC64_ZERO;
    } else {
        c.v[0] = a->v[0] - b->v[0] - borrow;
    }
    return c;
}

/* ============================================================================
 * 软阈值
 * ============================================================================ */

hc64_t hc64_soft_threshold(const hc64_t* SGN_RESTRICT X, const hc64_t* SGN_RESTRICT Lambda) {
    if (!X || !Lambda) return SGN_HC64_ZERO;
    if (hc64_less(X, Lambda) || hc64_equal(X, Lambda))
        return SGN_HC64_ZERO;
    return hc64_sub(X, Lambda);
}

/* ============================================================================
 * 右移
 * ============================================================================ */

hc64_t hc64_shift_right(const hc64_t* SGN_RESTRICT a, uint8_t shift) {
    if (!a) return SGN_HC64_ZERO;
    if (shift == 0) return *a;
    if (shift >= 64) return SGN_HC64_ZERO;
    hc64_t c;
    c.v[0] = a->v[0] >> shift;
    c.v[1] = (a->v[1] >> shift) | (a->v[0] << (64 - shift));
    return c;
}

/* ============================================================================
 * 物理值转换
 *
 * to_double: v[0] + v[1] / 2^64
 *   注意 v[0] 可能超过 2^53，转换时会损失低位精度。
 *
 * from_double: 拆分为两个 32 位部分以避免精度丢失。
 *   double 精度 53 位，拆成 hi（高 21 位）+ lo（低 32 位）分别写入。
 * ============================================================================ */

double hc64_to_double(hc64_t h) {
    return hc_physical_value(&h, SGN_HC_KIND_64);
}

hc64_t hc64_from_double(double v, overflow_t policy) {
    hc64_t h;
    memset(&h, 0, sizeof(h));
    /* 使用通用表驱动转换 */
    hc_from_double(v, policy, &h, SGN_HC_KIND_64);
    return h;
}

/* ============================================================================
 * 校验和
 * ============================================================================ */

uint8_t hc64_checksum(const hc64_t* hc) {
    if (!hc) return 0;
    /* 将 64 位值拆为两个 32 位部分加权 */
    uint32_t lo0 = (uint32_t)(hc->v[0] & 0xFFFFFFFF);
    uint32_t hi0 = (uint32_t)(hc->v[0] >> 32);
    uint32_t lo1 = (uint32_t)(hc->v[1] & 0xFFFFFFFF);
    uint32_t hi1 = (uint32_t)(hc->v[1] >> 32);
    uint32_t sum = 2 * lo0 + 3 * hi0 + 4 * lo1 + 5 * hi1;
    return (uint8_t)(sum & 0xFF);
}
