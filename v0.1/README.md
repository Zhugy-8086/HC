# SGN — 超度量数系技术栈

> **版本**: v0.1
> **语言**: C11 / C++11（含 Python 绑定）
> **许可证**: Apache License 2.0

---

## 1. 简介

SGN (Signed Globular Number) 是一套基于超度量空间的自定义数值类型系统。核心思想：用多层字节表示数，每层是独立的进制位，天然构成超度量树结构。

**核心 HC 类型**（本仓库主线）：

| 类型 | 层 × 位宽 | 精度 | 范围 | 用途 |
|------|-----------|------|------|------|
| HC8  | 6 × 8bit  | 48bit 小数 | [0, 256) | 嵌入式主力 |
| HC16 | 4 × 16bit | 64bit 小数 | [0, 65536) | 时间戳、高精度 |
| HC32 | 3 × 32bit | 96bit 小数 | [0, 2³²) | PC 端高精度 |
| HC64 | 2 × 64bit | 128bit 小数 | [0, 2⁶⁴) | 高精度扩展 |

**DC 类型**（HC 扩展，非独立 ABI）：

| 类型 | 结构 | 物理值 | 当前定位 |
|------|------|--------|---------|
| `dc_t` | `(int64_t index, uint32_t level)` | `index / 10^level` | HC 与十进制世界互转的桥接扩展 |

---

## 2. HC 的本职工作与扩展能力

HC 的能力分两层。**本职工作**是 HC 存在的根本理由；**扩展能力**围绕 HC 字节层构建，按需链接，不被本职工作拖累。

### 2.1 本职工作（HC 之所以存在）

| 职责 | 说明 | 关键 API |
|------|------|---------|
| **作为时间变量** | HC 在 SGN 中以合体模式 `level × 256 + HC` 担任时间变量本身，提供时序高精度小数部分 | `leveled_hpdc8_t`、`leveled_add` |
| **精确记录** | 模型文件、模板库、时序存档——任何要求跨平台字节级一致、可压缩、可校验的持久化场景 | 13 字节固定记录、Merkle/RS/CRC |
| **超度量比较与索引** | 字典序即超度量距离，Trie 前缀即超度量球，天然支持层次聚类与最近邻 | `hc8_less` / `hc8_equal`、Trie |
| **基础数值运算** | 加法（饱和/回绕/合体进位）、减法、软阈值、移位、校验和 | `hc8_add_sat` / `hc8_sub` / `hc8_soft_threshold` |

### 2.2 HC 能干什么（扩展能力，按需链接）

| 模块 | 能力 | 典型场景 |
|------|------|---------|
| **Trie 索引** | 256 叉超度量前缀树，前缀剪枝 + 候选收集 | 模板库快速匹配、超度量聚类 |
| **WTA 引擎** | Trie 前缀剪枝 + Hamming 匹配 + Top-K 选择 | 竞争学习、最近模板 |
| **LRU 引擎** | 命中递增 + 全局衰减 + 最小值淘汰 + 核心晋升 | 模板库容量管理 |
| **形态学** | 膨胀/腐蚀/开/闭（基于 Trie 邻域） | 模板邻域扩展、噪声去除 |
| **排序网络** | bitonic 排序（2 的幂递归，非 2 的幂退化） | 大规模模板排序 |
| **存储可靠性** | RLE 压缩、Merkle 树、RS(8,6) 纠错、CRC8、HC 校验和、TMR 三模冗余 | 模型文件、Flash 容错、防篡改 |
| **网络分布式** | UART 帧、COBS 零字节消除、Lamport 时钟、HC16 看门狗 | MCU 间同步、分布式事件排序 |
| **插件系统** | NORMATIVE/EXTENSION/HYBRID 三类插件、动态加载、能力掩码 | 实验功能隔离、第三方扩展 |
| **投影沙盒** | HC ↔ float64/float128 双向转换、除法/梯度/缩放 | PC 端数值实验 |
| **SIMD 批量** | SSE2 加速批量加法/比较/软阈值（标量回退） | PC 端高吞吐 |

> 嵌入式项目可只链接 HC 本职（`hc.c` + `hc8.c`），不引入任何扩展模块；PC 端按需追加 `hpdc_trie.cpp` / `hpdc_engine.cpp` / `hpdc_storage.cpp` 等。

