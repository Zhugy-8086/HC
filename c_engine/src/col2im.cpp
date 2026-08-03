// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086
//
// col2im.cpp - col2im 操作 C++ 封装
//
// col2im_c.c 作为独立 C 源文件编译（保留 C 语义），
// 通过 col2im_c.h 引用 C 接口声明。

#include "col2im.h"
#include "col2im_c.h"

// col2im_c.c 作为独立 C 源文件在 CMakeLists.txt 中编译（不再 #include 到 C++）
// 这样 C 代码保留 C 语义，Clang 以 C 编译器处理而非 C++ 编译器

// -----------------------------------------------------------------------------
// 串行回退：小规模时 OpenMP 线程池 fork/join 开销 > 计算量
// libomp 的线程池启动开销（~20-50μs）在 BC < 128 时显著影响性能
// 串行路径绕过 OpenMP 运行时，直接执行累加循环
// -----------------------------------------------------------------------------
static void col2im_add_serial(const float* x_col, float* x_padded,
                              int B, int C, int kh, int kw,
                              int H_out, int W_out, int stride,
                              int H_padded, int W_padded) {
    const int kh_kw_Hout_Wout = kh * kw * H_out * W_out;
    const int kw_Hout_Wout = kw * H_out * W_out;
    const int Hout_Wout = H_out * W_out;
    const int Hp_Wp = H_padded * W_padded;

    for (int b = 0; b < B; ++b) {
        for (int c = 0; c < C; ++c) {
            float* dst_bc = x_padded + (size_t)b * C * Hp_Wp + (size_t)c * Hp_Wp;
            const float* src_bc = x_col + (size_t)b * C * kh_kw_Hout_Wout
                                        + (size_t)c * kh_kw_Hout_Wout;
            for (int i = 0; i < kh; ++i) {
                float* dst_i = dst_bc + (size_t)i * W_padded;
                const float* src_i = src_bc + (size_t)i * kw_Hout_Wout;
                for (int j = 0; j < kw; ++j) {
                    const float* src_ij = src_i + (size_t)j * Hout_Wout;
                    float* dst_ij_col = dst_i + j;
                    if (stride == 1) {
                        for (int ho = 0; ho < H_out; ++ho) {
                            float* dst_row = dst_ij_col + (size_t)ho * W_padded;
                            const float* src_row = src_ij + (size_t)ho * W_out;
                            int wo = 0;
                            for (; wo + 4 <= W_out; wo += 4) {
                                dst_row[wo]     += src_row[wo];
                                dst_row[wo + 1] += src_row[wo + 1];
                                dst_row[wo + 2] += src_row[wo + 2];
                                dst_row[wo + 3] += src_row[wo + 3];
                            }
                            for (; wo < W_out; ++wo) {
                                dst_row[wo] += src_row[wo];
                            }
                        }
                    } else {
                        for (int ho = 0; ho < H_out; ++ho) {
                            float* dst_row = dst_ij_col
                                           + (size_t)ho * stride * W_padded;
                            const float* src_row = src_ij + (size_t)ho * W_out;
                            for (int wo = 0; wo < W_out; ++wo) {
                                dst_row[wo * stride] += src_row[wo];
                            }
                        }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// C++ 类封装：Col2im::execute 根据规模选择串行/并行路径
// BC < 128 时串行（避免 libomp 线程池开销），否则 OpenMP 并行
// -----------------------------------------------------------------------------
void Col2im::execute(const float* x_col, float* x_padded,
                     int B, int C, int kh, int kw,
                     int H_out, int W_out, int stride,
                     int H_padded, int W_padded) {
    const long BC = (long)B * (long)C;
    if (BC < 128) {
        col2im_add_serial(x_col, x_padded,
                          B, C, kh, kw, H_out, W_out, stride,
                          H_padded, W_padded);
    } else {
        col2im_add_c(x_col, x_padded,
                     B, C, kh, kw, H_out, W_out, stride,
                     H_padded, W_padded);
    }
}