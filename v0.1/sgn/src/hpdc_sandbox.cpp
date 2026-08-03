// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hpdc_sandbox.cpp
 * @brief HPDC 投影沙盒 ABI 实现
 * @version 2.0.0
 *
 * 所有物理值计算统一调用 hc.h / hc8.h / hc16.h / hc32.h 导出的函数，
 * 不再有内部重复实现。
 */

#include "hc/hpdc_sandbox.h"
#include "hc/hc.h"
#include "hc/hc8.h"
#include "hc/hc16.h"
#include "hc/hc32.h"
#include "hc/hc64.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

/* ============================================================================
 * 沙盒内部状态结构体
 * ============================================================================ */

struct sandbox {
    precision_t precision;
    hc_kind_t   hc_kind;      /* 关联的 HC 种类 */
    uint32_t        int_bits;
    double          R;            /* R = 2^int_bits */
    double          invR;         /* 1/R */
    char            scheme_name[32];
};

/* ============================================================================
 * 创建与销毁（增加 int_bits 校验）
 * ============================================================================ */

sandbox_t* sandbox_create(precision_t prec, uint32_t int_bits) {
    hc_kind_t kind = precision_to_hc_kind(prec);
    if (!sandbox_int_bits_valid(kind, int_bits)) return NULL;

    sandbox_t* sb = (sandbox_t*)malloc(sizeof(sandbox_t));
    if (!sb) return NULL;
    sb->precision = prec;
    sb->hc_kind = kind;
    sb->int_bits = int_bits;
    if (int_bits < 64) {
        sb->R = (double)(1ULL << int_bits);
    } else {
        sb->R = ldexp(1.0, (int)int_bits);
    }
    sb->invR = 1.0 / sb->R;
    strncpy(sb->scheme_name, "default", sizeof(sb->scheme_name) - 1);
    sb->scheme_name[sizeof(sb->scheme_name) - 1] = '\0';
    return sb;
}

sandbox_t* sandbox_create_auto(hc_kind_t kind) {
    const hc_meta_t* m = hc_meta_get(kind);
    if (!m) return NULL;
    precision_t prec = SGN_PREC_STD;
    /* 选择与 kind 匹配的精度等级 */
    switch (kind) {
        case SGN_HC_KIND_8:  prec = SGN_PREC_PREC; break;  /* 6 layers */
        case SGN_HC_KIND_16: prec = SGN_PREC_STD;  break;  /* 4 layers */
        case SGN_HC_KIND_32: prec = SGN_PREC_FAST; break;  /* 3 layers -> closest */
        case SGN_HC_KIND_64: prec = SGN_PREC_FAST; break;  /* 2 layers -> closest */
        default: break;
    }
    return sandbox_create(prec, m->bits);
}

void sandbox_destroy(sandbox_t* sb) {
    if (sb) free(sb);
}

/* ============================================================================
 * 投影（统一调用 sgn_hcX_to_double）
 * ============================================================================ */

double sandbox_project_hc8(sandbox_t* sb, hc8_t h) {
    if (!sb) return 0.0;
    return hc8_to_double(h) / sb->R;
}

double sandbox_project_hc16(sandbox_t* sb, hc16_t h) {
    if (!sb) return 0.0;
    return hc16_to_double(h) / sb->R;
}

#if SGN_PC_EXTENSION
double sandbox_project_hc32(sandbox_t* sb, hc32_t h) {
    if (!sb) return 0.0;
    return hc32_to_double(h) / sb->R;
}
#endif

/* ============================================================================
 * 逆投影（统一调用 sgn_double_to_hcX）
 * ============================================================================ */

hc8_t sandbox_inverse_hc8(sandbox_t* sb, double phi) {
    if (!sb) return SGN_HC8_ZERO;
    if (phi < 0.0) phi = 0.0;
    if (phi >= 1.0) phi = 0.9999999999999999;
    return hc8_from_double(phi * sb->R, SGN_OVERFLOW_SATURATE);
}

hc16_t sandbox_inverse_hc16(sandbox_t* sb, double phi) {
    if (!sb) return SGN_HC16_ZERO;
    if (phi < 0.0) phi = 0.0;
    if (phi >= 1.0) phi = 0.9999999999999999;
    return hc16_from_double(phi * sb->R, SGN_OVERFLOW_SATURATE);
}

#if SGN_PC_EXTENSION
hc32_t sandbox_inverse_hc32(sandbox_t* sb, double phi) {
    if (!sb) return SGN_HC32_ZERO;
    if (phi < 0.0) phi = 0.0;
    if (phi >= 1.0) phi = 0.9999999999999999;
    return hc32_from_double(phi * sb->R, SGN_OVERFLOW_SATURATE);
}
#endif

/* ============================================================================
 * 批量操作
 * ============================================================================ */

#if SGN_PC_EXTENSION

void sandbox_project_hc8_batch(sandbox_t* sb,
                                   const hc8_t* in, double* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_project_hc8(sb, in[i]);
}

void sandbox_project_hc16_batch(sandbox_t* sb,
                                    const hc16_t* in, double* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_project_hc16(sb, in[i]);
}

void sandbox_project_hc32_batch(sandbox_t* sb,
                                    const hc32_t* in, double* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_project_hc32(sb, in[i]);
}

void sandbox_inverse_hc8_batch(sandbox_t* sb,
                                   const double* in, hc8_t* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_inverse_hc8(sb, in[i]);
}

void sandbox_inverse_hc16_batch(sandbox_t* sb,
                                    const double* in, hc16_t* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_inverse_hc16(sb, in[i]);
}

