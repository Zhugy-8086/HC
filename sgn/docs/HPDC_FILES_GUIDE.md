# HPDC 项目文件总览

> 本文档描述当前项目全部文件的状态、用途与相互关系。  
> 阅读对象：开发者、集成者、绑定层维护者。  
> 文件总数：20 个（见下文清单）。

---

## 一、文件清单（20 个）

按层次分组列出：

### 1. C ABI 核心层（16 个文件）

| # | 文件 | 类型 | 职责 |
|---|------|------|------|
| 1 | `hpdc_core.h` | 头文件 | 数值类型定义（HC8/16/32/64、DC、SHC8、精度/模式/溢出枚举）、常量声明、基础运算接口 |
| 2 | `hpdc_core.cpp` | 实现 | 比较、加减、软阈值、移位掩码、物理值转换、DC 运算、错误码字符串、常量初始化 |
| 3 | `hpdc_sandbox.h` | 头文件 | 投影沙盒 C API：创建/销毁、project/inverse、除法/梯度/缩放、批量操作、方案切换 |
| 4 | `hpdc_sandbox.cpp` | 实现 | 沙盒内部状态管理、HC8/16/32 物理值计算、project/inverse、算术辅助函数、scheme 占位 |
| 5 | `hpdc_trie.h` | 头文件 | 超度量树索引：节点定义、内存池、插入/查找/前缀匹配/候选收集 |
| 6 | `hpdc_trie.cpp` | 实现 | 静态内存池初始化、节点分配、路径查找、模板插入、懒删除、叶子收集 |
| 7 | `hpdc_engine.h` | 头文件 | 推理引擎：WTA 竞争、Hamming 匹配、软衰减、LRU、模板合并、形态学、排序网络、中位数 |
| 8 | `hpdc_engine.cpp` | 实现 | WTA 前缀剪枝+Top-K 选择排序、LRU 命中/衰减/淘汰、形态学占位、bitonic 排序、逐层中位数 |
| 9 | `hpdc_storage.h` | 头文件 | 存储格式：文件头/记录、RLE、Merkle 树、RS(8,6)、CRC8、HC8 校验和、TMR 投票 |
| 10 | `hpdc_storage.cpp` | 实现 | 文件读写占位、精度升降级、RLE 占位、Merkle 哈希更新、RS 编解码（GF256）、CRC8、校验和、TMR |
| 11 | `hpdc_network.h` | 头文件 | 通信协议：UART 帧结构、COBS、Lamport 逻辑时钟、HC16 时间、看门狗条目 |
| 12 | `hpdc_network.cpp` | 实现 | UART 打包/解析（CRC8 校验）、COBS 编解码、Lamport 更新/比较、看门狗超时判断与轮询 |
| 13 | `hpdc_plugin.h` | 头文件 | 插件架构：类型/能力掩码/事件枚举、插件描述符、生命周期回调、管理器 API、外围注册表 |
| 14 | `hpdc_plugin.cpp` | 实现 | 动态库加载（dlopen/LoadLibrary）、能力校验、init/register/shutdown 调用、槽位管理、晋升路径 |
| 15 | `hpdc_normative.h` | 头文件 | **官方内核注册表**：沙盒投影方案注册、HC 运算符注册、精度等级激活、WTA/LRU/Trie 策略替换 |
| 16 | `pysgn.cpp` | 实现 | **Python 绑定**（pybind11）：PyHC8/PyHC16/PyDC/PySandbox/PyTriePool 包装、枚举导出、模块定义、插件 API 暴露 |

### 2. 过渡补丁层（2 个文件）

| # | 文件 | 类型 | 职责 |
|---|------|------|------|
| 17 | `sgn_abi_dynamic.h` | 头文件 | 动态化改进：HC 元数据表（`sgn_hc_meta_t`）、精度等级映射、通用物理值 API、HC32 最小声明、沙盒联动、Trie 动态深度 |
| 18 | `sgn_abi_dynamic.cpp` | 实现 | 元数据表初始化、通用 `sgn_hc_physical_value`（修复 HC16 bug）、通用 `sgn_double_to_hc`、HC32 最小实现、沙盒 int_bits 联动、Trie 深度 |

