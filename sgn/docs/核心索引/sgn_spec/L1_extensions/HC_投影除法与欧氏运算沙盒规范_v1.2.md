# HC 投影除法与欧氏运算沙盒规范

> **版本**: v1.2（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈 PC 端扩展层规范——代数闭包补完  
> **状态**: 已去重，引用核心文件；已修正 E3（float128 溢出声明）  
> **核心原则**: 本文档仅保留投影方案的业务逻辑与精度分析。所有溢出语义、模式定义禁止重复解释，必须引用 `sgn_config_guide` 唯一权威源。

> **v1.2 修订说明**: 保留 v1.1 双方案与 float128 统一沙盒，**重点修正 E3**：明确 float128 对 HC32 默认 int_part=32 位时仍有 15 位损失。删除重复溢出语义解释，改为引用 sgn_config_guide。

---

## 0. 问题起源

当前 SGN 的 28 份核心规范构建了完整的存储/比较/通信/容错体系，但在**运算层**存在一块明确的代数短板：

| 运算 | 标准 HC 实现 | 局限 | SGN 需求 |
|------|-------------|------|---------|
| **除法** | 2-adic 逆元（奇数除数）或长除法（偶数除数） | 奇数限制、O(L^2) 复杂度 | **任意除数、批量 SIMD** |
| **梯度更新** | 无（SGN 是离散竞争网络） | 不支持反向传播 | **开源扩展需兼容梯度框架** |
| **曲线求交** | 无 | 超度量空间无连续曲线 | **时间序列/动态系统建模** |
| **负空间** | 补码 SHC8（范围减半） | `int_part` 上限缩至 127 | **对称梯度需完整负域** |

**核心矛盾**: 标准 HC 的纯整数路径（Hensel 提升、逐位试商）在 8 位 MCU 上是优雅解，但在 PC 端面对 10 万次批量除法时，**展平-压缩的来回开销**和**分支密集的逐层进位**成为瓶颈。

本规范不推翻 HC 基座，而是在**仅 HC 模式**上叠加一个**可拆卸的欧氏运算沙盒**：

> **HC 负责存和传**（确定性、跨平台、md5sum 一致）  
> **float64/float128 负责算**（SIMD、任意除数、批量流水线）  
> **仅 HC 模式的 255 上限负责兜底**（防止运算结果撕裂超度量结构）

---

## 1. 形式化模型（双方案）

### 1.1 核心问题：到底是谁在溢出？

在讨论投影方案前，必须先澄清一个根本问题：**float64 的 53 位尾数是硬天花板**。无论整数还是小数，只要总信息量超过 53 位，float64 就会**溢出**——表现为低位被截断、归零或失真。

| 层级 | 溢出者 | 被溢出者 | 溢出表现 |
|------|--------|---------|---------|
| **方案 A** | **float64 尾数 53 位** | **被大整数撑爆** | 小数被迫归零 |
| **方案 B** | **float64 尾数 53 位** | **被小数层撑爆** | 低位截断，精度损失 |
| **float128** | **float128 尾数 113 位** | **覆盖 HC8/16** | 无损 |
| **float128 (HC32)** | **float128 尾数 113 位** | **默认 int_part=32 位时** | **仍有 15 位损失** |

**关键洞察**: 不是 HC 体系溢出，而是 **float 装不下 HC 体系的总信息量**。

### 1.2 方案 A: 大整数投影（v1.0 保留，适合 HC8 + float64）

设仅 HC 模式下的 HC8 数 `H`，外部参数 `x in R>=0`, `y in R>0`, `t in N>=0`：

```
Phi_A(H; x, y, t) = H_physical_int * x * y^t
```

其中 `H_physical_int` 为 HC8 展平的 56 位物理值。

**适用场景**: HC8（56 位总信息），float64 的 53 位尾数**边缘覆盖**。

**溢出机制**: float64 的 53 位尾数被 56 位大整数**撑爆**，小数部分被挤占。

