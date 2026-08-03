// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file dc.c
 * @brief SGN DC（十进制定点数）运算实现
 * @version 2.0.0
 *
 * 从原 hpdc_core.cpp 迁移 DC 运算部分。
 *
 * 依赖：dc.h, hc.h
 */

#include "hc/dc.h"
#include "hc/hc.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * 内部辅助
 * ============================================================================ */

static int64_t dc_pow10(uint32_t exp) {
    if (exp > 18) exp = 18;
    int64_t r = 1;
    for (uint32_t i = 0; i < exp; ++i) r *= 10;
    return r;
}

static void dc_align(const dc_t* a, const dc_t* b,
                     int64_t* a_idx, int64_t* b_idx, uint32_t* out_level) {
    if (!a || !b || !a_idx || !b_idx || !out_level) return;
    if (a->level >= b->level) {
        uint32_t delta = a->level - b->level;
        *a_idx = a->index;
        *b_idx = b->index * dc_pow10(delta);
        *out_level = a->level;
    } else {
        uint32_t delta = b->level - a->level;
        *a_idx = a->index * dc_pow10(delta);
        *b_idx = b->index;
        *out_level = b->level;
    }
}

/* ============================================================================
 * 比较
 * ============================================================================ */

bool dc_less(const dc_t* a, const dc_t* b) {
    if (!a || !b) return false;
    int64_t ai, bi; uint32_t lvl;
    dc_align(a, b, &ai, &bi, &lvl);
    return ai < bi;
}

bool dc_equal(const dc_t* a, const dc_t* b) {
    if (!a || !b) return false;
    int64_t ai, bi; uint32_t lvl;
    dc_align(a, b, &ai, &bi, &lvl);
    return ai == bi;
}

/* ============================================================================
 * 跨精度转换
 * ============================================================================ */

dc_t dc_to_level(const dc_t* a, uint32_t target_level) {
    if (!a) { dc_t c = {0, 0}; return c; }
    dc_t c = *a;
    if (target_level > a->level) {
        c.index *= dc_pow10(target_level - a->level);
        c.level = target_level;
    } else if (target_level < a->level) {
        c.index /= dc_pow10(a->level - target_level);
        c.level = target_level;
    }
    return c;
}

/* ============================================================================
 * 算术
 * ============================================================================ */

dc_t dc_add(const dc_t* a, const dc_t* b) {
    if (!a || !b) { dc_t c = {0, 0}; return c; }
    int64_t ai, bi; uint32_t lvl;
    dc_align(a, b, &ai, &bi, &lvl);
    dc_t c;
    c.index = ai + bi;
    c.level = lvl;
    return c;
}

dc_t dc_sub(const dc_t* a, const dc_t* b) {
    if (!a || !b) { dc_t c = {0, 0}; return c; }
    int64_t ai, bi; uint32_t lvl;
    dc_align(a, b, &ai, &bi, &lvl);
    dc_t c;
    c.index = ai - bi;
    c.level = lvl;
    return c;
}

dc_t dc_mul(const dc_t* a, const dc_t* b) {
    if (!a || !b) { dc_t c = {0, 0}; return c; }
    dc_t c;
    int64_t result = a->index * b->index;
    if (a->index != 0 && result / a->index != b->index) {
        c.index = (a->index > 0) == (b->index > 0) ? INT64_MAX : INT64_MIN;
    } else {
        c.index = result;
    }
    c.level = a->level + b->level;
    return c;
}

/* ============================================================================
 * 与 double 互转
 * ============================================================================ */

double dc_to_double(const dc_t* a) {
    if (!a) return 0.0;
    return (double)a->index / pow(10.0, (double)a->level);
}

dc_t dc_from_double(double v, uint32_t level) {
    dc_t c;
    double scale = pow(10.0, (double)level);
    c.index = (int64_t)round(v * scale);
    c.level = level;
    return c;
}

/* ============================================================================
 * 与 HC8 互转
 * ============================================================================ */

hc8_t dc_to_hc8(const dc_t* a, precision_t prec) {
    (void)prec;
    if (!a) return SGN_HC8_ZERO;
    double v = dc_to_double(a);
    hc8_t h;
    memset(&h, 0, sizeof(h));
    hc_from_double(v, SGN_OVERFLOW_SATURATE, &h, SGN_HC_KIND_8);
    return h;
}

dc_t hc8_to_dc(const uint8_t* hc8_v, uint32_t level) {
    if (!hc8_v) { dc_t c = {0, 0}; return c; }
    /* 手动计算 HC8 物理值 */
    double v = 0.0;
    double scale = 1.0;
    for (int i = 0; i < 6; ++i) {
        v += hc8_v[i] * scale;
        scale /= 256.0;
    }
    return dc_from_double(v, level);
}

/* ============================================================================
 * JSON 序列化
 * ============================================================================ */

uint32_t dc_serialize(const dc_t* a, char* out_buf, uint32_t buf_size) {
    if (!a || !out_buf || buf_size == 0) return 0;
    int n = snprintf(out_buf, buf_size, "{\"index\":%lld,\"level\":%u}",
                     (long long)a->index, a->level);
    return (n > 0 && (uint32_t)n < buf_size) ? (uint32_t)n : 0;
}

int dc_deserialize(const char* json, dc_t* out) {
    if (!json || !out) return -1;
    long long idx = 0;
    unsigned int lvl = 0;
    int n = sscanf(json, "{\"index\":%lld,\"level\":%u}", &idx, &lvl);
    if (n != 2) {
        n = sscanf(json, "{index:%lld,level:%u}", &idx, &lvl);
        if (n != 2) return -1;
    }
    out->index = (int64_t)idx;
    out->level = (uint32_t)lvl;
    return 0;
}
