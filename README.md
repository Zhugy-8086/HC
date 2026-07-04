# SGN — 超度量数系技术栈

> **版本**: 2.0.0
> **目标平台**: AI8051U (48MHz/34KB SRAM/64KB Flash) 及 PC 端
> **语言**: C11 / C++11

---

## 简介

SGN (Signed Globular Number) 是一套基于超度量空间的自定义数值类型系统。核心思想：用多层字节表示数，每层是独立的进制位，天然构成超度量树结构。

**核心类型**:

| 类型 | 层 × 位宽 | 精度 | 范围 | 用途 |
|------|-----------|------|------|------|
| HC8 | 6 × 8bit | 48bit 小数 | [0, 256) | 嵌入式主力 |
| HC16 | 4 × 16bit | 64bit 小数 | [0, 65536) | 时间戳、高精度 |
| HC32 | 3 × 32bit | 96bit 小数 | [0, 2³²) | PC 端高精度 |
| HC64 | 2 × 64bit | 128bit 小数 | [0, 2⁶⁴) | 预留 |
| DC | - | 十进制定点 | 精确十进制 | 空间变量 |

---

## 目录结构

```
hc/
├── include/hc/               ← 头文件（ABI 契约）
│   ├── hc.h                    通用基础设施（元数据、错误码）
│   ├── hc8.h / hc16.h / hc32.h / hc64.h
│   ├── dc.h                    十进制定点数
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
│   ├── dc.c                    DC 运算
│   ├── hc_simd.c               SIMD 批量操作（SSE2 + 标量回退）
│   ├── hpdc_sandbox.cpp        投影沙盒
│   ├── hpdc_trie.cpp           Trie 索引
│   ├── hpdc_engine.cpp         引擎算法
│   ├── hpdc_storage.cpp        存储与可靠性
│   ├── hpdc_network.cpp        网络与分布式
│   └── hpdc_plugin.cpp         插件系统
│
├── bindings/python/          ← Python 绑定（pybind11）
│   ├── pysgn.cpp
│   ├── setup.py
│   └── example_pysgn.py
├── examples/                 ← C++ 示例
├── tests/                    ← 自动化测试（44 测试 / 156 断言）
│   ├── sgn_test.h              测试框架
│   └── test_sgn.c              数据驱动测试套件
└── docs/                     ← 文档与规范
```

---

## 快速开始

### 嵌入式（按需链接最小集合）

```c
#include "hc/hc16.h"

hc16_t a = hc16_from_double(1000.5, SGN_OVERFLOW_SATURATE);
hc16_t b = hc16_from_double(500.25, SGN_OVERFLOW_SATURATE);
hc16_t c = hc16_add_sat(&a, &b);
double v = hc16_to_double(c);  // 1500.75
```

### 使用便利宏（可选）

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

```bash
# 仅需两个文件
arm-none-eabi-gcc -c -Iinclude src/hc.c -o hc.o
arm-none-eabi-gcc -c -Iinclude src/hc16.c -o hc16.o
```

### PC 端 C++

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

### C++ 便利层（hpdc::op）

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

### 类型安全宏

`hc_value(ptr)` 通过 `_Generic` 在编译期自动推导 HC 类型，消除 `void*` 参数的类型安全隐患：

```c
hc8_t h8 = hc8_from_double(3.14, SGN_OVERFLOW_SATURATE);
double v = hc_value(&h8);  // 编译期自动选择 SGN_HC_KIND_8
```

### Python

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

## 编译与测试

### 使用 GCC

```bash
gcc -std=c11 -O2 -Iinclude -Itests -o tests/test_sgn.exe \
    src/hc.c src/hc8.c src/hc16.c src/hc32.c \
    src/hc64.c src/dc.c src/hc_simd.c tests/test_sgn.c -lm
tests/test_sgn.exe
```

### 使用 TCC