### 3. C++ 官方包装层（2 个文件）

| # | 文件 | 类型 | 职责 |
|---|------|------|------|
| 19 | `hpdc_cpp.hpp` | 头文件 | **官方 C++ RAII 包装器**：`HC<T>` 模板（零硬编码）、`DC` 类、`Sandbox` 类（自动分派）、`TriePool` 类、类型别名 HC8/HC16/HC32/HC64 |
| 20 | `hpdc_cpp_example.cpp` | 示例 | C++ 使用示例：HC8/HC16 运算、HC32 构造与层识别、Sandbox 投影/除法/梯度/缩放/map/批量、DC 运算、Trie 插入、软阈值 |

> **配套文件**（未计入上述 20 个，但项目中存在）：
> - `setup.py`：Python 模块构建脚本（pybind11）
> - `example_pysgn.py`：Python 使用示例

---

## 二、层次关系图

```
┌─────────────────────────────────────────────────────────────┐
│  应用层                                                       │
│  ─────────────────                                           │
│  pysgn.cpp          ← Python 绑定（pybind11）               │
│  hpdc_cpp.hpp       ← C++ 官方包装器（可选）                 │
│  hpdc_cpp_example.cpp ← C++ 示例                            │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│  扩展注册层（仅插件开发者使用）                                │
│  ─────────────────────────────────                           │
│  hpdc_normative.h   ← 官方内核注册表（投影方案/运算符/策略）    │
│  hpdc_plugin.h/cpp  ← 插件生命周期、动态加载、能力隔离        │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│  功能模块层（按需链接）                                        │
│  ─────────────────────────────────                           │
│  hpdc_sandbox.h/cpp ← 投影沙盒（float ↔ HC 转换）            │
│  hpdc_trie.h/cpp    ← 超度量树索引                           │
│  hpdc_engine.h/cpp  ← WTA、LRU、形态学、排序                 │
│  hpdc_storage.h/cpp ← 文件、RLE、Merkle、RS、CRC、TMR       │
│  hpdc_network.h/cpp ← UART、COBS、Lamport、看门狗           │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│  数值核心层（必须链接）                                        │
│  ─────────────────────────────────                           │
│  hpdc_core.h/cpp    ← HC/DC/SHC8 类型、比较、加减、转换      │
└─────────────────────────────────────────────────────────────┘

【临时补丁层】（位于核心层之上，未来合并回 core）
  sgn_abi_dynamic.h/cpp ← 动态化修复（HC16 bug、HC32 最小实现、硬编码消除）
```

---

## 三、各文件详解

### 3.1 数值核心层：`hpdc_core.h` / `.cpp`

**地位：最底层，无外部依赖。**

`hpdc_core.h` 定义了 HPDC 的全部基础类型：
- `sgn_hc8_t`（6×8 位）、`sgn_hc16_t`（4×16 位）、`sgn_hc32_t`（3×32 位）、`sgn_hc64_t`（2×64 位）
- `sgn_dc_t`（十进制定点数，index/level）
- `sgn_leveled_hpdc8_t`（合体模式，时间变量）
- `sgn_shc8_t`（有符号 HC8）
- 精度/模式/溢出枚举

`hpdc_core.cpp` 实现基础运算：
- 字典序比较（`less`、`equal`）
- 饱和加法、循环加法、合体加法、有符号加法
- 减法（含借位、有符号）
- 软阈值（`max(X-Lambda, 0)`）
- 移位与掩码
- 物理值 ↔ double 转换
- DC 运算（对齐、加减乘、跨精度转换、JSON 序列化）

**已知问题**：
- `hc16_physical_value` 起始 `scale = 1.0/65536.0` 为 bug（应为 `1.0`），导致 HC16 投影结果整体缩小 65536 倍。
- 多处硬编码 `6` 层、`256.0` 基数，HC32/HC64 无法复用同一套逻辑。
- HC32 有类型定义但无任何运算函数实现。

### 3.2 投影沙盒：`hpdc_sandbox.h` / `.cpp`

**地位：规范级组件，依赖 core。**