---

## 3. HC 与 DC 是两个相互不同的 ABI

### 3.1 关系定位

```
┌─────────────────────────────────────────┐
│  HC ABI（本仓库主线）                    │
│  - 字节层数组 (int_part, frac[])         │
│  - 超度量空间、Trie 索引                 │
│  - 时间变量承载、精确记录                │
└─────────────────────────────────────────┘
              ↕ 桥接扩展（非独立 ABI）
┌─────────────────────────────────────────┐
│  dc.h / dc.c（本仓库内的「DC 扩展」）    │
│  - 十进制定点 (index, level)             │
│  - 与 double / HC8 互转                  │
│  - 不构成独立 DC ABI                     │
└─────────────────────────────────────────┘
```

### 3.2 为什么当前 `dc.h` / `dc.c` 不是真正的 DC

本仓库内的 `dc_t` 设计目标是「让 HC 能与十进制世界互转」，它的存在完全依附于 HC：

- `dc_to_hc8` / `hc8_to_dc`：与 HC8 互转，是 HC 的桥接扩展
- `dc_to_double` / `dc_from_double`：经过十进制与浮点世界沟通
- `dc_serialize`：JSON 序列化，服务于 HC 模型文件描述

它**没有**独立的 DC 运算体系（无 DC 自身的 Trie、WTA、Merkle 等），**没有**独立的 DC 存储格式，**没有**独立的 DC ABI 契约。因此：

> 当前仓库的 `dc.h` / `dc.c` 应被视为 HC 的扩展模块，而不是 DC ABI 的实现。

### 3.3 SGN 视角下的 HC 与 DC

- **SGN 的时间变量**：HC 在 SGN 中以合体模式 `level × 256 + HC` 作为时间变量本身（HC 提供 256 叉小数层，level 提供整数范围）。
- **SGN 的空间变量**：属于 SGN，使用纯整数 level 体系，与 HC 小数层无关。
- **HC 与 DC**：是数值表示层的两个对偶 ABI，HC 是字节进制的超度量路径，DC 是十进制的阿基米德路径。HC 在 SGN 中作为时间变量，不强制使用 DC。

---

## 4. 目录结构

```
hc/
├── include/hc/               ← 头文件（ABI 契约）
│   ├── hc.h                    通用基础设施（元数据、错误码、版本）
│   ├── hc8.h / hc16.h / hc32.h / hc64.h
│   ├── dc.h                    DC 扩展（HC 桥接，非独立 ABI）
│   ├── hc_simd.h               SIMD 批量操作
│   ├── sgn_macros.h            便利宏（可选，简化常见操作）
│   ├── sgn.h                   统一入口（包含全部）
│   ├── hpdc_core.h             向后兼容转发
│   ├── hpdc_cpp.hpp            C++ RAII 包装器 + op 便利层
│   ├── hpdc_sandbox.h          投影沙盒（带 int_bits 校验）
│   ├── hpdc_trie.h             超度量树索引
│   ├── hpdc_engine.h           WTA/LRU/形态学/排序
│   ├── hpdc_storage.h          文件/Merkle/RS/CRC/TMR
│   ├── hpdc_network.h          UART/COBS/Lamport/看门狗
│   ├── hpdc_plugin.h           插件架构
│   └── hpdc_normative.h        官方内核注册表
│
├── src/                      ← 实现文件
│   ├── hc.c                    通用（元数据表、版本、通用转换）
│   ├── hc8.c / hc16.c / hc32.c / hc64.c
│   ├── dc.c                    DC 扩展实现（HC 桥接）
│   ├── hc_simd.c               SIMD 批量操作（SSE2 + 标量回退）
│   ├── hpdc_sandbox.cpp        投影沙盒
│   ├── hpdc_trie.cpp           Trie 索引
│   ├── hpdc_engine.cpp         引擎算法
│   ├── hpdc_storage.cpp        存储与可靠性
│   ├── hpdc_network.cpp        网络与分布式
│   └── hpdc_plugin.cpp         插件系统
│
├── bindings/python/          ← Python 绑定（pybind11）
├── examples/                 ← C++ 示例
└── tests/                    ← 自动化测试
```