void sandbox_inverse_hc32_batch(sandbox_t* sb,
                                    const double* in, hc32_t* out, uint32_t n) {
    if (!sb || !in || !out) return;
    for (uint32_t i = 0; i < n; ++i) out[i] = sandbox_inverse_hc32(sb, in[i]);
}

#endif /* SGN_PC_EXTENSION */

/* ============================================================================
 * 除法
 * ============================================================================ */

hc8_t sandbox_divide_hc8(sandbox_t* sb, hc8_t a, hc8_t b) {
    if (!sb) return SGN_HC8_ZERO;
    double pa = sandbox_project_hc8(sb, a);
    double pb = sandbox_project_hc8(sb, b);
    if (pb == 0.0) return SGN_HC8_MAX;
    return hc8_from_double((pa / pb) * sb->R, SGN_OVERFLOW_SATURATE);
}

hc16_t sandbox_divide_hc16(sandbox_t* sb, hc16_t a, hc16_t b) {
    if (!sb) return SGN_HC16_ZERO;
    double pa = sandbox_project_hc16(sb, a);
    double pb = sandbox_project_hc16(sb, b);
    if (pb == 0.0) return SGN_HC16_MAX;
    return hc16_from_double((pa / pb) * sb->R, SGN_OVERFLOW_SATURATE);
}

#if SGN_PC_EXTENSION
hc32_t sandbox_divide_hc32(sandbox_t* sb, hc32_t a, hc32_t b) {
    if (!sb) return SGN_HC32_ZERO;
    double pa = sandbox_project_hc32(sb, a);
    double pb = sandbox_project_hc32(sb, b);
    if (pb == 0.0) return SGN_HC32_MAX;
    return hc32_from_double((pa / pb) * sb->R, SGN_OVERFLOW_SATURATE);
}
#endif

hc64_t sandbox_divide_hc64(sandbox_t* sb, hc64_t a, hc64_t b) {
    if (!sb) return SGN_HC64_ZERO;
    double pa = hc64_to_double(a) / sb->R;
    double pb = hc64_to_double(b) / sb->R;
    if (pb == 0.0) return SGN_HC64_MAX;
    return hc64_from_double((pa / pb) * sb->R, SGN_OVERFLOW_SATURATE);
}

/* ============================================================================
 * 梯度更新
 * ============================================================================ */

hc8_t sandbox_gradient_hc8(sandbox_t* sb, hc8_t h, double grad, double eta) {
    if (!sb) return SGN_HC8_ZERO;
    double phi = sandbox_project_hc8(sb, h);
    return sandbox_inverse_hc8(sb, phi - eta * grad * sb->invR);
}

hc16_t sandbox_gradient_hc16(sandbox_t* sb, hc16_t h, double grad, double eta) {
    if (!sb) return SGN_HC16_ZERO;
    double phi = sandbox_project_hc16(sb, h);
    return sandbox_inverse_hc16(sb, phi - eta * grad * sb->invR);
}

#if SGN_PC_EXTENSION
hc32_t sandbox_gradient_hc32(sandbox_t* sb, hc32_t h, double grad, double eta) {
    if (!sb) return SGN_HC32_ZERO;
    double phi = sandbox_project_hc32(sb, h);
    return sandbox_inverse_hc32(sb, phi - eta * grad * sb->invR);
}
#endif

hc64_t sandbox_gradient_hc64(sandbox_t* sb, hc64_t h, double grad, double eta) {
    if (!sb) return SGN_HC64_ZERO;
    double phi = hc64_to_double(h) / sb->R;
    return hc64_from_double((phi - eta * grad * sb->invR) * sb->R, SGN_OVERFLOW_SATURATE);
}

/* ============================================================================
 * 缩放
 * ============================================================================ */

hc8_t sandbox_scale_hc8(sandbox_t* sb, hc8_t h, double factor) {
    if (!sb) return SGN_HC8_ZERO;
    return hc8_from_double(hc8_to_double(h) * factor, SGN_OVERFLOW_SATURATE);
}

hc16_t sandbox_scale_hc16(sandbox_t* sb, hc16_t h, double factor) {
    if (!sb) return SGN_HC16_ZERO;
    return hc16_from_double(hc16_to_double(h) * factor, SGN_OVERFLOW_SATURATE);
}

#if SGN_PC_EXTENSION
hc32_t sandbox_scale_hc32(sandbox_t* sb, hc32_t h, double factor) {
    if (!sb) return SGN_HC32_ZERO;
    return hc32_from_double(hc32_to_double(h) * factor, SGN_OVERFLOW_SATURATE);
}
#endif

hc64_t sandbox_scale_hc64(sandbox_t* sb, hc64_t h, double factor) {
    if (!sb) return SGN_HC64_ZERO;
    return hc64_from_double(hc64_to_double(h) * factor, SGN_OVERFLOW_SATURATE);
}

/* ============================================================================
 * 有符号运算
 * ============================================================================ */

shc8_t sandbox_signed_add(sandbox_t* sb, shc8_t a, shc8_t b) {
    (void)sb;
    return shc8_add(&a, &b);
}

shc8_t sandbox_signed_sub(sandbox_t* sb, shc8_t a, shc8_t b) {
    (void)sb;
    return shc8_sub(&a, &b);
}

/* ============================================================================
 * 投影方案切换
 * ============================================================================ */

int sandbox_set_scheme(sandbox_t* sb, const char* scheme_name) {
    if (!sb || !scheme_name) return -1;
    if (strcmp(scheme_name, "default") != 0) return -1;
    strncpy(sb->scheme_name, scheme_name, sizeof(sb->scheme_name)-1);
    sb->scheme_name[sizeof(sb->scheme_name)-1] = '\0';
    return 0;
}

const char* sandbox_get_scheme(const sandbox_t* sb) {
    if (!sb) return "default";
    return sb->scheme_name;
}