提供 HC 与 float64 之间的双向转换：
- `project`：物理值 / R（R = 2^int_bits），结果在 [0, 1)
- `inverse`：phi × R，饱和回 HC
- `divide`：任意除数除法（通过 float 中间值）
- `gradient`：H_new = H - eta × grad
- `scale`：H × factor
- `signed_add` / `signed_sub`：有符号运算
- 批量操作（循环实现，预留 SIMD）
- 方案切换占位（`set_scheme`，目前仅支持 "default"）

**已知问题**：
- `int_bits` 与 HC 类型未联动：HC8 配 `int_bits=16` 时，逆投影几乎全部饱和到 `HC8_MAX`。
- HC32 的 project/inverse 已声明但依赖 `SGN_PC_EXTENSION` 宏。

### 3.3 Trie 索引：`hpdc_trie.h` / `.cpp`

**地位：索引层，依赖 core。**

基于 HC 字节层的 256 叉超度量树：
- 静态内存池（2048 节点），零动态分配
- 节点插入、路径查找、前缀匹配
- 模板插入（沿 6 层路径创建节点，叶子存 template_id）
- 懒删除（标记 `0xFFFF`）
- 候选收集（沿查询 HC 前 k 层下降，收集子树所有叶子）

**已知问题**：
- 遍历深度硬编码为 `6`，未适配 HC16（4 层）或 HC32（3 层）。
- 内存池节点数固定 2048，未参数化。

### 3.4 推理引擎：`hpdc_engine.h` / `.cpp`

**地位：算法层，依赖 core + trie。**

- **WTA 竞争**：Trie 前缀剪枝（前 2 层）+ 候选 Hamming 匹配 + 选择排序取 Top-K
- **软衰减**：全局软阈值化计数器数组
- **LRU**：命中递增、全局衰减、最小值淘汰、核心晋升
- **模板合并**：逐字节 OR / AND
- **形态学**：膨胀/腐蚀/开/闭（占位实现，未完整）
- **排序网络**：bitonic 排序（2 的幂用递归，非 2 的幂退化为插入排序）
- **逐层中位数**：频率计数 + 逐层过滤（用于 Quorum 共识）

### 3.5 存储与可靠性：`hpdc_storage.h` / `.cpp`

**地位：持久化层，依赖 core。**

- **文件格式**：23 字节头（magic/version/level/num_records/merkle_root/rs_parity）+ 13 字节记录（mode/sign/int_part/frac[6]/level）
- **精度升降级**：`grade_up`（补零）、`grade_down`（截断）
- **RLE**：前缀深度 + 后缀的压缩项（占位实现）
- **Merkle 树**：完全二叉树数组存储，8 字节哈希，叶子更新后向上重算
- **RS(8,6)**：GF(256) 上的 Reed-Solomon，LFSR 编码，伴随式解码（纠 1 位错）
- **CRC8**：多项式 0x31，初值 0xFF
- **HC8 校验和**：加权字节和（权重 1-7），支持增量更新
- **TMR 投票**：三模冗余，按字节层多数投票，输出错误掩码

### 3.6 网络与分布式：`hpdc_network.h` / `.cpp`

**地位：通信层，依赖 core。**

- **UART 帧**：同步头 `0xAA 0x55` + 类型 + 长度 + 负载 + CRC8
- **COBS**：Consistent Overhead Byte Stuffing，零字节消除
- **Lamport 时钟**：level/sec/ss[4] 四级时间，更新时取 max + 1 滴答
- **看门狗**：8 槽位，HC16 时间格式，超时回调

### 3.7 插件系统：`hpdc_plugin.h` / `.cpp`

**地位：扩展层，依赖 core。**

- **插件类型**：NORMATIVE（内核级）、EXTENSION（外围级）、HYBRID（混合级）
- **能力掩码**：64 位，官方可持 NORMATIVE_MASK，第三方被强制限制为 EXTENSION_MASK
- **生命周期**：LOADED → init() → register() → ACTIVE → shutdown() → 卸载
- **动态加载**：dlopen / LoadLibrary，导出 `sgn_plugin_get_desc`，能力校验，回调调用
- **晋升路径**：ACTIVE → `propose_normative()` → CANDIDATE → Minor 版本冻结 → FROZEN（不可卸载）

### 3.8 官方注册表：`hpdc_normative.h`

**地位：内核扩展接口，仅官方插件包含。**