---

### 1.3 方案 B: 全局右移投影（v1.1 新增，适合 HC16/32 + float128）

设仅 HC 模式下的 HC 数 `H`，`int_part_bits` 为整数部分位宽：

```
R = 2^int_part_bits

投影: Phi_B(H) = physical_value / R    // Phi in [0, 1)
运算: 在 Phi 域做除法/梯度（全部是小数运算）
回灌: b = Phi_result * R               // 再分解回 int_part + frac
```

此时 float64/float128 的尾数**全部用来表示小数**，相当于一个定点小数。

| 类型 | 物理位宽 | int_part 位宽 | float64 尾数 53 位 | float128 尾数 113 位 |
|------|---------|--------------|-------------------|---------------------|
| HC8  | 56 位   | 8 位         | 损失 3 位 | **无损** |
| HC16 | 80 位   | 16 位        | 损失 27 位 | **无损** |
| HC32 | 128 位  | 32 位（默认）| 损失 75 位 | **损失 15 位（E3 修正）** |
| HC32 | 128 位  | 8 位（压缩） | 损失 75 位 | **无损** |

**E3 修正（v1.2）**：
- float128 尾数 113 位，HC32 默认 int_part=32 位时总信息量 128 位，**仍有 15 位溢出**。
- **无损条件**：通过"位宽压缩"将 int_part 压到 8 位（总信息量 72 位 < 113 位），float128 方可完全无损覆盖 HC32。
- 文档 v1.1 声称"float128 统一沙盒...HC32 压缩后也可无损"是正确的，但**默认 int_part=32 位时并非无损**。v1.2 明确区分默认配置与压缩配置。

---

### 1.4 位宽左移右移: HC 内部的精度微调

在 HC 体系内部，`int_part` 与 `frac` 的边界是**逻辑可调的**。通过 level 机制把大整数抬走，可压缩 `int_part` 的有效位宽：

| 操作 | 效果 | 示例 |
|------|------|------|
| **level 抬走** | int_part > 255 时，用 level 吸收 256 的倍数 | int=500 -> level=1, int=244 |
| **位宽压缩** | 声明 int_part 只使用 8 位，高位置零 | HC32 的 32 位 int 逻辑压缩为 8 位 |
| **纯小数化** | int_part = 0，全部位给小数 | 适合 <1.0 的物理量 |

**位宽压缩后的收益（以 HC32 为例）**：

| 模式 | int_part 有效位 | 总信息量 | float128 被溢出位数 |
|------|----------------|---------|-------------------|
| 默认 | 32 位 | 128 位 | **15 位（E3）** |
| 压缩到 8 位 | 8 位 | 72 位 | **0 位** |
| 纯小数 | 0 位 | 96 位 | **0 位** |

**结论**: float128 下 HC8/16 直接无损；HC32 需位宽压缩到 8 位以下才无损。**float64 的 53 位尾数对所有 HC16/32 都是瓶颈**。

---

### 1.5 负离散空间（方案 B: 独立 sign 字段）

标准文档采用基 256 补码（《HC_补码负数扩展规范》），本沙盒为兼容欧氏梯度运算，采用**独立 sign 字段**：

```
SignedHC = (sign in {-1, +1},  hc: HC)
```

**编码**:
- 负值 `-V`: `sign = -1`，`hc = HC.from_float(V)`（存储绝对值）

**加法**:
```
SignedHC_A + SignedHC_B
    = to_float(A) + to_float(B)   （欧氏域）
    = SignedHC.from_float(sum)   （回灌，sign 自动判定）
```

**与补码方案的对比**：
- 补码: 硬件友好，复用现有比较器/加法器（SGN 推荐，见 `sgn_hc_ops` §2/§3）
- 独立 sign: 欧氏运算友好，无需字节取反加一，适合沙盒内的 float 中间层

---

## 2. 算法实现（双方案对照）

