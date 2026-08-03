# SGN C Engine — HC 神经网络运算扩展（C 末尾版）

> **版本**: v0.2（C 语言实现末尾版本，本库最后一次更新）
> **语言**: C11 / C++23（含 Python 绑定，pybind11）
> **许可证**: Apache License 2.0

---

## 1. 简介

`sgn_c_engine` 是 SGN 技术栈的 HC 神经网络运算扩展模块，围绕 HC8 / HC16
核心数值类型构建低精度整数矩阵运算与量化推理路径。本版本为 **C 语言实现的末尾版本**，
之后不再更新；HC32 / HC64 / DC / 沙盒 / Trie / 引擎 / 存储 / 网络 / 插件 / C++ 包装
等 v0.1 中的扩展模块在此版本不再提供。

**编译目标**：单一 pybind11 扩展模块 `sgn_c_engine`，输出到 `build/`。

**指令集要求**：AVX2 + FMA + AVX-VNNI，并启用 OpenMP。

---

## 2. 模块清单

| 模块 | 头文件 | 说明 |
|------|--------|------|
| HC8 神经网络 | `src/hc8_net.h` | 对称量化（scale + offset 128）、int8×int8→int32 矩阵乘、整数 ReLU、扁平存储互转 |
| HC16 神经网络 | `src/hc16_net.h` | int16 有符号直接存储，AVX2 `_mm256_madd_epi16` 高效 int16×int16→int32 累加，精度较 HC8 提升 258× |
| HC16MS 多视角 | `src/hc16ms.h` | 2 字节 int16 容器多视角存储，HC16 / HC8 / HC4 三档精度零拷贝切换，3× 存储压缩 |
| HC4 PSHUFB | `src/hc4_pshufb.h` | 基于 `_mm256_shuffle_epi8` 的 int4×int4→int8 查表乘法 LUT |
| col2im | `src/col2im.h` | im2col 反向操作（累加重叠区域），C→C++ 迁移模板，保留 `extern "C"` 接口 |
| HC8 余积 | `src/hc8_coproduct.h` | HC8 余积运算扩展 |

Python 绑定（pybind11）：`pysgn_net`、`pysgn_hc16`、`pysgn_hc16ms`、`pysgn_hc4_pshufb`、`pysgn_col2im`。

---

## 3. 目录结构

```
v0.2/
├── include/hc/            ← 头文件（ABI 契约）
│   ├── hc.h                 通用基础设施（元数据、错误码、版本）
│   ├── hc8.h / hc16.h       HC8 / HC16 核心类型
│   ├── hc_simd.h            SIMD 批量操作（SGN_USE_SIMD 启用）
│   ├── sgn_macros.h         便利宏（可选）
│   └── sgn.h                统一入口（仅聚合本目录实际存在的头文件）
│
├── src/                   ← 实现文件 + pybind11 绑定
│   ├── hc8_net.c / hc16_net.c / hc16ms.c / hc4_pshufb.c   C 实现
│   ├── col2im_c.c / col2im.cpp / col2im_bindings.cpp      col2im C/C++ 实现
│   ├── hc8_coproduct_bindings.cpp                          HC8 余积绑定
│   └── pysgn_*.cpp                                         pybind11 绑定
│
├── CMakeLists.txt         ← 构建脚本（sgn_c_engine pybind11 模块）
└── LICENSE
```

> 与 v0.1 不同，本版本保持扁平结构（`include/`、`src/` 直接置于版本根目录下）。

---

## 4. 编译

```bash
# 前置：pybind11、OpenMP、支持 AVX2+FMA+AVX-VNNI 的工具链
cmake -S v0.2 -B v0.2/build
cmake --build v0.2/build -j
# 产物：v0.2/build/sgn_c_engine*.so
```

编译选项：

| 宏 | 效果 |
|----|------|
| `SGN_PC_EXTENSION` | CMakeLists 默认开启（=1） |
| `SGN_USE_SIMD` | 启用 `hc_simd.h` 批量操作（需链接 `hc_simd.c`，本版本未提供该 .c） |

---

## 5. 与 v0.1 的关系

- **v0.1**：SGN 主线完整实现，包含 HC8/HC16/HC32/HC64/DC、沙盒、Trie、引擎、存储、网络、插件、C++ 包装、Python 绑定、测试套件。
- **v0.2（本目录）**：从 v0.1 抽取 HC8/HC16 核心，专注神经网络运算扩展（量化矩阵乘、多视角存储、int4/int16 路径、col2im），为 C 语言实现的末尾版本，不再追加新功能。

---

## 6. 许可证

Apache License 2.0 — 详见 [LICENSE](LICENSE)。

```
Copyright (c) 2026 zhugy-8086

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