定义 `sgn_normative_registry_t`，包含函数指针：
- `register_sandbox_converter`：注册新投影方案（如 log_domain、float128）
- `register_hc8_add_op` / `sub_op` / `soft_threshold_op`：覆盖 HC 运算符
- `activate_precision_grade`：激活预留精度（如 FUTURE）
- `register_wta_strategy` / `lru_policy` / `candidate_collector`：替换引擎策略

**注意**：第三方插件禁止包含此头文件。

### 3.9 Python 绑定：`pysgn.cpp`

**地位：语言绑定层，依赖全部 C ABI。**

用 pybind11 暴露：
- `PyHC8` / `PyHC16`：构造、to_float、层访问、运算符、比较、hash、bytes 序列化
- `PyDC`：构造、运算、JSON、跨精度转换
- `PySandbox`：project、inverse、divide、gradient、scale、map、map2、batch、DC 桥梁
- `PyTriePool`：内存池包装
- 枚举导出：Precision、Mode、Overflow、Error、PluginType、MonitorEvent
- 插件 API：load/unload、count、name、capabilities、propose_normative、register_static
- 便利函数：quick_divide、quick_gradient、array_divide、array_scale、soft_threshold、tmr_vote、rs86 编解码等

**当前状态**：`PyHC8`/`PyHC16` 为手写重复实现，与 C++ 层 `hpdc::HC<T>` 逻辑高度重合。未来建议复用 `hpdc_cpp.hpp`。

### 3.10 过渡补丁：`sgn_abi_dynamic.h` / `.cpp`

**性质：临时补丁，非长期组件。**

#### 为什么存在？

`hpdc_core.cpp` 当前有硬编码和 bug，但正式重构需要时间。此补丁作为**过渡方案**，让急需修复的项目可以先用，同时不破坏原有 ABI 接口。

#### 提供的内容

- `sgn_hc_meta_t` 元数据表：描述 HC8/16/32/64 的层数、位数、基数、上限
- `sgn_hc_physical_value()`：通用物理值计算，修复 HC16 的 scale bug
- `sgn_double_to_hc()`：通用饱和转换，上限自动查表
- `sgn_hc32_to_double` / `sgn_double_to_hc32` / `sgn_hc32_less` / `sgn_hc32_equal`：HC32 最小实现，解决链接报错
- `sgn_sandbox_default_int_bits()` / `sgn_sandbox_int_bits_valid()`：沙盒与 HC 类型联动
- `sgn_trie_depth_for_hc()`：Trie 动态深度（HC8→6, HC16→4, HC32→3）
- 兼容层：`sgn_hc8_to_double`、`sgn_hc16_to_double` 等旧接口用新 API 重新实现

#### 生命周期

**短期**。当 `hpdc_core.cpp` 完成以下工作后，本补丁将被合并并删除：
1. 硬编码全部替换为查表/参数化
2. HC16 bug 修复
3. HC32 运算函数补齐
4. Trie 深度、校验和权重、文件记录格式等动态化

**使用建议**：
- 稳定项目：等待 `hpdc_core` 正式更新，不引入本补丁。
- 急需修复 HC16 或试用 HC32：临时链接 `sgn_abi_dynamic.cpp`，后续迁移到正式版。

### 3.11 C++ 包装器：`hpdc_cpp.hpp`

**性质：长期组件，正式接口。**

为 C++ 用户提供的官方 RAII 包装，设计目标：
- **零硬编码**：`HC<T>` 的层数、位数从 C 结构体自动推导（`sizeof(v)/sizeof(v[0])`）
- **模板统一**：HC8/16/32/64 共用一套 `HC<T>` 代码
- **自动分派**：`Sandbox::project(a)` 根据 `a` 的类型自动调用 `project_hc8` 或 `project_hc16`
- **RAII 安全**：`Sandbox` 自动创建/销毁；禁用拷贝，支持移动

#### 关键类