---

## 5. 快速开始

### 5.1 C（嵌入式最小集合）

```c
#include "hc/hc16.h"

hc16_t a = hc16_from_double(1000.5, SGN_OVERFLOW_SATURATE);
hc16_t b = hc16_from_double(500.25, SGN_OVERFLOW_SATURATE);
hc16_t c = hc16_add_sat(&a, &b);
double v = hc16_to_double(c);  // 1500.75
```

```bash
# 仅需两个文件
arm-none-eabi-gcc -c -Iinclude src/hc.c -o hc.o
arm-none-eabi-gcc -c -Iinclude src/hc16.c -o hc16.o
```

### 5.2 使用便利宏（可选）

```c
#include "hc/hc8.h"
#include "hc/sgn_macros.h"

hc8_t a = HC8_FROM_DOUBLE(3.14);
hc8_t b = HC8_FROM_DOUBLE(2.0);
hc8_t c = HC8_ADD(a, b);       // 5.14
double v = HC8_TO_DOUBLE(c);

if (HC8_LESS(a, b)) { ... }        // 比较
hc8_t st = HC8_SOFT_THRESH(a, b);  // 软阈值
```

**可用宏**:

| 类别 | HC8 | HC16 | HC32 | HC64 |
|------|-----|------|------|------|
| 转换 | `HC8_FROM_DOUBLE` | `HC16_FROM_DOUBLE` | `HC32_FROM_DOUBLE` | `HC64_FROM_DOUBLE` |
| 输出 | `HC8_TO_DOUBLE` | `HC16_TO_DOUBLE` | `HC32_TO_DOUBLE` | `HC64_TO_DOUBLE` |
| 加法 | `HC8_ADD` | `HC16_ADD` | `HC32_ADD` | `HC64_ADD` |
| 减法 | `HC8_SUB` | `HC16_SUB` | `HC32_SUB` | `HC64_SUB` |
| 比较 | `HC8_LESS` / `HC8_EQUAL` | `HC16_LESS` / `HC16_EQUAL` | `HC32_LESS` / `HC32_EQUAL` | `HC64_LESS` / `HC64_EQUAL` |
| 形态学 | `HC8_SOFT_THRESH` | `HC16_SOFT_THRESH` | `HC32_SOFT_THRESH` | `HC64_SOFT_THRESH` |

> 嵌入式项目如需最小体积，可不包含 `sgn_macros.h`，不影响核心功能。

### 5.3 PC 端 C++

```cpp
#include "hc/hpdc_cpp.hpp"
using namespace hpdc;

HC8 a(3.14), b(2.72);
HC8 c = a + b;
HC32 big(123456789.0);
HC32 sum = big + HC32(1.0);

Sandbox sb;
double phi = sb.project(a);
HC8 q = sb.divide(a, b);
```

### 5.4 C++ 便利层（hpdc::op）

极简命名空间，类似 Sandbox 的 `map`/`map2` 体验：

```cpp
#include "hc/hpdc_cpp.hpp"
using namespace hpdc::op;

HC8 a(3.14), b(2.72);
auto c = add(a, b);      // 饱和加法，7 字符
bool ok = less(a, b);     // 比较
HC8 st = thresh(a, b);    // 软阈值，6 字符
double v = to(a);         // 转 double，5 字符
HC8 h = from<HC8>(3.14);  // 从 double 创建
```

| 函数 | 等价 C API | 字符数 |
|------|-----------|--------|
| `add(a, b)` | `hc8_add_sat(&a, &b)` | 7 vs 16 |
| `sub(a, b)` | `hc8_sub(&a, &b)` | 7 vs 13 |
| `less(a, b)` | `hc8_less(&a, &b)` | 9 vs 15 |
| `eq(a, b)` | `hc8_equal(&a, &b)` | 7 vs 15 |
| `thresh(x, l)` | `hc8_soft_threshold(&x, &l)` | 7 vs 26 |
| `to(v)` | `hc8_to_double(v)` | 5 vs 15 |
| `from<T>(v)` | `hc8_from_double(v, ...)` | 10 vs 25 |

