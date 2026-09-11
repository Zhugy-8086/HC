// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc8.c
 * @brief SGN HC8 全部运算实现
 * @version 2.0.0
 *
 * 从原 hpdc_core.cpp 迁移，逻辑不变。
 * 依赖：hc8.h
 */

#include "hc/hc8.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * 常量
 * ============================================================================ */

static hc8_t make_hc8(uint8_t v0, uint8_t v1, uint8_t v2,
                           uint8_t v3, uint8_t v4, uint8_t v5) {
    hc8_t h;
    h.v[0] = v0; h.v[1] = v1; h.v[2] = v2;
    h.v[3] = v3; h.v[4] = v4; h.v[5] = v5;
    return h;
}

hc8_t SGN_HC8_ZERO = {{0,0,0,0,0,0}};
hc8_t SGN_HC8_MAX  = {{255,255,255,255,255,255}};

/* ============================================================================
 * 比较
 * ============================================================================ */

bool hc8_less(const hc8_t* SGN_RESTRICT a, const hc8_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    /* HC8 为单字节数组，字节序即字典序，可直接用 memcmp */
    return memcmp(a, b, sizeof(hc8_t)) < 0;
}

bool hc8_equal(const hc8_t* SGN_RESTRICT a, const hc8_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    return memcmp(a, b, sizeof(hc8_t)) == 0;
}

bool shc8_less(const shc8_t* SGN_RESTRICT a, const shc8_t* SGN_RESTRICT b) {
    if (!a || !b) return false;
    if (a->sign != b->sign) return a->sign != 0;
    if (a->int_part != b->int_part) return a->int_part < b->int_part;
    return memcmp(&a->frac, &b->frac, sizeof(hc8_t)) < 0;
}

/* ============================================================================
 * 加法
 * ============================================================================ */

hc8_t hc8_add_sat(const hc8_t* SGN_RESTRICT a, const hc8_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC8_ZERO;
    hc8_t c;
    uint16_t carry = 0;
    for (int i = 5; i >= 0; --i) {
        uint16_t s = (uint16_t)a->v[i] + (uint16_t)b->v[i] + carry;
        c.v[i] = (uint8_t)(s & 0xFF);
        carry = s >> 8;
    }
    if (carry) c = SGN_HC8_MAX;
    return c;
}

hc8_t hc8_add_wrap(const hc8_t* SGN_RESTRICT a, const hc8_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC8_ZERO;
    hc8_t c;
    uint16_t carry = 0;
    for (int i = 5; i >= 0; --i) {
        uint16_t s = (uint16_t)a->v[i] + (uint16_t)b->v[i] + carry;
        c.v[i] = (uint8_t)(s & 0xFF);
        carry = s >> 8;
    }
    return c;
}

leveled_hpdc8_t leveled_add(const leveled_hpdc8_t* SGN_RESTRICT a,
                                     const leveled_hpdc8_t* SGN_RESTRICT b) {
    leveled_hpdc8_t c;
    if (!a || !b) { memset(&c, 0, sizeof(c)); return c; }
    c.level = a->level + b->level;
    uint16_t carry = 0;
    for (int i = 5; i >= 0; --i) {
        uint16_t s = (uint16_t)a->frac.v[i] + (uint16_t)b->frac.v[i] + carry;
        c.frac.v[i] = (uint8_t)(s & 0xFF);
        carry = s >> 8;
    }
    uint16_t int_sum = (uint16_t)a->int_part + (uint16_t)b->int_part + carry;
    c.int_part = (uint8_t)(int_sum & 0xFF);
    c.level += (int_sum >> 8);
    return c;
}

shc8_t shc8_add(const shc8_t* SGN_RESTRICT a, const shc8_t* SGN_RESTRICT b) {
    shc8_t zero = {0, 0, {{0,0,0,0,0,0}}};
    if (!a || !b) return zero;
    if (a->sign == b->sign) {
        hc8_t mag_a, mag_b, sum;
        mag_a.v[0] = a->int_part;
        memcpy(&mag_a.v[1], &a->frac, 5);
        mag_a.v[5] = a->frac.v[5];
        mag_b.v[0] = b->int_part;
        memcpy(&mag_b.v[1], &b->frac, 5);
        mag_b.v[5] = b->frac.v[5];
        sum = hc8_add_sat(&mag_a, &mag_b);
        shc8_t c;
        c.sign = a->sign;
        c.int_part = sum.v[0];
        memcpy(&c.frac, &sum.v[1], 5);
        c.frac.v[5] = sum.v[5];
        return c;
    }
    hc8_t mag_a, mag_b;
    mag_a.v[0] = a->int_part;
    memcpy(&mag_a.v[1], &a->frac, 5);
    mag_a.v[5] = a->frac.v[5];
    mag_b.v[0] = b->int_part;
    memcpy(&mag_b.v[1], &b->frac, 5);
    mag_b.v[5] = b->frac.v[5];
    if (hc8_equal(&mag_a, &mag_b)) return zero;
    shc8_t c;
    if (hc8_less(&mag_a, &mag_b)) {
        hc8_t diff = hc8_sub(&mag_b, &mag_a);
        c.sign = b->sign;
        c.int_part = diff.v[0];
        memcpy(&c.frac, &diff.v[1], 5);
        c.frac.v[5] = diff.v[5];
    } else {
        hc8_t diff = hc8_sub(&mag_a, &mag_b);
        c.sign = a->sign;
        c.int_part = diff.v[0];
        memcpy(&c.frac, &diff.v[1], 5);
        c.frac.v[5] = diff.v[5];
    }
    return c;
}