| 类 | 说明 |
|----|------|
| `hpdc::HC<RawT>` | 通用 HC 包装。`HC8 = HC<sgn_hc8_t>`，`HC16 = HC<sgn_hc16_t>`。层访问边界检查自动用推导的 `layers`。 |
| `hpdc::DC` | 十进制定点数。`+`、`-`、`*`、自动对齐、`to_json()`。 |
| `hpdc::Sandbox` | 沙盒包装。`project`、`inverse`、`divide`、`gradient`、`scale`、`map`、`map2`、批量操作、DC 桥梁。 |
| `hpdc::TriePool` | Trie 内存池包装。`insert()` 简化接口。 |

#### 与补丁层的关系

**不依赖** `sgn_abi_dynamic.h`。只依赖正式的 `hpdc_core.h` + `hpdc_sandbox.h` + `hpdc_trie.h`。

- HC8/HC16：直接调用已有 C API（`sgn_hc8_add_saturated`、`sgn_hc16_less` 等）。
- HC32：目前只能构造和 `to_float()`（通用推导），`+`/`-`/`<` 被 `static_assert` 禁用，等待 C 层补齐 `sgn_hc32_add_saturated` 等函数后自动解锁。

#### 未来扩展

C 层加了 `sgn_hc128_t` 后，只需在 `hc_api_traits` 中加 8-10 行特化，`HC128` 立即全功能可用，`HC<T>` 模板本身一行不改。

### 3.12 C++ 示例：`hpdc_cpp_example.cpp`

演示 `hpdc_cpp.hpp` 的完整用法：
- HC8/HC16 的构造、运算、比较、层访问、序列化
- HC32 的构造与层数自动识别（运算待解锁）
- Sandbox 的投影、除法、梯度、缩放、泛型 `map`、批量操作
- DC 的构造、加减、JSON、与 HC 互转
- TriePool 的插入操作
- 软阈值

编译命令：
```bash
g++ -std=c++11 hpdc_cpp_example.cpp -I. \
    hpdc_core.cpp hpdc_sandbox.cpp hpdc_trie.cpp \
    -o example && ./example
```

---

## 四、依赖关系

```
hpdc_cpp.hpp
    ├── hpdc_core.h
    ├── hpdc_sandbox.h
    └── hpdc_trie.h

sgn_abi_dynamic.h / .cpp
    └── hpdc_core.h

pysgn.cpp
    ├── hpdc_core.h
    ├── hpdc_sandbox.h
    ├── hpdc_trie.h
    ├── hpdc_engine.h
    ├── hpdc_storage.h
    ├── hpdc_network.h
    ├── hpdc_plugin.h
    └── hpdc_normative.h
    # 未来可引入 hpdc_cpp.hpp，减少 PyHC8/PyHC16 的重复手写代码
```

---

## 五、使用场景速查

| 场景 | 需要链接的文件 | 备注 |
|------|---------------|------|
| **MCU 嵌入式（纯 C）** | `hpdc_core.o` + 按需 `hpdc_trie.o` / `hpdc_engine.o` | 不链接沙盒、网络、插件 |
| **PC 端 C++ 项目** | `hpdc_core.cpp` + `hpdc_sandbox.cpp` + `hpdc_trie.cpp` + `hpdc_cpp.hpp` | 首选官方 C++ 包装器 |
| **急需 HC16 bug 修复** | 上述文件 + `sgn_abi_dynamic.cpp` | 临时补丁，后续迁移 |
| **Python 模块构建** | `setup.py` 中列出的全部 `.cpp` | pybind11 编译 |
| **插件开发** | `hpdc_plugin.h` + `hpdc_normative.h`（官方）/ `hpdc_plugin.h`（第三方） | 按类型选择注册表 |

---

## 六、未来演进

| 阶段 | 目标 | 对文件的影响 |
|------|------|-------------|
| **近期** | `hpdc_core.cpp` 完成动态化重构 | `sgn_abi_dynamic.h/cpp` **合并并删除**；硬编码全部消除；HC16 bug 修复 |
| **中期** | C 层补齐 HC32/HC64 运算 | `hpdc_cpp.hpp` 中 `HC32`/`HC64` 自动解锁 `+`、`-`、`<` |
| **远期** | Python 绑定复用 C++ 层 | `pysgn.cpp` 引入 `hpdc_cpp.hpp`，`PyHC8`/`PyHC16` 大幅瘦身 |

---

*本文档随项目迭代更新，如有疑问请参考各头文件顶部的 `@version` 标记。*
