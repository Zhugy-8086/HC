# SGN — 超度量数系技术栈

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.22711085.svg)](https://doi.org/10.5281/zenodo.22711085)

> **引用**：概念 DOI `10.5281/zenodo.22711085`（始终解析到最新版本）；精确引用本版（v0.2.0）用 `10.5281/zenodo.22711086`。

> **主版本**: ABI v2.0.0 (SGN_ABI_MAJOR=2, SGN_ABI_MINOR=0, SGN_ABI_PATCH=0)
> **语言**: C11 / C++11 / C++23（含 Python 绑定 via pybind11）
> **许可证**: Apache License 2.0 — 见 [LICENSE](LICENSE)

[![CI](https://github.com/Zhugy-8086/HC/actions/workflows/ci.yml/badge.svg)](https://github.com/Zhugy-8086/HC/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows-blue)](.github/workflows/ci.yml)

---

> ⚠️ **状态：已完结（数学上已证伪）**
> 本仓库（HC / SGN 数值类型系统）已完成并归档。但需注意：作为**神经网络数系路线**，
> HC 在数学上已被证伪——详见 [integer-quant-math-verification](https://github.com/Zhugy-8086/integer-quant-math-verification)
> 的"超度量违反率≈100%"结论：梯度幅度在 log 距离下不满足超度量不等式，
> HC 赖以成立的超度量结构在 NN 场景下不成立，**无法支持反向梯度/训练**。
> 因此 HC 的神经网络方向终止；本仓库仅保留其数值类型与（前向）量化推理成果作为归档。

## 项目介绍

SGN (Signed Globular Number) 是一套**基于超度量空间的自定义数值类型系统**。核心思想：用多层字节数组表示数，每层是独立的进制位（256 叉 / 65536 叉），天然构成超度量树结构——字典序即超度量距离，前缀即超度量球。

SGN 同时服务于两个世界：

- **嵌入式 / MCU 场景**：最小化链接体积（仅需 `hc.c` + `hc8.c`，无第三方依赖），用于时间变量、精确记录、Flash 容错。
- **PC 端神经网络量化推理**：围绕 HC8/HC16 核心构建 int4/int8/int16 低精度整数矩阵乘路径，支持 AVX2 / FMA / AVX-VNNI 指令集加速。

---

## 版本速览

仓库内包含两个独立版本目录，可按需使用：

| 版本 | 定位 | 语言标准 | 构建目标 | 维护状态 |
|------|------|---------|---------|---------|
| **v0.1** | SGN 主线完整实现 | C11 / C++11 | 静态库 `sgn_core` / `sgn_ext` / `sgn` + 测试 | 主线（推荐研究/扩展使用） |
| **v0.2** | HC 神经网络运算扩展模块（C 末尾版） | C11 / C++23 | pybind11 模块 `sgn_c_engine*.so` | **C 语言实现末尾版，不再追加新功能** |

两者关系：**v0.2 是从 v0.1 抽取 HC8/HC16 核心后，在神经网络量化推理方向的深度扩展**（SBE 分块量化、VNNI matmul、Conv2d 融合、多视角存储等）。v0.2 移除了 HC32/HC64/DC/沙盒/Trie/引擎/存储/网络/插件等 v0.1 扩展，专注低精度矩阵运算。

---

## 核心 HC 类型（HC = 层级小数）

所有 HC 变体共享同一张**编译期元数据表**（表驱动，消除硬编码层数/基数），由 [v0.2/include/hc/hc.h](v0.2/include/hc/hc.h) 的 `hc_meta_t` / `hc_meta_get()` 统一管理。

| 类型 | 结构 | 层 × 位宽 | 小数精度 | 数值范围 | 典型用途 |
|------|------|-----------|---------|---------|---------|
| **HC8** | `uint8_t v[6]`（紧缩 6 字节） | 6 × 8 bit | 48 bit | [0, 256) | 嵌入式主力、对称量化 int8 矩阵乘、HC4 非对称拆分基础 |
| **HC16** | `uint16_t v[4]`（8 字节） | 4 × 16 bit | 64 bit | [0, 65536) | 时间戳、高精度存储、int16×int16 量化矩阵乘（精度比 HC8 高 256 倍） |
| HC32 | `uint32_t v[3]`（12 字节） | 3 × 32 bit | 96 bit | [0, 2³²) | PC 端高精度扩展（仅 v0.1 提供） |
| HC64 | `uint64_t v[2]`（16 字节） | 2 × 64 bit | 128 bit | [0, 2⁶⁴) | 高精度实验扩展（仅 v0.1 提供） |

配套扩展类型（非独立 ABI，依附 HC）：

- `shc8_t`（有符号 HC8：sign + int_part + hc8_t）
- `leveled_hpdc8_t`（合体模式：level × 256 + HC8，担任 SGN 时间变量）
- `hc16_lamport_t`（HC16 Lamport 逻辑时钟）
- `dc_t`（十进制定点桥接：`(int64_t index, uint32_t level)`，见 v0.1 [include/hc/dc.h](v0.1/sgn/include/hc/dc.h)）

---

## v0.1 主线：完整技术栈模块

> 详细文档见 [v0.1/README.md](v0.1/README.md)。v0.1 区分「本职工作」与「扩展能力」两层——嵌入式只链本职，PC 按需追加扩展。

### 本职工作（HC 之所以存在）

| 能力 | 说明 | 关键 API |
|------|------|---------|
| 时间变量 | 合体模式 `level × 256 + HC` 担任 SGN 时间变量本身 | `leveled_hpdc8_t` / `leveled_add` |
| 精确记录 | 跨平台字节级一致、可压缩、可校验的持久化格式 | 13 字节固定记录 / CRC8 / Merkle / RS(8,6) / TMR 三模冗余 |
| 超度量比较与索引 | 字典序 = 超度量距离，Trie 前缀 = 超度量球 | `hc8_less` / `hc8_equal` / HP Trie |
| 基础数值运算 | 饱和加 / 回绕加 / 合体进位 / 减法 / 软阈值 / 移位 / 校验和 | `hc8_add_sat` / `hc8_sub` / `hc8_soft_threshold` |

### 扩展能力（按需链接，不拖累本职）

| 模块 | 位置（v0.1/sgn/include/hc） | 能力 |
|------|------------------------------|------|
| **Trie 索引** | `hpdc_trie.h` | 256 叉超度量前缀树，前缀剪枝 + 候选收集 |
| **WTA / LRU 引擎** | `hpdc_engine.h` | Winner-Take-All 竞争学习 + LRU 模板库容量管理 + 形态学 + Bitonic 排序网络 |
| **存储可靠性** | `hpdc_storage.h` | RLE 压缩 / Merkle 树 / RS(8,6) 纠错 / TMR 三模冗余 / Flash 容错 |
| **网络分布式** | `hpdc_network.h` | UART 帧 / COBS 零字节消除 / HC16 Lamport 时钟 / 看门狗 |
| **插件系统** | `hpdc_plugin.h` | NORMATIVE / EXTENSION / HYBRID 三类插件 + 动态加载 + 能力掩码 |
| **投影沙盒** | `hpdc_sandbox.h` | HC ↔ float64/float128 双向投影 / 除法 / 梯度 / 缩放（PC 数值实验） |
| **SIMD 批量** | `hc_simd.h` | SSE2 批量加 / 比较 / 软阈值（自动标量回退） |
| **DC 桥接** | `dc.h` | HC ↔ 十进制定点 ↔ double 的互转扩展（非独立 ABI） |
| **C++ RAII** | `hpdc_cpp.hpp` | HC8/HC16/HC32/HC64 类包装 + 运算符重载 + `hpdc::op` 极简便利层 |

---

## v0.2 神经网络扩展：量化推理专用

> 详细文档见 [v0.2/README.md](v0.2/README.md)。v0.2 为 C 语言实现末尾版，之后不再追加新 C 模块。

### 模块清单

| 模块 | 头文件（v0.2/src） | 核心能力 | 加速指令集 |
|------|-------------------|---------|-----------|
| **HC8 神经网络** | `hc8_net.h` | 对称量化（scale+offset 128）int8×int8→int32 矩阵乘 / 整数 ReLU / UFP-1 & UFP-2 残差精度 / 扁平⇄紧缩互转 / SBE 语义块编码 | AVX-VNNI `_mm256_dpbusd_epi32` |
| **HC16 神经网络** | `hc16_net.h` | int16 有符号直接存储矩阵乘 / 精度比 HC8 提升 256 倍 | AVX2 `_mm256_madd_epi16` |
| **HC16MS 多视角** | `hc16ms.h` | 2 字节 int16 容器 → HC16 / HC8 / HC4 三档精度**零拷贝**切换 / 3× 存储压缩 | AVX2 |
| **HC4 PSHUFB** | `hc4_pshufb.h` | int4×int4→int8 查表乘法（16×16 LUT 256 字节）/ PSHUFB 每批 32 乘积 / 量化 float→float matmul | AVX2 `_mm256_shuffle_epi8` |
| **col2im** | `col2im.h` | im2col 反向 scatter-add / BC<128 串行绕过 libomp 开销 / extern "C" 兼容 | OpenMP |
| **HC8 余积** | `hc8_coproduct.h` | HC8 余积运算扩展绑定 | — |

### SBE 语义块编码家族（v0.2 重点交付）

`hc8_net.h` 中实现的 SBE 系列 API（C 化 + Python 绑定）：

| API 族 | 作用 | 精度提升机制 |
|--------|------|-------------|
| `sbe_quantize_weight_blocks` / `sbe_matmul` | per-block 分块量化 + VNNI kernel | 把大矩阵按 k 维切块，每块独立量化 scale |
| `sbe_matmul_smoothed` | Smoothing 融合：per-row mean shift 后再量化 | 主项 INT8 matmul（动态范围更小）+ 修正项 float 累加，精度更高 |
| `sbe_quantize_weight_blocks_perchannel` / `sbe_matmul_perchannel` | per-channel 量化（w_scales 从 groups 维升级为 groups×n） | CNN 精度逼近 float 阈值 |
| `sbe_conv2d_forward` | **Conv2d 前向融合**（im2col + SBE matmul + bias add 全在 C 层完成） | 消除 Python 层 im2col + transpose + reshape 开销 |
| `sbe_rescale_to_triple` | Triple-int8 缩放：float → 3 个 int8 分量（24-bit 精度） | 为 WEF+Triple 训练管线提供 C 级 30×+ 加速 |
| `hc8_multiview_matmul` | 正交多视角 matmul（HC 树并行解读） | 把累加值 C_acc 拆为 n_views 个位段视角并行乘权重，无交叉项复杂度 |

### Python 绑定（pybind11）

v0.2 通过单一 pybind11 模块 `sgn_c_engine` 暴露全部 API（注册器模式）：

| 源文件 | 绑定内容 |
|--------|---------|
| `src/pysgn_net.cpp` | HC8/HC4 残差矩阵乘 / SIMD 版 / SBE 全家桶 / Conv2d 融合 / Triple-int8 / 多视角 / OpenMP 线程控制 / AVX-VNNI 检测 |
| `src/pysgn_hc16.cpp` | HC16 神经网络矩阵乘 / ReLU / 存储格式互转 |
| `src/pysgn_hc16ms.cpp` | HC16MS 多视角零拷贝切换 / AVX2 检测 |
| `src/pysgn_hc4_pshufb.cpp` | HC4 PSHUFB 查表乘 / int4 矩阵乘 / 量化矩阵乘 |
| `src/pysgn_col2im.cpp` | `sgn.col2im_add` + `sgn.Col2im` C++ 类 + numpy 6D/4D 校验 |
| `src/col2im_bindings.cpp` / `hc8_coproduct_bindings.cpp` | 子模块注册器（供 placeholder 统一调用） |

> v0.1 的 Python 绑定见 [v0.1/sgn/bindings/python/](v0.1/sgn/bindings/python/)，使用 `setup.py` + pybind11。

---

## 快速开始

### C 最小集合（嵌入式，v0.1）

```c
#include "hc/hc16.h"

hc16_t a = hc16_from_double(1000.5, SGN_OVERFLOW_SATURATE);
hc16_t b = hc16_from_double( 500.25, SGN_OVERFLOW_SATURATE);
hc16_t c = hc16_add_sat(&a, &b);
double v = hc16_to_double(c);  // 1500.75
```

```bash
# 仅需 2 个 C 文件即可链接
arm-none-eabi-gcc -c -I v0.1/sgn/include  v0.1/sgn/src/hc.c   -o hc.o
arm-none-eabi-gcc -c -I v0.1/sgn/include v0.1/sgn/src/hc16.c -o hc16.o
```

### 便利宏（可选，v0.1 / v0.2 通用）

```c
#include "hc/hc8.h"
#include "hc/sgn_macros.h"

hc8_t a = HC8_FROM_DOUBLE(3.14);
hc8_t b = HC8_FROM_DOUBLE(2.0);
hc8_t c = HC8_ADD(a, b);       // 5.14
if (HC8_LESS(a, b)) { /* … */ }
hc8_t st = HC8_SOFT_THRESH(a, b);
```

### PC 端 C++（v0.1）

```cpp
#include "hc/hpdc_cpp.hpp"
using namespace hpdc;

HC8 a(3.14), b(2.72);
HC8 c = a + b;
HC32 big(123456789.0);

Sandbox sb;
double phi = sb.project(a);
HC8 q = sb.divide(a, b);
```

### SBE Conv2d 融合推理（Python + v0.2 构建产物）

```python
import numpy as np
import sgn_c_engine as sgn

# x: (B, C_in, H, W) float32
# w: (C_out, C_in, kh, kw) float32 → 先离线调用 sgn.sbe_quantize_weight_blocks
#   得到 (w_signed, w_sum_b, w_scales)，之后每帧推理只用这三个

y = sgn.sbe_conv2d_forward(
    x, w_signed, w_sum_b, w_scales,
    B, C_in, H, W,
    C_out, kh, kw, stride, padding,
    groups, k_block,
    bias,               # None 或 (C_out,)
)
```

---

## 编译与测试

### v0.1 完整构建（含测试套件，CI 默认构建）

```bash
cmake -B v0.1/build -S v0.1 -DCMAKE_BUILD_TYPE=Release
cmake --build v0.1/build -j

# 运行全部自动化测试（C 单元测试：HC8/HC16/HC32/HC64/DC/SIMD）
./v0.1/build/test_sgn
```

> GitHub Actions CI 在 `ubuntu-latest` / `macos-latest` / `windows-latest` 三个平台执行 v0.1 构建 + `test_sgn`。见 [.github/workflows/ci.yml](.github/workflows/ci.yml)。

### v0.2 神经网络扩展构建

前置依赖：**pybind11**、**OpenMP**、支持 AVX2 + FMA + AVX-VNNI 的工具链（GCC 11+ / Clang 14+ / MSVC 2022+）。

```bash
cmake -S v0.2 -B v0.2/build
cmake --build v0.2/build -j

# 产物：v0.2/build/sgn_c_engine*.so  （导入为 import sgn_c_engine as sgn）
```

可用编译宏：

| 宏 | 作用 | 版本 |
|----|------|------|
| `SGN_PC_EXTENSION` | 启用 PC 扩展函数 / 批量运算（v0.2 CMakeLists 默认 =1） | v0.1 / v0.2 |
| `SGN_USE_SIMD` | 启用 SIMD 批量操作（自动标量回退），需链接 hc_simd.c | v0.1 / v0.2 |
| `-O3 -mavx2 -mavxvnni -mfma` | v0.2 默认编译优化选项 | v0.2 |

---

## 项目结构

```
HC/
├── .github/workflows/          CI（三平台构建 + 跑 v0.1 test_sgn）
│   ├── ci.yml
│   └── codeql.yml
├── v0.1/                       SGN 主线完整实现（ABI v2）
│   ├── sgn/
│   │   ├── include/hc/         ABI 头文件（18 个头，含全部扩展）
│   │   ├── src/                实现（C 核心 7 文件 + C++ 扩展 8 文件）
│   │   ├── bindings/python/    pybind11 绑定 + setup.py + 示例
│   │   └── examples/           C++ RAII / Sandbox 示例
│   ├── tests/                  C 单元测试框架 + test_sgn.c + sgn_test.h
│   ├── CMakeLists.txt          sgn_core / sgn_ext / sgn 三静态库 + test_sgn
│   ├── LICENSE                 Apache 2.0
│   └── README.md               v0.1 详细文档
├── v0.2/                       HC 神经网络运算扩展（C 末尾版）
│   ├── include/hc/             ABI 头文件（hc/hc8/hc16 + SIMD + sgn 入口 + 宏）
│   ├── src/                    C 实现 + C++ 封装 + pybind11 绑定（共 22 文件）
│   │   ├── hc8_net.c/.h          SBE / VNNI / Conv2d / Smoothing / Triple / 多视角
│   │   ├── hc16_net.c/.h         int16 量化矩阵乘
│   │   ├── hc16ms.c/.h           HC16MS 多视角零拷贝存储
│   │   ├── hc4_pshufb.c/.h       int4 查表乘法 LUT
│   │   ├── col2im_c.c / col2im.cpp / col2im_bindings.cpp  C→C++ 迁移模板
│   │   ├── hc8_coproduct.h / bindings.cpp
│   │   └── pysgn_*.cpp           5 个 pybind11 注册器
│   ├── CMakeLists.txt          sgn_c_engine pybind11 模块（要求 C++23、VNNI）
│   ├── LICENSE                 Apache 2.0
│   └── README.md               v0.2 详细文档
├── LICENSE                     仓库全局 Apache 2.0 许可证
├── .gitignore
└── README.md                   本文件
```

---

## 版本规划

| 阶段 | 交付 | 状态 |
|------|------|------|
| v0.1 ABI v2.0.0 完整主线 | HC8/HC16/HC32/HC64/DC + Trie/引擎/存储/网络/插件/Sandbox/SIMD/C++/测试/Python 绑定 | ✅ 已发布 |
| v0.2 C 末尾版 | HC8/HC16 量化矩阵乘 + SBE 全家桶 + Conv2d 融合 + Triple-int8 + 多视角 + HC16MS + HC4 PSHUFB + col2im | ✅ 已发布（C 版本不再新增功能） |
| 后续方向 | C++ / Rust 重写 v0.2 特性，HC32/HC64 与 GPU 扩展 | 🔮 规划中 |

---

## 许可证

本仓库代码统一使用 **Apache License 2.0** 开源。

```
Copyright (c) 2026 zhugy-8086 and SGN Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

> **维护注记（2026-09-11）**：v0.2 核心 C 内核（hc4_pshufb / hc16_net / hc16ms /
> col2im_c）已同步 SGN 主线正确性修复——LUT 并发初始化原子化、`_aligned_free` 判空、
> `_mm256_sad_epu8` 索引修正、`size_t` 溢出防护。SGN 主线的后续演化（hc8_net 新增
> L1 距离内核、pybind11 绑定扩展等）见 [SGN 主仓](https://github.com/Zhugy-8086/SGN)，不再回灌本归档。
