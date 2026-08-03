// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc.c
 * @brief SGN 通用基础设施实现 - 元数据表、错误码、版本、通用转换
 * @version 2.0.0
 *
 * 本文件包含所有 HC 变体共享的表驱动基础设施。
 * 合并自原 hpdc_core.cpp（常量初始化、错误码）和 sgn_abi_dynamic.cpp（元数据表）。
 *
 * 依赖：hc.h
 */

#include "hc/hc.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * HC 元数据表（编译期常量，表驱动核心）
 * ============================================================================ */

static const hc_meta_t HC_META[SGN_HC_KIND_COUNT] = {
    /* [SGN_HC_KIND_8]  */ { 6, 8,  1, 256.0,               255.9999999999999,     6,  "HC8"  },
    /* [SGN_HC_KIND_16] */ { 4, 16, 2, 65536.0,              65535.9999999999999,   8,  "HC16" },
    /* [SGN_HC_KIND_32] */ { 3, 32, 4, 4294967296.0,         4294967295.9999995,    12, "HC32" },
    /* [SGN_HC_KIND_64] */ { 2, 64, 8, 18446744073709551616.0, 18446744073709551615.9999999, 16, "HC64" },
};

const hc_meta_t* hc_meta_get(hc_kind_t kind) {
    if (kind < 0 || kind >= SGN_HC_KIND_COUNT) return NULL;
    return &HC_META[kind];
}

/* ============================================================================
 * 精度等级映射
 * ============================================================================ */

uint32_t precision_layers(precision_t prec) {
    switch (prec) {
        case SGN_PREC_ARCHIVE: return 0;
        case SGN_PREC_FAST:    return 2;
        case SGN_PREC_STD:     return 4;
        case SGN_PREC_PREC:    return 6;
        case SGN_PREC_FUTURE:  return 8;
        default:               return 4;
    }
}

hc_kind_t precision_to_hc_kind(precision_t prec) {
    uint32_t layers = precision_layers(prec);
    if (layers <= 2) return SGN_HC_KIND_64;
    if (layers <= 3) return SGN_HC_KIND_32;
    if (layers <= 4) return SGN_HC_KIND_16;
    return SGN_HC_KIND_8;
}

/* ============================================================================
 * 错误码字符串
 * ============================================================================ */

const char* error_string(error_t err) {
    switch (err) {
        case SGN_OK:                   return "OK";
        case SGN_ERR_NOMEM:            return "Out of memory";
        case SGN_ERR_OUT_OF_RANGE:     return "Out of range";
        case SGN_ERR_INVALID_ARG:      return "Invalid argument";
        case SGN_ERR_TMR_FAULT:        return "TMR single-bit fault";
        case SGN_ERR_TMR_MULTI_FAULT:  return "TMR multi-bit fault";
        case SGN_ERR_RS_UNCORRECTABLE: return "RS uncorrectable";
        case SGN_ERR_FILE_CORRUPT:     return "File corrupt";
        case SGN_ERR_VERSION_MISMATCH: return "Version mismatch";
        default:                       return "Unknown error";
    }
}

/* ============================================================================
 * ABI 版本查询
 * ============================================================================ */

uint32_t abi_version(void) {
    return (SGN_ABI_MAJOR << 16) | (SGN_ABI_MINOR << 8) | SGN_ABI_PATCH;
}

/* ============================================================================
 * 通用物理值计算（修复了 HC16 scale bug）
 *
 * 旧 bug: hc16_physical_value() 起始 scale = 1.0/65536.0（错误！）
 * 正确:   scale 起始为 1.0，每层除以 base
 * ============================================================================ */

double hc_physical_value(const void* hc_raw, hc_kind_t kind) {
    const hc_meta_t* m = hc_meta_get(kind);
    if (!m || !hc_raw) return 0.0;

    const uint8_t* p = (const uint8_t*)hc_raw;
    double v = 0.0;
    double scale = 1.0;
    for (uint32_t i = 0; i < m->layers; ++i) {
        uint64_t elem = 0;
        const uint8_t* base = p + i * m->elem_bytes;
        for (uint32_t j = 0; j < m->elem_bytes; ++j) {
            elem |= (uint64_t)base[j] << (j * 8);
        }
        v += (double)elem * scale;
        scale /= m->base;
    }
    return v;
}

/* ============================================================================
 * 通用 double→HC 转换
 * ============================================================================ */

void hc_from_double(double v, overflow_t policy,
                      void* out_hc, hc_kind_t kind) {
    const hc_meta_t* m = hc_meta_get(kind);
    if (!m || !out_hc) return;

    if (v < 0.0) {
        if (policy == SGN_OVERFLOW_WRAP) {
            v = fmod(v, m->base);
            if (v < 0.0) v += m->base;
        } else {
            v = 0.0;
        }
    }
    if (v >= m->base) {
        if (policy == SGN_OVERFLOW_WRAP) {
            v = fmod(v, m->base);
        } else {
            v = m->max_phys;
        }
    }

    uint8_t* p = (uint8_t*)out_hc;
    double scaled = v;
    for (uint32_t i = 0; i < m->layers; ++i) {
        uint64_t elem = (uint64_t)scaled;
        uint8_t* dst = p + i * m->elem_bytes;
        for (uint32_t j = 0; j < m->elem_bytes; ++j) {
            dst[j] = (uint8_t)(elem >> (j * 8));
        }
        scaled = (scaled - (double)elem) * m->base;
    }
}

/* ============================================================================
 * 沙盒联动辅助
 * ============================================================================ */

uint32_t sandbox_default_int_bits(hc_kind_t kind) {
    const hc_meta_t* m = hc_meta_get(kind);
    return m ? m->bits : 8;
}

bool sandbox_int_bits_valid(hc_kind_t kind, uint32_t int_bits) {
    const hc_meta_t* m = hc_meta_get(kind);
    if (!m) return false;
    return int_bits <= m->bits;
}

uint32_t trie_depth_for_hc(hc_kind_t kind) {
    const hc_meta_t* m = hc_meta_get(kind);
    return m ? m->layers : 6;
}
