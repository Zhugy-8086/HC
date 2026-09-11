// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hpdc_cpp_example.cpp
 * @brief HPDC C++ 包装器使用示例
 * @version 1.0.0
 *
 * 编译：
 *   g++ -std=c++11 -I../include \
 *       ../src/hc.c ../src/hc8.c ../src/hc16.c ../src/hc32.c ../src/hc64.c ../src/dc.c \
 *       ../src/hpdc_sandbox.cpp ../src/hpdc_trie.cpp ../src/hpdc_engine.cpp \
 *       hpdc_cpp_example.cpp -o example
 */

#include "hc/hpdc_cpp.hpp"
#include <iostream>
#include <vector>
#include <cassert>

using namespace hpdc;

int main() {
    std::cout << "=== HPDC C++ Wrapper Demo ===" << std::endl;

    /* ========================================================================
     * 1. HC8：完整功能（C API 已全部实现）
     * ======================================================================== */
    std::cout << "\n--- HC8 (6 layers x 8 bits) ---" << std::endl;

    HC8 a(3.14159);           // 从 float 构造
    HC8 b(2.71828);
    std::cout << "a = " << a.repr() << std::endl;
    std::cout << "b = " << b.repr() << std::endl;

    // 运算
    HC8 c = a + b;            // 饱和加法
    HC8 d = a - b;            // 减法
    std::cout << "a + b = " << c.repr() << std::endl;
    std::cout << "a - b = " << d.repr() << std::endl;

    // 比较
    std::cout << "a < b = " << (a < b) << std::endl;
    std::cout << "a == b = " << (a == b) << std::endl;

    // 层访问（自动边界检查，layers=6）
    std::cout << "a[0] (int layer) = " << (int)a[0] << std::endl;
    std::cout << "a[1] (frac[0])   = " << (int)a[1] << std::endl;
    // a[10]; // 运行时会抛 std::out_of_range

    // 序列化
    std::string bytes = a.to_bytes();
    HC8 a2 = HC8::from_bytes(bytes);
    std::cout << "roundtrip bytes: " << a2.repr() << std::endl;

    // 常量
    HC8 z = HC8::zero();
    HC8 mx = HC8::max_val();
    std::cout << "zero = " << z.repr() << ", max = " << mx.repr() << std::endl;

    /* ========================================================================
     * 2. HC16：完整功能（部分 C API 缺失，包装器内联补齐）
     * ======================================================================== */
    std::cout << "\n--- HC16 (4 layers x 16 bits) ---" << std::endl;

    HC16 x(1000.5);
    HC16 y(333.3);
    std::cout << "x = " << x.repr() << std::endl;
    std::cout << "y = " << y.repr() << std::endl;

    HC16 zsum = x + y;
    HC16 zdiff = x - y;
    std::cout << "x + y = " << zsum.repr() << std::endl;
    std::cout << "x - y = " << zdiff.repr() << std::endl;

    std::cout << "x < y = " << (x < y) << std::endl;

    // 层访问（自动边界检查，layers=4）
    std::cout << "x[0] = " << x[0] << ", x[3] = " << x[3] << std::endl;

    /* ========================================================================
     * 3. HC32：完整功能（阶段2 已补齐 C API）
     * ======================================================================== */
    std::cout << "\n--- HC32 (3 layers x 32 bits) ---" << std::endl;

    HC32 big(123456789.0);
    HC32 big2(987654321.0);
    std::cout << "big = HC32(" << big.to_float() << ")" << std::endl;
    std::cout << "big2 = HC32(" << big2.to_float() << ")" << std::endl;
    std::cout << "HC32 layers = " << HC32::layers << ", elem_bits = " << HC32::elem_bits << std::endl;

    HC32 big_sum = big + big2;
    HC32 big_diff = big2 - big;
    std::cout << "big + big2 = " << big_sum.to_float() << std::endl;
    std::cout << "big2 - big = " << big_diff.to_float() << std::endl;
    std::cout << "big < big2 = " << (big < big2) << std::endl;

    /* ========================================================================
     * 4. Sandbox：模板化投影与运算
     * ======================================================================== */
    std::cout << "\n--- Sandbox ---" << std::endl;

    Sandbox sb;

    // 自动分派 project：HC8 → project_hc8，HC16 → project_hc16，HC32 → project_hc32
    double phi_a = sb.project(a);
    double phi_x = sb.project(x);
    double phi_big = sb.project(big);
    std::cout << "project(a) = " << phi_a << std::endl;
    std::cout << "project(x) = " << phi_x << std::endl;
    std::cout << "project(big) = " << phi_big << std::endl;

    // 除法（任意除数！）
    HC8 q = sb.divide(a, b);
    std::cout << "divide(a, b) = " << q.repr() << std::endl;

    // 梯度下降
    HC8 h(100.0);
    h = sb.gradient(h, 0.5, 0.01);
    std::cout << "gradient(100, 0.5, 0.01) = " << h.repr() << std::endl;

    // 缩放
    HC8 scaled = sb.scale(a, 2.5);
    std::cout << "scale(a, 2.5) = " << scaled.repr() << std::endl;

    // 泛型 map：任意 lambda
    HC8 squared = sb.map(a, [](double v) { return v * v; });
    std::cout << "map(a, v*v) = " << squared.repr() << std::endl;

    HC8 avg = sb.map2(a, b, [](double u, double v) { return (u + v) / 2.0; });
    std::cout << "map2(a, b, avg) = " << avg.repr() << std::endl;

    // 批量操作
    std::vector<HC8> arr = { HC8(1.0), HC8(2.0), HC8(3.0), HC8(4.0) };
    std::vector<double> phis = sb.project_batch(arr);
    std::cout << "project_batch: ";
    for (auto p : phis) std::cout << p << " ";
    std::cout << std::endl;

    std::vector<double> processed = { 0.1, 0.2, 0.3, 0.4 };
    std::vector<HC8> back = sb.inverse_batch<HC8>(processed);
    std::cout << "inverse_batch: ";
    for (auto& h : back) std::cout << h.to_float() << " ";
    std::cout << std::endl;

    /* ========================================================================
     * 5. DC：十进制定点数（空间变量）
     * ======================================================================== */
    std::cout << "\n--- DC (Decimal Coordinate) ---" << std::endl;

    DC coord(0.1, 1);           // 1 位小数
    DC offset(0.001, 3);        // 3 位小数
    std::cout << "coord = " << coord.to_float() << " (level=" << coord.level() << ")" << std::endl;
    std::cout << "offset = " << offset.to_float() << " (level=" << offset.level() << ")" << std::endl;

    DC moved = coord + offset;  // 自动对齐到 level=3
    std::cout << "coord + offset = " << moved.to_float() << " (level=" << moved.level() << ")" << std::endl;

    std::cout << "JSON: " << moved.to_json() << std::endl;

    // DC <-> HC 桥梁
    DC from_hc = sb.hc_to_dc(a, 3);
    HC8 to_hc = sb.dc_to_hc(from_hc, SGN_PREC_STD);
    std::cout << "a -> dc -> hc roundtrip: " << to_hc.repr() << std::endl;

    /* ========================================================================
     * 6. TriePool：超度量树索引
     * ======================================================================== */
    std::cout << "\n--- TriePool ---" << std::endl;

    TriePool pool;
    HC8 sig1(1.1), sig2(1.2), sig3(2.1);
    bool ok1 = pool.insert(sig1, 101);
    bool ok2 = pool.insert(sig2, 102);
    bool ok3 = pool.insert(sig3, 103);
    std::cout << "insert 3 templates: " << ok1 << " " << ok2 << " " << ok3 << std::endl;

    /* ========================================================================
     * 7. 软阈值（稀疏诱导）
     * ======================================================================== */
    std::cout << "\n--- Soft Threshold ---" << std::endl;

    HC8 X(10.0), Lambda(3.0);
    HC8 st = soft_threshold(X, Lambda);
    std::cout << "soft_threshold(10, 3) = " << st.repr() << std::endl;

    HC8 small(2.0);
    HC8 st_zero = soft_threshold(small, Lambda);
    std::cout << "soft_threshold(2, 3) = " << st_zero.repr() << " (below threshold -> 0)" << std::endl;

    std::cout << "\n=== All demos passed ===" << std::endl;
    return 0;
}