### 5.5 类型安全宏

`hc_value(ptr)` 通过 `_Generic` 在编译期自动推导 HC 类型，消除 `void*` 参数的类型安全隐患：

```c
hc8_t h8 = hc8_from_double(3.14, SGN_OVERFLOW_SATURATE);
double v = hc_value(&h8);  // 编译期自动选择 SGN_HC_KIND_8
```

### 5.6 Python

```bash
cd bindings/python
pip install pybind11
python setup.py build_ext --inplace
```

```python
import pysgn

a = pysgn.HC8(3.14)
b = pysgn.HC8(2.72)
print(a + b)            # HC8(5.86...)

c = pysgn.HC32(123456789.0)
print(c + pysgn.HC32(1))  # HC32(123456790.0)

sb = pysgn.Sandbox(pysgn.Precision.STD, 8)
phi = sb.project(a)
inv = sb.inverse(phi)
```

---

## 6. 编译与测试

### 6.1 使用 GCC

```bash
gcc -std=c11 -O2 -Iinclude -Itests -o tests/test_sgn.exe \
    src/hc.c src/hc8.c src/hc16.c src/hc32.c \
    src/hc64.c src/dc.c src/hc_simd.c tests/test_sgn.c -lm
tests/test_sgn.exe
```

### 6.2 使用 TCC

```bash
tcc -O2 -Iinclude -Itests -o tests/test_sgn.exe \
    src/hc.c src/hc8.c src/hc16.c src/hc32.c \
    src/hc64.c src/dc.c src/hc_simd.c tests/test_sgn.c
tests/test_sgn.exe
```

### 6.3 编译选项

| 宏 | 效果 |
|----|------|
| `SGN_PC_EXTENSION` | 启用 HC32 沙盒函数、批量操作 |
| `SGN_USE_SIMD` | 启用 SSE2 批量操作（自动标量回退） |
| `-O2` | 编译器优化（推荐） |

### 6.4 添加新测试

```c
#include "sgn_test.h"

TEST(my_new_test) {
    hc8_t a = hc8_from_double(1.0, SGN_OVERFLOW_SATURATE);
    hc8_t b = hc8_from_double(2.0, SGN_OVERFLOW_SATURATE);
    hc8_t c = hc8_add_sat(&a, &b);
    ASSERT_NEAR(hc8_to_double(c), 3.0, 0.01);
}

// 在 main() 中添加:
RUN(my_new_test);
```

---

## 7. 模块依赖

```
hc.h ← hc8.h / hc16.h / hc32.h / hc64.h / dc.h（HC 扩展）
         ↓
sgn_macros.h（可选）
         ↓
hpdc_sandbox.h  ←  hpdc_trie.h  ←  hpdc_engine.h
         ↓                              ↓
hpdc_storage.h    hpdc_network.h    hpdc_plugin.h
```

嵌入式项目仅链接所需模块，不被无关代码拖累。
`sgn_macros.h` 为可选组件，不包含不影响核心功能。

---

## 8. 命名规范

### 8.1 C API

函数名格式：`{类型}_{操作}[_{变体}]`

```c
hc8_from_double(v, policy)    // 类型_操作
hc8_add_sat(&a, &b)           // 类型_操作_变体
hc8_to_double(h)              // 类型_操作
hc_value(&h)                  // 类型安全宏（_Generic 自动推导）
```

类型名格式：`{类型}_t`

```c
hc8_t, hc16_t, hc32_t, hc64_t    // 无符号 HC
shc8_t                             // 有符号 HC
dc_t                               // 十进制定点（HC 扩展，非独立 ABI）
```

### 8.2 C++ API

命名空间 `hpdc`，类型别名 + 运算符重载：

```cpp
HC8 a(3.14), b(2.72);
HC8 c = a + b;        // 运算符
double v = a.to_float();
```

便利层 `hpdc::op`，极简自由函数：

```cpp
using namespace hpdc::op;
auto c = add(a, b);
```

### 8.3 C 宏

格式：`{TYPE}_{操作}`

```c
HC8_ADD(a, b)
HC8_FROM_DOUBLE(v)
HC8_SOFT_THRESH(x, l)
```

---

## 9. 许可证

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