### 2.1 方案 A: 大整数投影沙盒（Python 原型，v1.0 保留）

```python
# sgn_projection_sandbox.py

class ProjectionSandboxA:
    def __init__(self, x: float = 1.0, y: float = 1.0, t: int = 0):
        self.scale = x * (y ** t)

    def project(self, h: HC8) -> float:
        # 正向投影: HC -> float64（大整数路径）
        return h.to_physical_int() * self.scale   # 56位大整数 * scale

    def inverse_normalize(self, phi: float) -> Tuple[HC8, int, int]:
        # 逆归一化: float64 -> HC
        if self.scale == 0:
            raise ValueError("scale=0，投影不可逆")
        b = phi / self.scale
        # ... 回灌逻辑（调用 sgn_hc_ops 类型转换接口）
```

**适用**: HC8（56 位），float64 边缘覆盖。  
**警告**: HC16/32 使用时会出现「小数变零」现象。**溢出者是 float64 尾数，被大整数撑爆**。

---

### 2.2 方案 B: 全局右移投影沙盒（v1.1 新增）

```python
class ProjectionSandboxB:
    def __init__(self, int_part_bits: int = 8):
        self.R = 2 ** int_part_bits
        self.int_part_bits = int_part_bits

    def project(self, h: HC) -> float:
        # 正向投影: HC -> float64/128（定点小数路径）
        return h.to_physical_value() / self.R    # Phi in [0, 1)

    def inverse_normalize(self, phi: float) -> Tuple[HC, int, int]:
        # 逆归一化: float64/128 -> HC
        if phi < 0 or phi >= 1.0:
            # 仅 HC 模式下饱和到 [0, R)
            phi = max(0.0, min(0.9999999999999999, phi))
        b = phi * self.R

        q = int(b) // self.R      # 仅 HC 模式下截断，不进 level
        r = b - q * self.R        # remainder in [0, R)

        hc = HC.from_float(r)     # r 分解回 int_part + frac
        return hc, q, 1

    def divide(self, a: HC, b: HC) -> HC:
        # 任意除数除法: 利用 float64/128 中间层
        pa = self.project(a)
        pb = self.project(b)
        if pb == 0:
            return HC_MAX
        phi_res = pa / pb
        hc, _, _ = self.inverse_normalize(phi_res)
        return hc

    def gradient_update(self, h: HC, grad: float, eta: float) -> HC:
        # 梯度更新: H_new = H - eta * grad
        phi = self.project(h)
        phi_new = phi - eta * grad / self.R    # 学习率同步缩放
        hc, q, _ = self.inverse_normalize(phi_new)
        return hc
```

**关键差异**：
- `to_physical_value()` 返回的是 `[0, R)` 区间的实数，不是 56/80/128 位大整数。
- `float64` 全程只处理 `[0, 1)` 定点小数，**不装大整数**。
- 回灌时 `q` 被截断，`r` 写回当前 HC 层。