/* ============================================================================
 * 减法
 * ============================================================================ */

hc8_t hc8_sub(const hc8_t* SGN_RESTRICT a, const hc8_t* SGN_RESTRICT b) {
    if (!a || !b) return SGN_HC8_ZERO;
    hc8_t c;
    int borrow = 0;
    for (int i = 5; i >= 0; --i) {
        int diff = (int)a->v[i] - (int)b->v[i] - borrow;
        if (diff < 0) { diff += 256; borrow = 1; }
        else borrow = 0;
        c.v[i] = (uint8_t)diff;
    }
    if (borrow) c = SGN_HC8_ZERO;
    return c;
}

shc8_t shc8_sub(const shc8_t* SGN_RESTRICT a, const shc8_t* SGN_RESTRICT b) {
    shc8_t zero = {0, 0, {{0,0,0,0,0,0}}};
    if (!a || !b) return zero;
    shc8_t neg_b;
    neg_b.sign = b->sign ? 0 : 1;
    neg_b.int_part = b->int_part;
    neg_b.frac = b->frac;
    return shc8_add(a, &neg_b);
}

/* ============================================================================
 * 软阈值
 * ============================================================================ */

hc8_t hc8_soft_threshold(const hc8_t* SGN_RESTRICT X, const hc8_t* SGN_RESTRICT Lambda) {
    if (!X || !Lambda) return SGN_HC8_ZERO;
    if (hc8_less(X, Lambda) || hc8_equal(X, Lambda))
        return SGN_HC8_ZERO;
    return hc8_sub(X, Lambda);
}

shc8_t shc8_soft_threshold(const shc8_t* SGN_RESTRICT X, const shc8_t* SGN_RESTRICT Lambda) {
    shc8_t zero;
    zero.sign = 0; zero.int_part = 0;
    memset(&zero.frac, 0, sizeof(zero.frac));
    if (!X || !Lambda) return zero;
    shc8_t abs_X = *X; abs_X.sign = 0;
    shc8_t abs_L = *Lambda; abs_L.sign = 0;
    if (shc8_less(&abs_X, &abs_L)) {
        shc8_t zero;
        zero.sign = 0; zero.int_part = 0;
        memset(&zero.frac, 0, sizeof(zero.frac));
        return zero;
    }
    shc8_t result = shc8_sub(&abs_X, &abs_L);
    result.sign = X->sign;
    return result;
}

/* ============================================================================
 * 移位与掩码
 * ============================================================================ */

hc8_t hc8_shift_right(const hc8_t* SGN_RESTRICT a, uint8_t shift) {
    if (!a) return SGN_HC8_ZERO;
    if (shift == 0) return *a;
    if (shift >= 8) return SGN_HC8_ZERO;
    hc8_t c;
    uint16_t borrow = 0;
    for (int i = 0; i < 6; ++i) {
        uint16_t val = (a->v[i] >> shift) | (borrow << (8 - shift));
        c.v[i] = (uint8_t)val;
        borrow = a->v[i] & ((1U << shift) - 1);
    }
    return c;
}

hc8_t hc8_mask_threshold(const hc8_t* SGN_RESTRICT X, uint8_t mask) {
    if (!X) return SGN_HC8_ZERO;
    hc8_t c;
    for (int i = 0; i < 6; ++i) c.v[i] = X->v[i] & ~mask;
    return c;
}

/* ============================================================================
 * 有符号与无符号互转
 * ============================================================================ */

shc8_t hc8_to_shc8(hc8_t x) {
    shc8_t s;
    s.sign = 0;
    s.int_part = x.v[0];
    memcpy(&s.frac, &x.v[1], 5);
    s.frac.v[5] = 0;
    return s;
}

hc8_t shc8_to_hc8(shc8_t x, bool* ok) {
    if (x.sign) {
        if (ok) *ok = false;
        return SGN_HC8_ZERO;
    }
    if (ok) *ok = true;
    hc8_t h;
    h.v[0] = x.int_part;
    memcpy(&h.v[1], &x.frac, 5);
    return h;
}

/* ============================================================================
 * 物理值转换
 * ============================================================================ */

double hc8_to_double(hc8_t h) {
    return hc_physical_value(&h, SGN_HC_KIND_8);
}

hc8_t hc8_from_double(double v, overflow_t policy) {
    hc8_t h;
    memset(&h, 0, sizeof(h));
    hc_from_double(v, policy, &h, SGN_HC_KIND_8);
    return h;
}

/* ============================================================================
 * 校验和
 * ============================================================================ */

uint8_t hc8_checksum(const hc8_t* hc) {
    if (!hc) return 0;
    static const uint8_t W[6] = {2, 3, 4, 5, 6, 7};
    uint16_t sum = 0;
    for (int i = 0; i < 6; ++i) sum += W[i] * hc->v[i];
    return (uint8_t)(sum & 0xFF);
}