```bash
tcc -O2 -Iinclude -Itests -o tests/test_sgn.exe \
    src/hc.c src/hc8.c src/hc16.c src/hc32.c \
    src/hc64.c src/dc.c src/hc_simd.c tests/test_sgn.c
tests/test_sgn.exe
```

```
=== SGN Automated Test Suite ===

[hc.c]  [hc8.c]  [shc8.c]  [hc16.c]  [hc32.c]  [hc64.c]  [dc.c]  [hc_simd.c]  [bench]  [macros]

Tests: 44 | PASS: 156 | FAIL: 0
ALL PASSED
```

### 编译选项

| 宏 | 效果 |
|----|------|
| `SGN_PC_EXTENSION` | 启用 HC32 沙盒函数、批量操作 |
| `SGN_USE_SIMD` | 启用 SSE2 批量操作（自动标量回退） |
| `-O2` | 编译器优化（推荐） |

### 添加新测试

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

## 模块依赖

```
hc.h ← hc8.h / hc16.h / hc32.h / hc64.h / dc.h
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

## 命名规范

### C API

函数名格式：`{类型}_{操作}[_{变体}]`

```c
hc8_from_double(v, policy)    // 类型_操作
hc8_add_sat(&a, &b)           // 类型_操作_变体
hc8_to_double(h)              // 类型_操作
hc_value(&h)                  // 类型安全宏（_Generic 自动推导）
```

类型名格式：`{类型}_t`

```c
hc8_t, hc16_t, hc32_t, hc64_t    // 无符号
shc8_t                             // 有符号
dc_t                               // 十进制定点
```

### C++ API

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

### C 宏

格式：`{TYPE}_{操作}`

```c
HC8_ADD(a, b)
HC8_FROM_DOUBLE(v)
HC8_SOFT_THRESH(x, l)
```

---

## 已完成的工程改造

| 阶段 | 内容 |
|------|------|
| 问题1 | 8 个 bug 修复（Trie 越界、RS 解码、Lamport 进位、形态学、WTA、Merkle、COBS、median） |
| 阶段1 | C 层按 HC 类型拆分（hc.c/hc8.c/hc16.c/hc32.c/hc64.c/dc.c） |
| 阶段2 | HC16/32/64 全部标量运算补齐（26 个新函数） |
| 阶段3 | 沙盒去重 + int_bits 校验，C++ 包装器 HC16/32 解锁 |
| 阶段4 | RLE 编解码 + 文件 I/O 落地 |
| 阶段5 | SIMD 批量操作（SSE2 + 标量回退） |
| Python | 绑定更新：PyHC32/PyHC64 完整支持（运算符 + 沙盒） |
| 测试 | 自动化测试框架 + 44 个数据驱动测试（156 断言） |
| 编译 | GCC/TCC -O2 验证通过，全部头文件无冲突 |
| 宏 | sgn_macros.h 便利宏（可选组件，简化常见操作） |
| 命名 | 全局命名统一：去掉 `sgn_` 前缀，头文件路径 `sgn/` → `hc/`，宏名 `SGN_HC8_*` → `HC8_*` |
| 便利层 | C++ `hpdc::op` 极简命名空间（add/sub/less/eq/thresh/to/from） |
| 质量 | `hc_physical_value`/`hc_from_double` 泛化为元数据驱动，消除 if-else 链；`shc8_less` 改为逐字段比较；`#pragma pack(1)` 仅作用于 `hc8_t`；新增 `hc_value()` 类型安全宏 |
| 许可证 | Apache License 2.0 |

---

## 规范文档

完整数学规范和工程规范位于 `docs/核心索引/sgn_spec/`：

- **L0_core/** — 9 份核心地基（数学、运算、Trie、引擎、存储、网络、可靠性、配置、索引矩阵）
- **L1_extensions/** — 14 份扩展（Hensel 提升、布隆过滤器、形态学、DFA 等）
- **L2_bridge/** — PyTorch/ONNX 桥接（待完成）

---

## 许可证

Apache License 2.0 — 详见 [LICENSE](LICENSE)。
