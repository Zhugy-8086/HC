// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc32.c
 * @brief SGN HC32 全部运算实现
 * @version 2.0.0
 *
 * HC32: 3层 × 32位，基数 2^32。
 * 所有函数为新增实现（原仅在 sgn_abi_dynamic.cpp 中有最小占位）。
 *
 * 依赖：hc32.h, hc.h
 */

#include "hc/hc32.h"
#include "hc/hc.h"
#include <string.h>

/* ============================================================================
 * 常量
 * ============================================================================ */

hc32_t SGN_HC32_ZERO = {{0, 0, 0}};
hc32_t SGN_HC32_MAX  = {{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}};

/* ============================================================================
 * 比较
 * ============================================================================ */

bool hc32_less(const hc32_t* SGN_RESTRICT a, const hc32_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    /* 多字节层：不能直接用 memcmp（小端序下字节序与数值序不一致） */
    for (int i = 0; i < 3; ++i) {
        if (a->v[i] != b->v[i]) return a->v[i] < b->v[i];
    }
    return false;
}

bool hc32_equal(const hc32_t* SGN_RESTRICT a, const hc32_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    return memcmp(a, b, sizeof(hc32_t)) == 0;
}

/* ============================================================================
 * 加法
 * ============================================================================ */

hc32_t hc32_add_sat(const hc32_t* SGN_RESTRICT a, const hc32_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC32_ZERO;
    hc32_t c;
    uint64_t carry = 0;
    for (int i = 2; i >= 0; --i) {
        uint64_t s = (uint64_t)a->v[i] + (uint64_t)b->v[i] + carry;
        c.v[i] = (uint32_t)(s & 0xFFFFFFFF);
        carry = s >> 32;
    }
    if (carry) c = SGN_HC32_MAX;
    return c;
}

hc32_t hc32_add_wrap(const hc32_t* SGN_RESTRICT a, const hc32_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC32_ZERO;
    hc32_t c;
    uint64_t carry = 0;
    for (int i = 2; i >= 0; --i) {
        uint64_t s = (uint64_t)a->v[i] + (uint64_t)b->v[i] + carry;
        c.v[i] = (uint32_t)(s & 0xFFFFFFFF);
        carry = s >> 32;
    }
    return c;
}

/* ============================================================================
 * 减法
 * ============================================================================ */

hc32_t hc32_sub(const hc32_t* SGN_RESTRICT a, const hc32_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC32_ZERO;
    hc32_t c;
    int borrow = 0;
    for (int i = 2; i >= 0; --i) {
        int64_t diff = (int64_t)a->v[i] - (int64_t)b->v[i] - borrow;
        if (diff < 0) { diff += 4294967296LL; borrow = 1; }
        else borrow = 0;
        c.v[i] = (uint32_t)diff;
    }
    if (borrow) c = SGN_HC32_ZERO;
    return c;
}

/* ============================================================================
 * 软阈值
 * ============================================================================ */

hc32_t hc32_soft_threshold(const hc32_t* SGN_RESTRICT X, const hc32_t* SGN_RESTRICT Lambda) {
    if (!X || !Lambda) return SGN_HC32_ZERO;
    if (hc32_less(X, Lambda) || hc32_equal(X, Lambda))
        return SGN_HC32_ZERO;
    return hc32_sub(X, Lambda);
}

/* ============================================================================
 * 右移
 * ============================================================================ */

hc32_t hc32_shift_right(const hc32_t* SGN_RESTRICT a, uint8_t shift) {
    if (!a) return SGN_HC32_ZERO;
    if (shift == 0) return *a;
    if (shift >= 32) return SGN_HC32_ZERO;
    hc32_t c;
    uint64_t borrow = 0;
    for (int i = 0; i < 3; ++i) {
        uint64_t val = ((uint64_t)a->v[i] >> shift) | (borrow << (32 - shift));
        c.v[i] = (uint32_t)val;
        borrow = a->v[i] & ((1U << shift) - 1);
    }
    return c;
}

/* ============================================================================
 * 物理值转换
 * ============================================================================ */

double hc32_to_double(hc32_t h) {
    return hc_physical_value(&h, SGN_HC_KIND_32);
}

hc32_t hc32_from_double(double v, overflow_t policy) {
    hc32_t h;
    memset(&h, 0, sizeof(h));
    hc_from_double(v, policy, &h, SGN_HC_KIND_32);
    return h;
}

/* ============================================================================
 * 校验和
 * ============================================================================ */

uint8_t hc32_checksum(const hc32_t* hc) {
    if (!hc) return 0;
    uint32_t sum = 0;
    for (int i = 0; i < 3; ++i) {
        sum += (uint32_t)((i + 2) * hc->v[i]);
    }
    return (uint8_t)(sum & 0xFF);
}
