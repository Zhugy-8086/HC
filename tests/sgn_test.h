/**
 * @file sgn_test.h
 * @brief SGN 轻量级测试框架 - 数据驱动，易扩展
 * @version 1.0.0
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
 */

#ifndef SGN_TEST_H
#define SGN_TEST_H

#include <stdio.h>
#include <math.h>
#include <string.h>

/* ============================================================================
 * 测试状态（全局）
 * ============================================================================ */

static int _sgn_test_pass = 0;
static int _sgn_test_fail = 0;
static int _sgn_test_total = 0;
static const char* _sgn_test_current = NULL;

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

#define RUN(name) do { \
    _sgn_test_current = #name; \
    _sgn_test_total++; \
    _sgn_test_##name(); \
} while(0)

#define RUN_IF(name, cond) do { \
    if (cond) { RUN(name); } \
} while(0)

/* ============================================================================
 * 结果汇总
 * ============================================================================ */

#define SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Tests: %d | PASS: %d | FAIL: %d\n", _sgn_test_total, _sgn_test_pass, _sgn_test_fail); \
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

#include <time.h>

#define BENCH_START() clock_t _bench_start = clock()
#define BENCH_END(label, n) do { \
    double _ms = (double)(clock() - _bench_start) / CLOCKS_PER_SEC * 1000.0; \
    printf("  BENCH [%s]: %.3f ms (%d ops, %.3f us/op)\n", \
           label, _ms, (n), _ms * 1000.0 / (n)); \
} while(0)

#endif /* SGN_TEST_H */
