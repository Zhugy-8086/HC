// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086
/**
 * @file pysgn_col2im.cpp
 * @brief pysgn_col2im - col2im C 扩展的 Python 绑定（ResNet-18 反向加速）
 *
 * 独立 pybind11 扩展，不依赖 pysgn_net / pysgn 等其他模块。
 * 仅暴露一个函数 col2im_add，用于替代 numpy _col2im 的 scatter-add 核心。
 *
 * 暴露的 Python API：
 *   - col2im_add(x_col, x_padded, B, C, kh, kw, H_out, W_out, stride, H_padded, W_padded)
 *       原地累加：x_padded[b,c,i+ho*stride,j+wo*stride] += x_col[b,c,i,j,ho,wo]
 *       x_col 形状 (B, C, kh, kw, H_out, W_out) C-contiguous float32
 *       x_padded 形状 (B, C, H_padded, W_padded) C-contiguous float32
 *
 * 编译：
 *   cd fixes_相关修复/hc_v1.3_net_extension/
 *   py -3.14 setup_col2im.py build_ext --inplace
 *
 * 验证：
 *   py -3.14 test_col2im_c.py
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "col2im_c.h"

#include <cstdint>
#include <string>

namespace py = pybind11;

/**
 * col2im scatter-add（原地累加到 x_padded）
 *
 * Args:
 *   x_col:    (B, C, kh, kw, H_out, W_out) numpy float32, C-contiguous
 *   x_padded: (B, C, H_padded, W_padded) numpy float32, C-contiguous（会被原地修改）
 *   B, C, kh, kw, H_out, W_out, stride, H_padded, W_padded: 维度参数
 */
static void py_col2im_add(
    py::array_t<float, py::array::c_style | py::array::forcecast> x_col,
    py::array_t<float, py::array::c_style | py::array::forcecast> x_padded,
    int B, int C, int kh, int kw,
    int H_out, int W_out, int stride,
    int H_padded, int W_padded
) {
    auto x_col_buf = x_col.request();
    auto x_padded_buf = x_padded.request();

    /* 形状校验：x_col 必须是 6D (B, C, kh, kw, H_out, W_out) */
    if (x_col_buf.ndim != 6) {
        throw std::runtime_error(
            "x_col 必须是 6D 数组 (B, C, kh, kw, H_out, W_out)，得到 ndim=" +
            std::to_string(x_col_buf.ndim)
        );
    }
    const py::ssize_t expected_col[6] = {B, C, kh, kw, H_out, W_out};
    for (int i = 0; i < 6; ++i) {
        if (x_col_buf.shape[i] != expected_col[i]) {
            throw std::runtime_error(
                "x_col 形状不匹配：期望 (" + std::to_string(B) + ", " +
                std::to_string(C) + ", " + std::to_string(kh) + ", " +
                std::to_string(kw) + ", " + std::to_string(H_out) + ", " +
                std::to_string(W_out) + ")"
            );
        }
    }

    /* 形状校验：x_padded 必须是 4D (B, C, H_padded, W_padded) */
    if (x_padded_buf.ndim != 4) {
        throw std::runtime_error(
            "x_padded 必须是 4D 数组 (B, C, H_padded, W_padded)，得到 ndim=" +
            std::to_string(x_padded_buf.ndim)
        );
    }
    const py::ssize_t expected_pad[4] = {B, C, H_padded, W_padded};
    for (int i = 0; i < 4; ++i) {
        if (x_padded_buf.shape[i] != expected_pad[i]) {
            throw std::runtime_error(
                "x_padded 形状不匹配：期望 (" + std::to_string(B) + ", " +
                std::to_string(C) + ", " + std::to_string(H_padded) + ", " +
                std::to_string(W_padded) + ")"
            );
        }
    }

    /* 维度合法性检查 */
    if (B <= 0 || C <= 0 || kh <= 0 || kw <= 0 ||
        H_out <= 0 || W_out <= 0 || stride <= 0 ||
        H_padded <= 0 || W_padded <= 0) {
        throw std::runtime_error("所有维度参数必须为正数");
    }
    /* 越界检查：i + (H_out-1)*stride < H_padded, j + (W_out-1)*stride < W_padded */
    if (kh - 1 + (H_out - 1) * stride >= H_padded) {
        throw std::runtime_error(
            "col2im 越界：kh-1 + (H_out-1)*stride >= H_padded (" +
            std::to_string(kh - 1) + " + " + std::to_string((H_out - 1) * stride) +
            " >= " + std::to_string(H_padded) + ")"
        );
    }
    if (kw - 1 + (W_out - 1) * stride >= W_padded) {
        throw std::runtime_error(
            "col2im 越界：kw-1 + (W_out-1)*stride >= W_padded (" +
            std::to_string(kw - 1) + " + " + std::to_string((W_out - 1) * stride) +
            " >= " + std::to_string(W_padded) + ")"
        );
    }

    const float* col_ptr = (const float*)x_col_buf.ptr;
    float* pad_ptr = (float*)x_padded_buf.ptr;

    /* 释放 GIL 进行纯 C 计算（OpenMP 并行，无 Python 对象访问） */
    {
        py::gil_scoped_release release;
        col2im_add_c(col_ptr, pad_ptr,
                     B, C, kh, kw, H_out, W_out, stride,
                     H_padded, W_padded);
    }
}

PYBIND11_MODULE(pysgn_col2im, m) {
    m.doc() = "col2im C extension for ResNet-18 backward acceleration";

    m.def("col2im_add", &py_col2im_add,
          py::arg("x_col"), py::arg("x_padded"),
          py::arg("B"), py::arg("C"), py::arg("kh"), py::arg("kw"),
          py::arg("H_out"), py::arg("W_out"), py::arg("stride"),
          py::arg("H_padded"), py::arg("W_padded"),
          "col2im scatter-add（原地累加到 x_padded）。\n\n"
          "数学等价于 numpy _col2im 的核心 scatter-add：\n"
          "  x_padded[b,c,i+ho*stride,j+wo*stride] += x_col[b,c,i,j,ho,wo]\n\n"
          "Args:\n"
          "  x_col:    (B, C, kh, kw, H_out, W_out) float32, C-contiguous\n"
          "  x_padded: (B, C, H_padded, W_padded) float32, C-contiguous（原地修改）\n"
          "  B, C, kh, kw, H_out, W_out, stride, H_padded, W_padded: 维度参数\n\n"
          "Note:\n"
          "  x_padded 必须预先清零或包含需要累加的初始值（函数做 += 而非 =）");

    m.attr("__version__") = "1.0.0-col2im";
}