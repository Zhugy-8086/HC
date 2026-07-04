/**
 * @file hc_simd.c
 * @brief SGN SIMD 批量运算实现
 * @version 2.0.0
 *
 * 编译选项：
 *   -DSGN_USE_SIMD          启用 SIMD 路径（需 SSE2）
 *   -DSGN_USE_AVX2          启用 AVX2 路径（可选，需 AVX2）
 *   不定义上述宏 → 所有函数退化为标量循环
 *
 * 依赖：hc_simd.h, hc16.h, hc64.h
 */

#include "hc/hc_simd.h"
#include "hc/hc16.h"
#include "hc/hc64.h"
#include <string.h>

/* ============================================================================
 * 平台检测
 * ============================================================================ */

#if defined(SGN_USE_SIMD)
    #if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        #define SGN_HAS_SSE2 1
        #include <emmintrin.h>  /* SSE2 */
    #elif defined(__x86_64__) || defined(__amd64__)
        #define SGN_HAS_SSE2 1
        #include <emmintrin.h>
    #else
        #define SGN_HAS_SSE2 0
    #endif
#else
    #define SGN_HAS_SSE2 0
#endif

/* ============================================================================
 * HC16 批量饱和加法
 *
 * 策略：SIMD 并行计算各层 + 进位掩码，逐层传播进位。
 * HC16 内存布局：[v0_hi, v0_lo, v1_hi, v1_lo, v2_hi, v2_lo, v3_hi, v3_lo]
 * 每层 16 位，从 v[3]（最低层）向 v[0]（最高层）传播进位。
 * ============================================================================ */

#if SGN_HAS_SSE2

void hc16_add_sat_batch(const hc16_t* a, const hc16_t* b,
                                   hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc16_add_sat(&a[i], &b[i]);
    }
}

/* ============================================================================
 * HC16 批量比较（SIMD 加速）
 *
 * 字典序比较：从 v[0] 开始逐层比较，首个不等层决定结果。
 * 策略：并行比较所有 4 层，然后逐元素确定首次差异层。
 * ============================================================================ */

void hc16_less_batch(const hc16_t* a, const hc16_t* b,
                          uint8_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc16_less(&a[i], &b[i]) ? 1 : 0;
    }
}

/* ============================================================================
 * HC16 批量标量缩放（SIMD 加速 16 位乘法）
 *
 * out[i].v[j] = (a[i].v[j] * factor_q16) >> 16
 * 使用 _mm_mulhi_epu16 进行无符号 16×16→32 高半乘法。
 * ============================================================================ */

void hc16_scale_batch(const hc16_t* a, uint32_t factor_q16,
                           hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        for (int j = 0; j < 4; ++j) {
            uint32_t s = ((uint32_t)a[i].v[j] * factor_q16) >> 16;
            out[i].v[j] = (s > 65535) ? 65535 : (uint16_t)s;
        }
    }
}

/* ============================================================================
 * HC16 批量软阈值（SIMD 加速）
 * ============================================================================ */

void hc16_soft_threshold_batch(const hc16_t* a, const hc16_t* Lambda,
                                    hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc16_soft_threshold(&a[i], &Lambda[i]);
    }
}

/* ============================================================================
 * HC64 批量饱和加法（SSE2）
 *
 * HC64 每个元素 16 字节，恰好占满一个 SSE 寄存器。
 * 两个 64 位层：v[1]（低位）先加，进位传播到 v[0]（高位）。
 * ============================================================================ */

void hc64_add_sat_batch(const hc64_t* a, const hc64_t* b,
                                   hc64_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc64_add_sat(&a[i], &b[i]);
    }
}

/* ============================================================================
 * HC64 批量比较（SSE2）
 * ============================================================================ */

void hc64_less_batch(const hc64_t* a, const hc64_t* b,
                          uint8_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc64_less(&a[i], &b[i]) ? 1 : 0;
    }
}

#else /* !SGN_HAS_SSE2 → 标量回退 */

void hc16_add_sat_batch(const hc16_t* a, const hc16_t* b,
                                   hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = hc16_add_sat(&a[i], &b[i]);
    }
}

void hc16_less_batch(const hc16_t* a, const hc16_t* b,
                          uint8_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        out[i] = hc16_less(&a[i], &b[i]) ? 1 : 0;
}

void hc16_scale_batch(const hc16_t* a, uint32_t factor_q16,
                           hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        for (int j = 0; j < 4; ++j) {
            uint32_t s = ((uint32_t)a[i].v[j] * factor_q16) >> 16;
            out[i].v[j] = (s > 65535) ? 65535 : (uint16_t)s;
        }
    }
}

void hc16_soft_threshold_batch(const hc16_t* a, const hc16_t* Lambda,
                                    hc16_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        out[i] = hc16_soft_threshold(&a[i], &Lambda[i]);
}

void hc64_add_sat_batch(const hc64_t* a, const hc64_t* b,
                                   hc64_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        out[i] = hc64_add_sat(&a[i], &b[i]);
}

void hc64_less_batch(const hc64_t* a, const hc64_t* b,
                          uint8_t* out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        out[i] = hc64_less(&a[i], &b[i]) ? 1 : 0;
}

#endif /* SGN_HAS_SSE2 */
