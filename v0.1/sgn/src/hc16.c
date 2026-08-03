// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc16.c
 * @brief SGN HC16 全部运算实现
 * @version 2.0.0
 *
 * HC16: 4层 × 16位，基数 65536。
 * 包含从 hpdc_core.cpp 中的 less/equal/to_double，
 * 以及原缺失的 add_saturated/add_wrap/sub/soft_threshold/shift_right/checksum。
 *
 * 依赖：hc16.h, hc.h
 */

#include "hc/hc16.h"
#include "hc/hc.h"
#include <string.h>

/* ============================================================================
 * 常量
 * ============================================================================ */

hc16_t SGN_HC16_ZERO = {{0, 0, 0, 0}};
hc16_t SGN_HC16_MAX  = {{65535, 65535, 65535, 65535}};

/* ============================================================================
 * 比较（字典序，从最高层 frac[0] 开始）
 * ============================================================================ */

bool hc16_less(const hc16_t* SGN_RESTRICT a, const hc16_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    /* 多字节层：不能直接用 memcmp（小端序下字节序与数值序不一致） */
    for (int i = 0; i < 4; ++i) {
        if (a->v[i] != b->v[i]) return a->v[i] < b->v[i];
    }
    return false;
}

bool hc16_equal(const hc16_t* SGN_RESTRICT a, const hc16_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    return memcmp(a, b, sizeof(hc16_t)) == 0;
}

/* ============================================================================
 * 加法（饱和 / 回绕）
 * 从最低层 v[3] 开始，逐层向上传播进位
 * ============================================================================ */

hc16_t hc16_add_sat(const hc16_t* SGN_RESTRICT a, const hc16_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC16_ZERO;
    hc16_t c;
    uint32_t carry = 0;
    for (int i = 3; i >= 0; --i) {
        uint32_t s = (uint32_t)a->v[i] + (uint32_t)b->v[i] + carry;
        c.v[i] = (uint16_t)(s & 0xFFFF);
        carry = s >> 16;
    }
    if (carry) c = SGN_HC16_MAX;
    return c;
}

hc16_t hc16_add_wrap(const hc16_t* SGN_RESTRICT a, const hc16_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC16_ZERO;
    hc16_t c;
    uint32_t carry = 0;
    for (int i = 3; i >= 0; --i) {
        uint32_t s = (uint32_t)a->v[i] + (uint32_t)b->v[i] + carry;
        c.v[i] = (uint16_t)(s & 0xFFFF);
        carry = s >> 16;
    }
    return c;
}

/* ============================================================================
 * 减法（借位，饱和到 0）
 * ============================================================================ */

hc16_t hc16_sub(const hc16_t* SGN_RESTRICT a, const hc16_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC16_ZERO;
    hc16_t c;
    int borrow = 0;
    for (int i = 3; i >= 0; --i) {
        int32_t diff = (int32_t)a->v[i] - (int32_t)b->v[i] - borrow;
        if (diff < 0) { diff += 65536; borrow = 1; }
        else borrow = 0;
        c.v[i] = (uint16_t)diff;
    }
    if (borrow) c = SGN_HC16_ZERO;
    return c;
}

/* ============================================================================
 * 软阈值：max(X - Lambda, 0)
 * ============================================================================ */

hc16_t hc16_soft_threshold(const hc16_t* SGN_RESTRICT X, const hc16_t* SGN_RESTRICT Lambda) {
    if (!X || !Lambda) return SGN_HC16_ZERO;
    if (hc16_less(X, Lambda) || hc16_equal(X, Lambda))
        return SGN_HC16_ZERO;
    return hc16_sub(X, Lambda);
}

/* ============================================================================
 * 右移（逐层，跨层借位）
 * ============================================================================ */

hc16_t hc16_shift_right(const hc16_t* SGN_RESTRICT a, uint8_t shift) {
    if (!a) return SGN_HC16_ZERO;
    if (shift == 0) return *a;
    if (shift >= 16) return SGN_HC16_ZERO;
    hc16_t c;
    uint32_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        uint32_t val = (a->v[i] >> shift) | (borrow << (16 - shift));
        c.v[i] = (uint16_t)val;
        borrow = a->v[i] & ((1U << shift) - 1);
    }
    return c;
}

/* ============================================================================
 * 物理值转换
 * ============================================================================ */

double hc16_to_double(hc16_t h) {
    return hc_physical_value(&h, SGN_HC_KIND_16);
}

hc16_t hc16_from_double(double v, overflow_t policy) {
    hc16_t h;
    memset(&h, 0, sizeof(h));
    hc_from_double(v, policy, &h, SGN_HC_KIND_16);
    return h;
}

/* ============================================================================
 * 校验和
 * ============================================================================ */

uint8_t hc16_checksum(const hc16_t* hc) {
    if (!hc) return 0;
    uint32_t sum = 0;
    for (int i = 0; i < 4; ++i) {
        sum += (uint32_t)((i + 2) * hc->v[i]);
    }
    return (uint8_t)(sum & 0xFF);
}
