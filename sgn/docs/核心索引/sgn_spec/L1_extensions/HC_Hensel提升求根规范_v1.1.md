# HC Hensel 提升求根规范

> **版本**: v1.1（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈代数闭包层规范——多项式求根泛化  
> **状态**: 已去重，引用核心文件  
> **核心原则**: 本文档仅保留 Hensel 提升的数学原理与应用场景。所有多项式求值、导数计算、模逆元、带进位加法禁止重复实现，必须调用 `sgn_hc_ops` 定义的标准接口。

---

## 0. 问题起源

《HC 2-adic 平方根规范》已解决 `x² ≡ a (mod 2^W)` 的纯整数开方，但 SGN 的未来场景将触发更一般的非线性方程：

| 场景 | 方程形式 | 当前规避 |
|------|---------|---------|
| **势能竞争平衡** | `base + match·γ - k·x² = 0`（二次势能） | 纯线性竞争 |
| **多模板能量均分** | `Σ(x - w_i)² = E`（方差约束） | 无能量概念 |
| **特征值归一化** | `det(A - λI) = 0`（特征多项式） | 无矩阵运算 |
| **自适应增益衰减** | `γ_t = γ_0 / (1 + α·t²)`（二次分母） | 固定增益 |

**核心矛盾**：若此时被迫引入浮点 `numpy.roots` 或 `math.sqrt`，SGN 的跨平台确定性和 MCU 无 FPU 能力将崩塌。

Hensel 提升的泛化版本恰好解决此矛盾：对任意多项式 `f(x) ∈ Z[x]`，在 256 进制（即 2-adic 扩展）下逐层求解，全程纯整数。

---

## 1. 形式化模型

### 1.1 Hensel 引理（一般形式）

**定理**：设 `f(x)` 为整数系数多项式，`r` 满足：
```
f(r) ≡ 0      (mod 256^k)
f'(r) ≢ 0     (mod 256)
```
则存在唯一的 `r'` 使得：
```
r' ≡ r          (mod 256^k)
f(r') ≡ 0      (mod 256^{k+1})
```

**迭代公式**：
```
r_{k+1} = r_k - f(r_k) · [f'(r_k)]^{-1}   (mod 256^{k+1})
```

其中 `[f'(r_k)]^{-1}` 是 `f'(r_k)` 在模 256 下的乘法逆元（因 `f'(r) ≢ 0 mod 256`，逆元存在）。

### 1.2 与 HC 层的对齐

| 迭代步 k | 模数 | 对应 HC 精度 | 填充层数 |
|---------|------|------------|---------|
| 0 | 256^1 = 256 | 8 位 | int_part |
| 1 | 256^2 = 65536 | 16 位 | int_part + frac[0] |
| 2 | 256^3 = 16,777,216 | 24 位 | + frac[1] |
| 3 | 256^4 | 32 位 | + frac[2] |
| 4 | 256^5 | 40 位 | + frac[3] |
| 5 | 256^6 | 48 位 | + frac[4] |
| 6 | 256^7 | 56 位 | + frac[5]（HC8 满精度） |

**关键性质**：每步迭代精度翻倍（256^k → 256^{k+1}），但为与 HC 层对齐，实际每步填充 8 位（一层）。HC8 满精度需 7 步迭代。

### 1.3 逆元存在条件

`f'(r)` 在模 256 下可逆 ⟺ `gcd(f'(r), 256) = 1` ⟺ `f'(r)` 为奇数。

**处理偶导数**：
- 若 `f'(r)` 为偶数，标准 Hensel 提升失效
- 解决方案：提取因子 2^m，改用 `256/2^m` 为基的提升，或预处理多项式使其在根处导数为奇数

---

## 2. 算法实现（接口声明）

### 2.1 通用 Hensel 提升

```cpp
// sgn_hensel.h

// 多项式求值：f(x) = a_0 + a_1·x + a_2·x² + ... + a_n·x^n
// 实现要点：
//   1. 系数为 HC8 数，结果展平为 256 进制大整数
//   2. 每次乘法调用 sgn_hc8_mul()（sgn_hc_ops §3）
//   3. 每次加法调用 sgn_hc8_add_saturated()（sgn_hc_ops §3）
//   4. 模运算通过截断实现
uint64_t sgn_poly_eval(const uint8_t* coeffs, int n,
                       uint64_t x_mod, uint64_t mod);

// 导数求值：f'(x) = a_1 + 2·a_2·x + 3·a_3·x² + ...
// 实现要点：
//   1. 系数乘以 i（整数标量乘法）
//   2. 乘法/加法均调用 sgn_hc_ops 标准接口
uint64_t sgn_poly_deriv(const uint8_t* coeffs, int n,
                        uint64_t x_mod, uint64_t mod);

// 模 256 乘法逆元（扩展欧几里得，仅对奇数）
// 实现要点：
//   1. 扩展欧几里得算法，纯整数运算
//   2. 中间加法/减法调用 sgn_hc8_add_saturated() / sgn_hc8_sub()
uint8_t sgn_modinv_odd(uint8_t a);

// Hensel 提升主函数：求解 f(x) ≡ 0 (mod 256^L)
// 输入：初始根 r0 (mod 256)，系数数组，次数 n，目标层数 L
// 输出：HC8 格式的根
// 实现要点：
//   1. 逐层迭代：for(k=1; k<target_layers; k++)
//   2. 每步调用 sgn_poly_eval()、sgn_poly_deriv()、sgn_modinv_odd()
//   3. 修正量计算后，更新根 r = (r + mod - sub) % mod
//   4. 最终压缩回 HC8（调用 sgn_hc_ops 类型转换）
hc8_t sgn_hensel_lift(uint8_t r0,
                      const uint8_t* coeffs, int n,
                      int target_layers);
```

