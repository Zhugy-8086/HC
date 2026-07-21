// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hc_simd.h
 * @brief SGN SIMD 批量运算接口
 * @version 2.0.0
 *
 * 提供 HC16/HC64 的 SIMD 加速批量操作。
 * 编译时需定义 SGN_USE_SIMD 并链接 hc_simd.c。
 * 未定义 SGN_USE_SIMD 时，所有函数自动退化为标量循环。
 *
 * 支持平台：
 *   - x86/x64: SSE2 (默认), AVX2
 *
 * 依赖：hc16.h, hc64.h
 */

#ifndef SGN_HC_SIMD_H
#define SGN_HC_SIMD_H

#include "hc/hc8.h"
#include "hc/hc16.h"
#include "hc/hc32.h"
#include "hc/hc64.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * HC8 SIMD 批量操作
 * ============================================================================ */

/**
 * 批量 HC8 饱和加法：out[i] = saturate(a[i] + b[i])
 */
void hc8_add_sat_batch(const hc8_t* a, const hc8_t* b,
                                  hc8_t* out, uint32_t n);

/**
 * 批量 HC8 比较：out[i] = (a[i] < b[i]) ? 1 : 0
 */
void hc8_less_batch(const hc8_t* a, const hc8_t* b,
                         uint8_t* out, uint32_t n);

/**
 * 批量 HC8 软阈值：out[i] = max(a[i] - Lambda, 0)
 */
void hc8_soft_threshold_batch(const hc8_t* a, const hc8_t* Lambda,
                                     hc8_t* out, uint32_t n);

/* ============================================================================
 * HC16 SIMD 批量操作
 * ============================================================================ */

/**
 * 批量 HC16 饱和加法：out[i] = saturate(a[i] + b[i])
 * 利用 SIMD 并行检测进位掩码，逐层传播。
 */
void hc16_add_sat_batch(const hc16_t* a, const hc16_t* b,
                                   hc16_t* out, uint32_t n);

/**
 * 批量 HC16 比较：out[i] = (a[i] < b[i]) ? 1 : 0
 * 字典序比较，SIMD 可高效处理 16 位无符号比较。
 */
void hc16_less_batch(const hc16_t* a, const hc16_t* b,
                          uint8_t* out, uint32_t n);

/**
 * 批量 HC16 标量乘法（定点缩放）：out[i] = a[i] * factor_q16 >> 16
 * factor_q16 = factor * 65536，避免浮点。
 */
void hc16_scale_batch(const hc16_t* a, uint32_t factor_q16,
                           hc16_t* out, uint32_t n);

/**
 * 批量 HC16 软阈值：out[i] = max(a[i] - Lambda, 0)
 */
void hc16_soft_threshold_batch(const hc16_t* a, const hc16_t* Lambda,
                                    hc16_t* out, uint32_t n);

/* ============================================================================
 * HC64 SIMD 批量操作
 * ============================================================================ */

/**
 * 批量 HC64 饱和加法：out[i] = saturate(a[i] + b[i])
 * HC64 每个元素 16 字节，恰好占满一个 SSE 寄存器。
 */
void hc64_add_sat_batch(const hc64_t* a, const hc64_t* b,
                                   hc64_t* out, uint32_t n);

/**
 * 批量 HC64 比较：out[i] = (a[i] < b[i]) ? 1 : 0
 */
void hc64_less_batch(const hc64_t* a, const hc64_t* b,
                         uint8_t* out, uint32_t n);

/* ============================================================================
 * HC32 SIMD 批量操作
 * ============================================================================ */

/**
 * 批量 HC32 饱和加法：out[i] = saturate(a[i] + b[i])
 */
void hc32_add_sat_batch(const hc32_t* a, const hc32_t* b,
                                   hc32_t* out, uint32_t n);

/**
 * 批量 HC32 比较：out[i] = (a[i] < b[i]) ? 1 : 0
 */
void hc32_less_batch(const hc32_t* a, const hc32_t* b,
                         uint8_t* out, uint32_t n);

/**
 * 批量 HC32 软阈值：out[i] = max(a[i] - Lambda, 0)
 */
void hc32_soft_threshold_batch(const hc32_t* a, const hc32_t* Lambda,
                                    hc32_t* out, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* SGN_HC_SIMD_H */