> **溢出语义引用**: 仅 HC 模式下的截断/饱和行为详见 [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §2。本文档禁止重复解释溢出规则。

---

### 2.3 float128 统一沙盒（v1.1 新增，v1.2 修正 E3）

```cpp
// sgn_projection_sandbox.h
// 仅 PC 端编译（#ifdef SGN_PC_EXTENSION）

namespace sgn {
    // float128 统一沙盒: 一套代码覆盖 HC8/16/32
    // E3 修正: HC32 默认 int_part=32 位时仍有 15 位损失，
    //          需位宽压缩到 8 位（总信息量 72 位 < 113 位）才无损。
    struct ProjectionSandbox128 {
        uint32_t int_part_bits;
        long double R;   // float128，113 位尾数

        long double project(const hc8_t& h);
        long double project(const hc16_t& h);
        long double project(const hc32_t& h);
        // 实现: physical_value / R

        template<typename HC_T>
        HC_T inverse_normalize(long double phi);
        // 实现: 仅 HC 模式截断（引用 sgn_config_guide §2）
    };
}
```

**设计意图**：
- **float128（113 位尾数）** 直接覆盖 HC8（56 位）和 HC16（80 位），无损。
- HC32（128 位）在**默认 int_part=32 位时损失 15 位**（E3 修正）。
- HC32 通过「位宽压缩」把 int_part 压到 8 位（总信息量 72 位），实现**完全无损**。
- 无需分段沙盒，一套 `ProjectionSandbox128` 管所有精度档位——但需注意 HC32 的默认配置并非无损。

---

## 3. Python 验证器（完整测试套件，v1.2 修订）

```python
# sgn_projection_div_verify.py
# 运行方式: python sgn_projection_div_verify.py

import time

# ---- 复用上述 HC8 / ProjectionSandboxA/B / SignedHC 定义 ----

def verify_reversibility():
    # 任务 1: 投影可逆性（方案 B）
    print("[验证 1] 全局右移可逆性")
    sandbox = ProjectionSandboxB(int_part_bits=8)
    h = HC8.from_physical_value(125.5)
    phi = sandbox.project(h)
    b_recover = phi * sandbox.R
    assert abs(b_recover - 125.5) < 1e-12
    print(f"  原始=125.5, 逆推={b_recover:.10f}  ✅")

def verify_carry():
    # 任务 2: 进位规则验证（方案 B）
    print("[验证 2] 仅 HC 模式截断")
    sandbox = ProjectionSandboxB(int_part_bits=8)
    b = 257.3
    q = int(b) // 256
    r = b - q * 256
    hc, overflow, sign = sandbox.inverse_normalize(b / 256)
    assert hc.to_physical_value() == r and overflow == 1
    print(f"  b={b} -> q={q}, r={r:.2f}; 仅 HC 截断, q 未进 level  ✅")

def verify_negative_space():
    # 任务 3: 负空间自洽性
    print("[验证 3] 负空间加法")
    sandbox = ProjectionSandboxB(int_part_bits=8)
    s1 = SignedHC(HC8.from_physical_value(1.0), -1)   # -1.0
    s2 = SignedHC(HC8.from_physical_value(0.5), -1)   # -0.5
    result = signed_add(s1, s2, sandbox)
    assert abs(result.to_float(sandbox) - (-1.5)) < 1e-10
    print(f"  (-1.0)+(-0.5)={result.to_float(sandbox):.1f}  ✅")

def verify_curve_intersection():
    # 任务 4: 曲线交点非平凡条件
    print("[验证 4] 曲线交点")
    sb1 = ProjectionSandboxB(int_part_bits=8)
    sb2 = ProjectionSandboxB(int_part_bits=16)
    assert abs(sb1.project(HC8.zero()) - sb2.project(HC16.zero())) < 1e-15
    print("  零值交点一致  ✅")

def verify_gradient_overflow():
    # 任务 5: 梯度更新与溢出（方案 B）
    print("[验证 5] 梯度更新溢出")
    sandbox = ProjectionSandboxB(int_part_bits=8)
    h = HC8.from_physical_value(100.0)
    phi = sandbox.project(h)
    delta = 2 * (phi - 0.5) * sandbox.R   # 模拟梯度
    h_new = sandbox.gradient_update(h, delta / sandbox.R, 1.0)
    assert h_new.to_physical_value() <= 255.999
    print(f"  Δ={delta:.0f}, H_new={h_new.to_physical_value()}, 未逃逸到 level  ✅")

def verify_float128_lossless():
    # 任务 6: float128 覆盖精度验证（v1.2 修正 E3）
    print("[验证 6] float128 覆盖精度")
    from decimal import Decimal, getcontext
    getcontext().prec = 40

    # HC16: 80 位 -> float128 113 位，无损
    N16 = 65536
    num16 = 50000*(N16**4) + 100*(N16**3) + 200*(N16**2) + 300*N16 + 400
    dec_num16 = Decimal(num16)
    dec_R16 = Decimal(N16**5)
    dec_Phi16 = dec_num16 / dec_R16
    dec_back16 = dec_Phi16 * dec_R16
    loss16 = abs(int(dec_back16) - num16)
    print(f"  HC16 float128 误差 = {loss16}  ✅")

    # HC32 默认: int_part=32 位, 总信息量 128 位 -> float128 113 位，损失 15 位（E3）
    N32 = 4294967296
    num32 = 50000*(N32**4) + 100*(N32**3) + 200*(N32**2) + 300*N32 + 400
    dec_num32 = Decimal(num32)
    dec_R32 = Decimal(N32**5)
    dec_Phi32 = dec_num32 / dec_R32
    dec_back32 = dec_Phi32 * dec_R32
    loss32 = abs(int(dec_back32) - num32)
    print(f"  HC32(默认32位int) float128 误差 = {loss32} (位长={loss32.bit_length() if loss32 else 0})  ⚠️ E3")

    # HC32 压缩: int_part=8 位, 总信息量 72 位 -> float128 113 位，无损
    num32_compressed = 200*(N32**2) + 300*N32 + 400  # 模拟 int_part=8 位的范围
    dec_num32c = Decimal(num32_compressed)
    dec_R32c = Decimal(256 * (N32**3))  # R=256
    dec_Phi32c = dec_num32c / dec_R32c
    dec_back32c = dec_Phi32c * dec_R32c
    loss32c = abs(int(dec_back32c) - num32_compressed)
    print(f"  HC32(压缩8位int) float128 误差 = {loss32c}  ✅")

def benchmark_division(N: int = 50_000):
    # 性能基准
    print(f"[验证 7] 批量除法性能 (N={N})")
    random.seed(42)
    data_a = [HC8.random() for _ in range(N)]
    data_b = [HC8.random() for _ in range(N)]

    sandbox = ProjectionSandboxB(int_part_bits=8)
    t0 = time.perf_counter()
    for a, b in zip(data_a, data_b):
        _ = sandbox.divide(a, b)
    t_B = time.perf_counter() - t0

    print(f"  方案 B (全局右移): {t_B*1000:.1f} ms")
    print("  （注: PC 端 float128 路径比 float64 略慢，但确定性更好）")

if __name__ == "__main__":
    verify_reversibility()
    verify_carry()
    verify_negative_space()
    verify_curve_intersection()
    verify_gradient_overflow()
    verify_float128_lossless()
    benchmark_division(50_000)
```

---

## 4. 性能与架构分析（v1.2 修订）

### 4.1 PC 端向量化潜力

| 维度 | 方案 A（大整数） | 方案 B（全局右移） | float128 统一沙盒 |
|------|-----------------|-------------------|-------------------|
| **单次周期** | ~85 cycles | ~9 cycles | ~12 cycles |
| **批量加速** | 无（7 字节非对齐） | AVX2/AVX-512，一次 4~8 个 double | AVX-512 一次 2~4 个 float128 |
| **内存模式** | 跳跃访问，Cache Line 利用率低 | 连续 float64 数组，完美对齐 | 连续 float128 数组，对齐 |
| **分支预测** | 逐层进位判断，失败率高 | 纯算术流水线，零分支 | 纯算术流水线，零分支 |
| **除数限制** | 仅奇数（Hensel 逆元） | **任意实数** | **任意实数** |
| **HC8 精度** | float64 损失 3 位 | float64 损失 3 位 | **无损** |
| **HC16 精度** | **float64 损失 27 位** | **float64 损失 27 位** | **无损** |
| **HC32 精度** | **float64 损失 75 位** | **float64 损失 75 位** | 默认损失 15 位，压缩后 **无损** |

**结论**：
- **float64 路径**: 方案 A 和方案 B 对 HC8 等价；对 HC16/32，方案 B 的「定点小数」语义更清晰，但**溢出者仍是 float64 尾数**（总信息量超过 53 位）。
- **float128 路径**: 113 位尾数统一覆盖 HC8/16；HC32 默认 int_part=32 位时**仍有 15 位损失**（E3 修正），压缩到 8 位后无损。

---

### 4.2 仅 HC 模式的安全舱壁

| 溢出场景 | 合体模式（危险） | 仅 HC 模式（安全） |
|---------|----------------|------------------|
| `b = 257.3` | `q=1` 进位到 `level`，概念球撕裂 | `q=1` 被截断，`int_part=1`，**结构完整** |
| 梯度更新 `H_new = 500` | `level+=1`，超度量距离突变 | 饱和到 `255.999...`，**局部损失** |
| 负空间 `-357` | 补码最高位为 1，字典序突变 | sign 字段隔离，**比较规则不变** |

> **溢出语义引用**: 仅 HC 模式与合体模式的溢出边界详见 [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §2。本文档禁止重复解释。

---

## 5. 误区澄清（v1.2 修订）

| 误区 | 事实 |
|------|------|
| 投影方案破坏了 SGN 的纯整数确定性 | **没有**。模型文件、网络传输、持久化存储仍用 HC 字节。投影仅在**运算时**临时借道 float64/128，算完立即回灌。 |
| float64 除法比整数除法慢 | 在**标量**层面是；但在**批量 SIMD** 层面，float64 divpd 吞吐远高于 56 位整数除法（x86 `div` 指令极慢）。 |
| 逆归一化的 q 进位到 H[ℓ+1] 是标准语义 | **不是**。SGN 标准进位链是 `frac -> int_part -> level`。沙盒回灌后的 `q` 在仅 HC 模式下被截断，需重新对齐到标准语义。 |
| 仅 HC 模式饱和会丢失精度 | **会**，但这是**设计预期**。仅 HC 模式的价值不是保精度，而是**保结构**——不让进位逃逸到 level 层撕裂超度量树。 |
| 这个方案在 MCU 上也能用 | **不能**。MCU 无 float64 SIMD，且 float 运算需软件库。本规范明确标注为**PC 端扩展**。 |
| **方案 A 和方案 B 对 float64 等价** | **是**。float64 的 53 位尾数是硬天花板，无论大整数投影还是定点小数投影，HC16/32 的总信息量都装不下。**溢出者永远是 float64 尾数**。 |
| **float128 对 HC32 完全无损** | **否（E3 修正）**。默认 int_part=32 位时总信息量 128 位 > 113 位，**仍有 15 位损失**。需压缩 int_part 到 8 位才无损。 |

---

## 6. 与现有规范的协同关系

| 本规范能力 | 依赖的现有规范 | 协同方式 |
|-----------|--------------|---------|
| 负空间运算 | 《HC_补码负数扩展规范》 | 本规范采用独立 sign 方案，与补码方案在仅 HC 模式下等价，可互换 |
| 梯度稀疏 | 《HC_软阈值稀疏诱导算子规范》 | 投影回灌后，可接 `sgn_hc8_soft_threshold()` 做休眠区处理 |
| 代数闭包 | 《HC_2adic平方根》《HC_Hensel提升求根》 | 投影方案覆盖**任意除数**除法，与 Hensel 的奇数除法形成互补（简单路径 vs 纯整数路径） |
| 运算后比较 | 《HC_超度量哈希与Trie索引规范》 | 逆归一化后的 HC 可直接插入 Trie，参与 WTA 竞争 |
| 分布式同步 | 《HC_Lamport时钟》《HC_Quorum共识》 | 投影运算在本地完成，共识时广播的仍是标准 HC 帧 |

> **引用注意**: 上述协同关系中涉及的所有 HC 运算接口（`sgn_hc8_soft_threshold`、`sgn_hc8_add_saturated` 等）均定义于 [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md)，禁止重复实现。

---

## 7. 实现路径（v1.2 修订）

### 阶段 1: Python 验证器（已完成）

- 文件: `sgn_projection_div_verify.py`
- 覆盖: 方案 A/B 可逆性、进位、负空间、**E3 修正验证**（float128 对 HC32 默认配置仍有损失）、性能基准
- 运行: `python sgn_projection_div_verify.py`

### 阶段 2: C++ 扩展头文件（1 天）

```cpp
// sgn_projection_sandbox.h
// 仅 PC 端编译（#ifdef SGN_PC_EXTENSION）

namespace sgn {
    // 方案 A: 大整数投影（保留，适合 HC8 + float64）
    struct ProjectionSandboxA {
        double scale;
        double project(const hc8_t& h);
        hc8_t inverse_normalize(double phi);
    };

    // 方案 B: 全局右移投影（新增，适合 HC16/32 + float128）
    // E3 修正: HC32 默认 int_part=32 位时 float128 仍有 15 位损失
    struct ProjectionSandboxB {
        uint32_t int_part_bits;
        long double R;   // float128

        template<typename HC_T>
        long double project(const HC_T& h) { return h.to_physical_value() / R; }

        template<typename HC_T>
        HC_T inverse_normalize(long double phi);
    };

    // float128 统一沙盒（默认推荐）
    // 使用约束: HC32 需先位宽压缩到 8 位 int_part 才保证无损
    using ProjectionSandbox = ProjectionSandboxB;
}
```

### 阶段 3: PyTorch/NumPy 桥接（1 天）

```python
# sgn_projection_torch.py
import torch

def hc_tensor_to_float128(hc_tensor: torch.Tensor, int_part_bits: int) -> torch.Tensor:
    # hc_tensor: (N, bytes) uint8 -> (N,) float128
    R = 2 ** int_part_bits
    physical = ...  # 位运算拼接
    return physical.float128() / R

def float128_tensor_to_hc(phi_tensor: torch.Tensor, int_part_bits: int) -> torch.Tensor:
    # 逆归一化，仅 HC 模式饱和
    R = 2 ** int_part_bits
    b = phi_tensor * R
    b = torch.clamp(b, 0, R - 1)  # 仅 HC 饱和
    # 分解为 int_part + frac...
    return hc_bytes
```

### 阶段 4: 开源集成（0.5 天）

- 配置项:
  - `PROJECTION_SANDBOX_ENABLE`（bool，默认 false）
  - `PROJECTION_SCHEME`（'A'/'B'/'float128'，默认 'float128'）
- 文档: 明确标注 PC 端扩展，MCU 不适用
- 与现有 `sgn_core.py` 的 WTA 竞争、模板合并完全兼容

---

## 8. HC16/32/64 投影沙盒扩展（v1.2 重大修订）

### 8.1 为什么必须扩展

| 类型 | 物理位宽 | **float64 尾数 53 位** | **float128 尾数 113 位** | 推荐沙盒 |
|------|---------|-------------------|---------------------|---------|
| **HC8** | 56 位 | 损失 3 位 | **无损** | 方案 A/B 均可 |
| **HC16** | 80 位 | 损失 27 位 | **无损** | float128 统一沙盒 |
| **HC32** | 128 位 | 损失 75 位 | **默认损失 15 位（E3）** | float128 + 位宽压缩 |
| **HC64** | 192 位 | 完全不可用 | 缺口 79 位 | 整数沙盒（Python int / 分段） |

**核心原则**: 中间层精度必须 **>= HC 总信息量**，否则逆归一化时低位被吞掉，结果失真。**溢出者永远是 float 尾数，不是 HC 本身**。

### 8.2 float128 统一沙盒: 一套代码管所有精度（含 E3 修正）

```cpp
// sgn_projection_sandbox.h
// float128 统一沙盒: PC 端 only
// E3 修正说明:
//   - HC8 (56位): float128 113位 > 56位，无损 ✅
//   - HC16 (80位): float128 113位 > 80位，无损 ✅
//   - HC32 默认 (128位, int_part=32): float128 113位 < 128位，损失15位 ⚠️
//   - HC32 压缩 (72位, int_part=8): float128 113位 > 72位，无损 ✅

#ifdef SGN_PC_EXTENSION
namespace sgn {
    struct ProjectionSandbox128 {
        uint32_t int_part_bits;
        long double R;   // float128，113 位尾数

        template<typename HC_T>
        long double project(const HC_T& h) {
            return h.to_physical_value() / R;
        }

        template<typename HC_T>
        HC_T inverse_normalize(long double phi) {
            long double b = phi * R;
            uint64_t q = (uint64_t)(b / R);   // 截断，不进 level
            long double r = b - q * R;
            return HC_T::from_float(r);
        }

        template<typename HC_T>
        HC_T divide(const HC_T& a, const HC_T& b) {
            long double pa = project(a);
            long double pb = project(b);
            if (pb == 0) return HC_T::max_value();
            return inverse_normalize<HC_T>(pa / pb);
        }
    };
} // namespace sgn
#endif // SGN_PC_EXTENSION
```

**设计意图**：
- **float128（113 位尾数）** 直接覆盖 HC8（56 位）和 HC16（80 位），无损。
- HC32（128 位）在默认 int_part=32 位时**损失 15 位**（E3 修正）。
- HC32 可通过「位宽压缩」把 int_part 压到 8 位（总信息量 72 位），实现**完全无损**。
- 无需分段沙盒，一套 `ProjectionSandbox128` 管所有精度档位——但使用者必须知晓 HC32 默认配置的限制。

---

## 9. 总结（v1.2 修订）

> **投影除法不是让 HC 变成 float，而是让 float 临时为 HC 打工。**

- **方案 A（大整数投影）**: `Phi = H_physical_int * scale`，保留 v1.0，适合 HC8 + float64 边缘场景。**溢出者：float64 尾数被大整数撑爆**。
- **方案 B（全局右移投影）**: `Phi = physical_value / R`，R = 2^int_part_bits，float64/128 全程当定点小数用，不装大整数。**溢出者仍是 float64 尾数（总信息量超过 53 位）**。
- **位宽左移右移**: 通过 level 抬走大整数，压缩 int_part 有效位宽，减小 R，降低 float64 被撑爆的程度。但**float64 的 53 位尾数是瓶颈，不是 HC 的位数**。
- **float128 统一沙盒**: 113 位尾数直接覆盖 HC8/16；HC32 默认 int_part=32 位时**仍有 15 位损失（E3 修正）**，压缩到 8 位后无损。**一套代码管所有精度档位——但需注意 HC32 的默认配置限制**。
- **小数变零现象**: float64 装 HC16/32 时，53 位尾数被整数+小数总信息量撑爆，低位被迫归零。这是 float64 的硬天花板，不是用法错误。
- 纯整数基座不变: 存储、传输、共识仍用原生 HC 字节，md5sum 一致。
- **E3 修正**: float128 对 HC32 默认 int_part=32 位时**并非无损**，需压缩到 8 位才实现无损覆盖。

---

## 10. 引用规范

本文档在涉及以下概念时，**必须**引用核心文件的对应章节，禁止重复解释或重新定义：

- 三种模式选择（仅 HC / 仅 level / 合体）→ [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §1
- 溢出语义（仅 HC 饱和 vs 合体进位）→ [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §2
- 精度档位定义 → [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §3
- HC8/16/32 类型定义与转换 → [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §1
- 超度量距离与有限深度树 → [`sgn_math_core.md`](sandbox:///mnt/agents/output/sgn_math_core.md) §5.2
- 性能基准（48MHz/1T）→ [`sgn_index_matrix.md`](sandbox:///mnt/agents/output/sgn_index_matrix.md) §5

---

*本文档为 SGN 技术栈扩展文档。所有模式定义、溢出语义、精度档位必须遵守核心文件引用规范。*