> **实现引用**: `sgn_poly_eval()` 和 `sgn_poly_deriv()` 内部的所有乘法调用 `sgn_hc8_mul()`，加法调用 `sgn_hc8_add_saturated()`；`sgn_modinv_odd()` 的加减法调用 `sgn_hc8_sub()`。这些接口均定义于 [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §2–§4。禁止在本文档中重复实现。

### 2.2 平方根特例（与现有规范兼容）

```cpp
// f(x) = x² - a，f'(x) = 2x
// 预条件：a ≡ 1 (mod 8)，此时 x=1 是模 8 的根
// 处理：2 在模 256 下不可逆，需改用逐字节提升
// 见《HC 2-adic 平方根规范》的逐层字节提升算法

hc8_t sgn_sqrt_hensel_special(uint8_t a_mod256);
// 实现：调用 sqrt_by_layers()（HC_2adic平方根规范 §2.3）
```

> **引用注意**: 平方根特例的快速路径已在 [`HC_2adic平方根_Hensel提升规范_v1.2.md`](sandbox:///mnt/agents/output/HC_2adic平方根_Hensel提升规范_v1.2.md) §2.3 中定义。本文档不再重复。

### 2.3 Python 原型（通用求解器）

```python
# sgn_hensel.py

def hensel_lift(f_coeffs, df_coeffs, r0, target_bits=48):
    # 通用 Hensel 提升求解器
    # f_coeffs: f(x) 的整数系数列表 [a0, a1, a2, ...]
    # df_coeffs: f'(x) 的整数系数列表
    # r0: 模 256 的初始根
    # target_bits: 目标精度位数
    # 实现要点：
    #   1. 每步调用 poly_eval()、poly_deriv()（内部调用 sgn_hc8_mul/add）
    #   2. 逆元计算调用 modinv_odd()
    #   3. 结果压缩回 HC8（调用 sgn_hc_ops 类型转换接口）
    r = r0
    mod = 256
    k = 1

    while k * 8 < target_bits:
        mod *= 256
        fr = poly_eval(f_coeffs, r, mod)    # 内部调用 sgn_hc8_mul()
        dfr = poly_eval(df_coeffs, r, 256)  # 内部调用 sgn_hc8_mul()

        if dfr % 2 == 0:
            raise ValueError("导数为偶数，Hensel 提升失效")

        fk = (fr // (mod // 256)) % 256
        inv_dfr = pow(dfr, -1, 256)
        delta = (fk * inv_dfr) % 256

        r = (r - delta * (mod // 256)) % mod
        k += 1

    return int_to_hc8(r)  # 调用 sgn_hc_ops 类型转换
```

---

## 3. 性能与精度

### 3.1 迭代步数与精度

| 目标精度 | 模数 | 迭代步数 | 填充 HC 层数 | 总周期 @48MHz/1T |
|---------|------|---------|------------|-----------------|
| 8 位 | 256 | 1 | int_part | ~100 → **4.2 μs** |
| 16 位 | 65536 | 2 | + frac[0] | ~300 → **12.5 μs** |
| 24 位 | 16M | 3 | + frac[1] | ~700 → **29.2 μs** |
| 48 位 | 256^6 | 6 | + frac[5] | ~2,500 → **104.2 μs** |

> **修正说明**: v1.0 按 24MHz/12T 估算周期。48MHz/1T 下时间缩短为 1/24。

### 3.2 与浮点求根的对比

| 维度 | numpy.roots | Hensel 提升 |
|------|------------|-------------|
| **精度** | 浮点 53 位 | 48 位（HC8）/ 64 位（HC16） |
| **确定性** | 依赖 LAPACK/BLAS | **逐位确定**，md5sum 一致 |
| **硬件依赖** | 需 FPU/向量库 | **纯整数 ALU** |
| **适用域** | 一般复系数多项式 | **整数系数**，模 256 可逆导数 |
| **收敛速度** | 二次收敛（牛顿法） | **二次收敛**（同牛顿法） |
| **MCU 周期** | 不可用 | ~2,500 周期（48 位根）≈ **104 μs** |

---

## 4. 误区澄清（v1.1 修订）

| 误区 | 事实 |
|------|------|
| "Hensel 提升只能求一个根，复根怎么办" | SGN 场景（势能、增益）只需要**实根**。若需多根，可在不同初始根上并行运行多个 Hensel 实例。 |
| "导数为偶数时完全失效，太受限" | 可通过**变量替换**（如 `x = 2y + 1`）或**提取公因子**规避。 |
| "比 2-adic 平方根慢太多" | 平方根是 `f(x)=x²-a` 的特例，可用更直接的逐字节提升。通用 Hensel 用于更复杂多项式。 |
| "需要大整数运算，8051 算不动" | 迭代中 mod=256^k，k=6 时 mod=2^48，需 48 位中间变量。8051 可分段计算（6 字节逐次处理），或限制到 k=3（24 位）。 |
| "和补码负数冲突：多项式系数可为负" | 不冲突。负系数用补码 SHC8 表示，求值时按补码加法规则运算，结果自然正确。调用 `sgn_shc8_add()`（`sgn_hc_ops` §4）。 |
| "SGN 现在就需要这个吗" | **不是**。当前 SGN 无多项式求根场景。这是**代数闭包预判**，为势能竞争、特征值等远期功能保留数学接口。 |

---

## 5. 实现路径

### 阶段 1：Python 验证器（1 天）

- `sgn_hensel.py`：通用 Hensel 提升 + 平方根特例
- 验证：
  - `f(x)=x²-17`，初始根 `r0=1`（因 1²-17=-16 ≡ 0 mod 8），提升到 48 位
  - 验证 `r² ≡ 17 (mod 2^48)`，与 `math.isqrt` 结果对比
  - `f(x)=x³-2x-5`（Wallis 经典例），模 256 下找根并提升
- 绘制：迭代步数 vs 有效位数曲线（验证二次收敛）
- **禁止重复实现**：所有加减乘调用 `sgn_hc_ops.py` 标准接口

### 阶段 2：C 头文件（1 天）

- `sgn_hensel.h`：`sgn_hensel_lift()` / `sgn_poly_eval()` / `sgn_poly_deriv()` / `sgn_modinv_odd()`
- 限制：8051 版本仅支持 `target_layers ≤ 3`（24 位精度），PC 版本支持 6 层
- 与 `sgn_2adic_sqrt.h` 的 `sqrt_by_layers()` 形成平方根快速路径
- **禁止重复实现**：`sgn_poly_eval()` / `sgn_poly_deriv()` 内部调用 `sgn_hc8_mul()` 和 `sgn_hc8_add_saturated()`（`sgn_hc_ops.h`）

### 阶段 3：代数闭包集成（0.5 天）

- 预留接口：`sgn_solve_poly()`（通用多项式求解入口）
- 配置项：`HENSEL_ENABLE`（bool）、`HENSEL_MAX_LAYERS`（2/3/6）
- 文档：明确标注"当前仅验证器可用，核心引擎未调用"

---

## 6. 总结

> **Hensel 提升不是让 SGN 今天解方程，而是让 SGN 明天不解体。**

- 从 2-adic 平方根到任意多项式：统一的纯整数求根框架
- 每步迭代精度翻倍（二次收敛），7 步填满 HC8 的 56 位精度
- 与 HC 层天然对齐：每步填充 8 位，根的字节即 HC 的字节
- 导数可逆条件（奇数）可通过预处理满足，偶导数场景有规避策略
- 与补码负数协同：负系数用 SHC8 表示，求值自然正确（调用 `sgn_shc8_add()`）
- 与 2-adic 平方根规范形成闭环：简单根用逐字节提升，复杂根用通用 Hensel
- 纯整数、确定性、无 FPU，与 SGN 跨平台 md5sum 一致性完全兼容
- **去重完成**：所有多项式求值、导数计算、模逆元中的加减乘调用 `sgn_hc_ops` 标准接口，禁止重复实现

一句话记住：

**牛顿法是望远镜，Hensel 提升是显微镜；望远镜看远方，显微镜填精度。**

---

## 7. 引用规范

本文档在涉及以下运算时，**必须**调用核心文件定义的接口，禁止重复实现：

- HC8 带进位加法 → `sgn_hc8_add_saturated()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §3
- HC8 借位减法 → `sgn_hc8_sub()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §4
- HC8 多精度乘法 → `sgn_hc8_mul()`（内部调用 `sgn_hc8_add_saturated()`）—— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §3
- 有符号 HC8 加法 → `sgn_shc8_add()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §4
- 2-adic 平方根快速路径 → `sqrt_by_layers()` —— [`HC_2adic平方根_Hensel提升规范_v1.2.md`](sandbox:///mnt/agents/output/HC_2adic平方根_Hensel提升规范_v1.2.md) §2.3
- 性能基准（48MHz/1T）→ [`sgn_index_matrix.md`](sandbox:///mnt/agents/output/sgn_index_matrix.md) §5

---

*本文档为 SGN 技术栈扩展文档。所有基础运算 ABI 调用必须遵守上述引用规范。*
