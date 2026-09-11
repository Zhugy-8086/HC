// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file sgn_test.h
 * @brief SGN 轻量级测试框架 - 数据驱动，易扩展
 * @version 1.1.0
 *
 * 用法：
 *   #include "sgn_test.h"
 *
 *   TEST(hc8_add) {
 *       hc8_t a = hc8_from_double(3.0, SGN_OVERFLOW_SATURATE);
 *       hc8_t b = hc8_from_double(2.0, SGN_OVERFLOW_SATURATE);
 *       hc8_t c = hc8_add_sat(&a, &b);
 *       ASSERT_NEAR(hc8_to_double(c), 5.0, 0.01);
 *   }
 *
 *   int main(void) {
 *       SGN_TEST_INIT();           // 读取环境变量配置（过滤/超时）
 *       RUN(hc8_add);
 *       RUN(hc16_sub);
 *       SUMMARY();
 *   }
 *
 * 数据驱动示例：
 *   TEST_DATA(hc8_roundtrip, double, inputs, 5) {
 *       for (int i = 0; i < count; i++) {
 *           hc8_t h = hc8_from_double(inputs[i], SGN_OVERFLOW_SATURATE);
 *           ASSERT_NEAR(hc8_to_double(h), inputs[i], 0.01);
 *       }
 *   }
 *
 * 运行时过滤（环境变量 SGN_TEST_FILTER，子串匹配测试名）：
 *   SGN_TEST_FILTER=hc8 ./test_hc          # 仅运行名字包含 "hc8" 的测试
 *   SGN_TEST_FILTER=crc ./test_storage
 *
 * 超时保护（环境变量 SGN_TEST_TIMEOUT，单位秒，默认 5.0）：
 *   SGN_TEST_TIMEOUT=2.0 ./test_hc         # 单个测试超过 2 秒即判 FAIL
 */

#ifndef SGN_TEST_H
#define SGN_TEST_H

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ============================================================================
 * 测试状态（全局）
 * ============================================================================ */

static int _sgn_test_pass = 0;
static int _sgn_test_fail = 0;
static int _sgn_test_total = 0;
static int _sgn_test_skip = 0;          /* 被过滤跳过的测试数 */
static int _sgn_test_timeout_fail = 0;  /* 因超时失败的测试数 */
static const char* _sgn_test_current = NULL;

/* 运行时过滤与超时配置（通过环境变量注入） */
static const char* _sgn_test_filter = NULL;     /* NULL 表示无过滤 */
static double      _sgn_test_timeout_sec = 5.0; /* 默认 5 秒 */

/* ============================================================================
 * 框架初始化 - 从环境变量读取过滤串与超时阈值
 * 应在 main() 起始处、RUN 之前调用一次。
 * ============================================================================ */

#define SGN_TEST_INIT() do { \
    const char* _f = getenv("SGN_TEST_FILTER"); \
    if (_f && *_f) { _sgn_test_filter = _f; } \
    const char* _t = getenv("SGN_TEST_TIMEOUT"); \
    if (_t && *_t) { \
        double _v = atof(_t); \
        if (_v > 0.0) { _sgn_test_timeout_sec = _v; } \
    } \
    if (_sgn_test_filter) { \
        printf("  [filter] \"%s\"\n", _sgn_test_filter); \
    } \
    printf("  [timeout] %.3fs\n", _sgn_test_timeout_sec); \
} while(0)

/* ============================================================================
 * 断言宏
 * ============================================================================ */

#define ASSERT_TRUE(cond) do { \
    if (cond) { _sgn_test_pass++; } \
    else { \
        _sgn_test_fail++; \
        printf("  FAIL [%s] line %d: %s\n", _sgn_test_current, __LINE__, #cond); \
    } \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

#define ASSERT_NEAR(a, b, eps) ASSERT_TRUE(fabs((double)(a) - (double)(b)) < (eps))

#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

#define ASSERT_NOT_NULL(p) ASSERT_TRUE((p) != NULL)

#define ASSERT_NULL(p) ASSERT_TRUE((p) == NULL)

/* ============================================================================
 * 测试注册与运行
 * ============================================================================ */

#define TEST(name) \
    static void _sgn_test_##name(void)

#define TEST_DATA(name, type, data_var, data_count) \
    static void _sgn_test_##name(void)

/* 数据源宏：在 TEST_DATA 外部定义数据 */
#define TEST_ARRAY(type, name, ...) \
    static const type name[] = { __VA_ARGS__ }; \
    static const int name##_count = sizeof(name) / sizeof(name[0])

/* 内部：运行单个测试并计时，处理超时（不包含过滤逻辑） */
#define _SGN_RUN_TIMED(name) do { \
    clock_t _t_start = clock(); \
    _sgn_test_##name(); \
    double _t_elapsed = (double)(clock() - _t_start) / CLOCKS_PER_SEC; \
    if (_t_elapsed > _sgn_test_timeout_sec) { \
        _sgn_test_fail++; \
        _sgn_test_timeout_fail++; \
        printf("  TIMEOUT [%s] %.3fs > %.3fs (line-marker: assertion+timeout)\n", \
               #name, _t_elapsed, _sgn_test_timeout_sec); \
    } \
} while(0)

#define RUN(name) do { \
    _sgn_test_current = #name; \
    if (_sgn_test_filter && !strstr(#name, _sgn_test_filter)) { \
        _sgn_test_skip++; \
        break; \
    } \
    _sgn_test_total++; \
    _SGN_RUN_TIMED(name); \
} while(0)

#define RUN_IF(name, cond) do { \
    _sgn_test_current = #name; \
    if (!(cond)) { \
        _sgn_test_skip++; \
        break; \
    } \
    if (_sgn_test_filter && !strstr(#name, _sgn_test_filter)) { \
        _sgn_test_skip++; \
        break; \
    } \
    _sgn_test_total++; \
    _SGN_RUN_TIMED(name); \
} while(0)

/* ============================================================================
 * 结果汇总
 * ============================================================================ */

#define SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Tests: %d | PASS: %d | FAIL: %d | SKIP: %d\n", \
           _sgn_test_total, _sgn_test_pass, _sgn_test_fail, _sgn_test_skip); \
    if (_sgn_test_timeout_fail > 0) { \
        printf("  (of which %d timeout failures)\n", _sgn_test_timeout_fail); \
    } \
    printf("========================================\n"); \
    if (_sgn_test_fail == 0) { \
        printf("ALL PASSED\n"); \
    } else { \
        printf("FAILED: %d assertions\n", _sgn_test_fail); \
    } \
    return _sgn_test_fail > 0 ? 1 : 0; \
} while(0)

/* ============================================================================
 * 性能计时（可选）
 * ============================================================================ */

#define BENCH_START() clock_t _bench_start = clock()
#define BENCH_END(label, n) do { \
    double _ms = (double)(clock() - _bench_start) / CLOCKS_PER_SEC * 1000.0; \
    printf("  BENCH [%s]: %.3f ms (%d ops, %.3f us/op)\n", \
           label, _ms, (n), _ms * 1000.0 / (n)); \
} while(0)

#endif /* SGN_TEST_H */
