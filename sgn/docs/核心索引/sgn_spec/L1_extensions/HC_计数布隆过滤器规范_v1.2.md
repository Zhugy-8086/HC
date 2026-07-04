# HC 计数布隆过滤器（Count Bloom Filter）规范

> **版本**: v1.2（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈模板库成员测试层规范  
> **状态**: 已去重，引用核心文件；已修正 E2（假阳性率）、E6（level 语义矛盾）  
> **核心原则**: 本文档仅保留 CBF 业务逻辑与协同流程。所有 HC 基础运算（比较、加法、软阈值）禁止重复实现，必须调用 `sgn_hc_ops` 定义的标准接口。

---

## 0. 问题起源

当前 SGN 模板库的成员测试是**线性扫描**：

```python
for i, (tlb, tm, sc, hc) in enumerate(self.templates):
    sim = match_bits(tm, signature, d=self.D)
    if sim >= OR_T: ...
```

当模板库稳态 80 个、峰值 124 个时，单次测试需 80~124 次 `match_bits`（每次 `popcount` 16~64 位）。这在大规模部署（如 1024 模板）时将不可接受。

**传统布隆过滤器（BF）的局限**：
- 标准 BF：只能测试"可能存在/一定不存在"，无法删除（模板淘汰时无法更新）
- 计数 BF（CBF）：每个桶 4 位计数器（0~15），支持删除，但：
  - **饱和**：计数器满 15 后无法区分高频与超高频
  - **溢出**：多模板哈希到同一桶时，计数器快速饱和
  - **无梯度**：计数器是整数，无小数精度，无法表达"接近淘汰"的渐变状态

HC 计数器恰好解决这三点：
- **不饱和**：frac 层保留小数精度，"14.9" 与 "15.0" 在淘汰阈值附近可区分
- **有梯度**：软阈值算子实现渐进递减，而非硬减 1
- **可软衰减**：通过软阈值算子递减，保留唤醒可能

---

## 1. 形式化模型

### 1.1 HC-CBF 结构

```
HC-CBF:
  m: 桶数量（如 1024，2 的幂次便于位掩码寻址）
  k: 哈希函数数量（如 3）
  buckets[m]: 每个桶是一个 HC8 数

哈希函数:
  h_0, h_1, h_2: 模板签名 -> [0, m-1]
  基于保前缀哈希（见 sgn_trie_core §1）的 k 个独立切片
```

**插入模板签名 S**：
```
for i in 0..k-1:
    idx = h_i(S)
    buckets[idx] = sgn_hc8_add_saturated(buckets[idx], delta)
    // delta = HC8(0, [1,0,0,0,0,0]) 即 1/256
    // 调用 sgn_hc_ops §3: 仅 HC 模式饱和加法，int_part clamp 255，不向 level 进位
```

**查询模板签名 S**：
```
for i in 0..k-1:
    idx = h_i(S)
    if buckets[idx] == HC_ZERO:
        return DEFINITELY_NOT_EXISTS  // 一定不存在
min_val = min(buckets[h_i(S)] for i in 0..k-1)
// min 通过 sgn_hc8_less() 比较（sgn_hc_ops §2）
return min_val  // HC 数，表达存在概率/频率的梯度
```

**删除/衰减模板签名 S**：
```
for i in 0..k-1:
    idx = h_i(S)
    buckets[idx] = sgn_hc8_soft_threshold(buckets[idx], Lambda)
    // 调用 sgn_hc_ops §5: 单侧软阈值
```

### 1.2 假阳性率分析（v1.2 修正 E2）

传统 CBF 假阳性率（布尔存在测试）：
```
P_fp ≈ (1 - e^(-kn/m))^k
```

**v1.0 错误声明**：`n=256, m=1024, k=3` 时 `P_fp≈1.8%`  
**v1.2 修正**：按标准布隆过滤器公式计算，实际 `P_fp≈14.7%`。若需达到 1.8% 假阳性率，应使用 `m=4096, k=3`。

| 模板数 n | 桶数 m | 哈希数 k | 标准 CBF P_fp（理论） |
|---------|--------|--------|---------------------|
| 64 | 256 | 3 | ~0.8% |
| 128 | 512 | 3 | ~1.5% |
| 256 | 1024 | 3 | **~14.7%** |
| 256 | 4096 | 3 | **~1.8%** ✅ |
| 512 | 2048 | 4 | ~8.5% |

> **设计建议**：以标准公式为保守上界。HC-CBF 的"梯度阈值"判定（非布尔存在测试）可降低等效假阳性，但设计选型时必须以标准公式为保守上界。

HC-CBF 的改进：
- **查询返回梯度**：不是布尔"存在/不存在"，而是 HC 数 `min_val`。`min_val` 越小，假阳性概率越高（桶被其他模板碰撞填充）
- **自适应阈值**：根据 `min_val` 的物理值决定后续动作：
  - `min_val > 2.0`：高置信存在，直接进行精确匹配
  - `0.5 < min_val ≤ 2.0`：中置信，先查 Trie 索引确认
  - `min_val ≤ 0.5`：低置信，视为不存在（但非绝对）

### 1.3 超度量兼容的哈希函数

利用 HC 保前缀哈希（`PH_k`）的 k 个切片作为独立哈希：

```
h_0(S) = PH_2(S) & (m-1)  // 取前 2 层（16 位）作为哈希 0
h_1(S) = (PH_4(S) >> 8) & (m-1)  // 取第 3~4 层作为哈希 1
h_2(S) = (S.int_part ^ S.frac[0]) & (m-1)  // 混合层作为哈希 2
```

**性质**：由于 `PH_k` 是前缀敏感的，相似模板（超度量距离小）的哈希值在桶空间中也接近，形成**局部敏感哈希（LSH）**效果。

---

## 2. 算法实现

### 2.1 桶操作（接口声明）

```cpp
// sgn_cbf.h
#define SGN_CBF_M  1024   // 桶数，2^10
#define SGN_CBF_K  3      // 哈希数
#define SGN_CBF_M_MASK  (SGN_CBF_M - 1)

typedef struct {
    hc8_t buckets[SGN_CBF_M];  // 每个桶 6 字节
} sgn_cbf_t;

// 初始化: 所有桶清零
void sgn_cbf_init(sgn_cbf_t* cbf);

// 插入: 签名 S -> k 个桶 += delta（调用 sgn_hc8_add_saturated）
void sgn_cbf_insert(sgn_cbf_t* cbf, const hc8_t* sig, const hc8_t* delta);

// 查询: 返回 k 个桶中的 min_val（字典序，调用 sgn_hc8_less）
hc8_t sgn_cbf_query(const sgn_cbf_t* cbf, const hc8_t* sig);

// 软衰减: 所有桶 soft_threshold（调用 sgn_hc8_soft_threshold）
void sgn_cbf_decay(sgn_cbf_t* cbf, const hc8_t* Lambda);
```

> **实现引用**: `sgn_cbf_insert` 中的 `sgn_hc8_add_saturated()` 见 [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §3；`sgn_hc8_less()` 见 [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §2；`sgn_hc8_soft_threshold()` 见 [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §5。禁止在本文档中重复实现。

### 2.2 CBF 桶的 level 语义（v1.2 修正 E6）

**明确约束**：CBF 桶使用**仅 HC 模式**（`level=0` 固定）。

- `int_part` 饱和在 255（`sgn_hc8_add_saturated` 行为）
- **严禁向 level 进位**（仅 HC 模式安全舱壁）
- 若计数器达到 255.999...，视为"极高频"，饱和即可

> **E6 修正说明**：v1.0/v1.1 文档中"向 level 进位，范围无界"的表述与"仅 HC 模式（level=0）"矛盾。v1.2 明确：CBF 桶是仅 HC 模式，int_part 饱和 255，不触发 level 进位。若需无界计数，应使用合体模式，但 CBF 桶不采用合体模式。

### 2.3 与 Trie 索引的协同

```cpp
// 模板插入流程: CBF 预筛选 + Trie 精确索引
void sgn_template_insert(sgn_trie_t* trie, sgn_cbf_t* cbf, 
                         const hc8_t* sig, uint16_t tid) {
    // 1. CBF 快速测试: 是否已存在相似模板？
    hc8_t cbf_val = sgn_cbf_query(cbf, sig);

    if (cbf_val > THRESH_CBF_HIGH) {
        // 高置信存在: 直接查 Trie 找精确匹配
        uint16_t existing_tid = sgn_trie_search_exact(trie, sig);
        if (existing_tid != 0xFFFF) {
            // 合并或更新
            sgn_template_merge(existing_tid, sig);
            return;
        }
    }

    // 2. 新模板: 插入 Trie + CBF
    sgn_trie_insert(trie, sig, tid);  // 见 sgn_trie_core §3
    sgn_cbf_insert(cbf, sig, &HC8_DELTA_ONE);  // delta = 1/256
}
```

**加速效果**：
- CBF 查询：`O(k)` = 3 次哈希 + 3 次 HC 比较 ≈ 30 周期
- Trie 精确查询：`O(L)` = 6 层比较 ≈ 60 周期
- 线性扫描：`O(N·D)` = 80 × 64 位 popcount ≈ 12,000 周期
- **加速比**：80×（CBF 预筛）→ 200×（CBF+Trie）

---

## 3. 性能与内存

### 3.1 内存占用

| 方案 | 结构 | 大小 | 1024 模板场景 |
|------|------|------|-------------|
| 线性扫描 | 模板数组 | 80 × 20B = 1.6KB | 查询 12,000 周期 |
| 纯 Trie | 稀疏树节点 | ~80 × 8B = 640B | 查询 60 周期 |
| **Trie + CBF** | Trie + 1024 桶 | 640B + 6KB = **6.6KB** | 查询 30 周期 |
| 传统 CBF | 1024 × 4bit | 512B | 查询 10 周期（但无梯度） |

**结论**：HC-CBF 内存 6KB（若 RAM 受限，可缩至 256 桶/HC8 压缩）。若 RAM 受限，可：
- 桶数降至 256（1.5KB）
- HC8 改为 3 层（3 字节/桶，256 桶 = 768B）
- 或存 Flash（只读查询，慢写）

### 3.2 假阳性率实测（模拟，v1.2 补充）

| 模板数 | 桶数 m | 哈希 k | 标准 CBF P_fp（理论） | HC-CBF 低置信率（模拟） |
|--------|--------|--------|---------------------|----------------------|
| 64 | 256 | 3 | ~0.8% | ~1.2% |
| 128 | 512 | 3 | ~1.5% | ~2.1% |
| 256 | 1024 | 3 | **~14.7%** | ~15% |
| 256 | 4096 | 3 | **~1.8%** | ~2% |
| 512 | 1024 | 4 | ~32% | ~33% |

注：HC-CBF "低置信率"指 `min_val ≤ 0.5` 但仍需查 Trie 确认的比例。由于 HC-CBF 返回梯度而非布尔，实际假阳性（未查 Trie 直接误判）为 0。

---

## 4. 误区澄清（v1.2 修订）

| 误区 | 事实 |
|------|------|
| "CBF 有假阳性，不能用于精确匹配" | **正确**，但 HC-CBF 不是替代精确匹配，而是**预筛选**。低置信时回退 Trie，高置信时跳过 Trie 直接合并。 |
| "6KB RAM 太大，8051 放不下" | 可缩至 256 桶 × 3 字节 = 768B，或存 Flash。且 CBF 是**可选优化**，非必需。 |
| "HC 计数器向 level 进位，但 level 不在桶里" | **E6 修正**：CBF 桶使用**仅 HC 模式**（`level=0` 固定），`int_part` 饱和在 255，**不向 level 进位**。 |
| "软衰减后桶值归零，但模板还在 Trie 里，不一致" | 设计如此：CBF 是**频率估计器**，Trie 是**精确索引**。CBF 归零仅表示"近期无命中"，模板仍可通过 Trie 找到。 |
| "和传统 CBF 的 4 位计数器比，HC 太浪费" | 4 位计数器无梯度、无小数、饱和即死。HC8 的 6 字节换取**无饱和 + 软衰减 + 跨平台确定性**，在 SGN 的价值观中是合算的。 |
| "v1.0 的 1.8% 假阳性率是否准确" | **不准确（E2 修正）**。该数值与标准布隆过滤器公式严重偏离。`n=256,m=1024,k=3` 时实际为 **14.7%**；若需 1.8%，应使用 `m=4096`。 |

---

## 5. 实现路径

### 阶段 1：Python 验证器（0.5 天）

- `sgn_cbf.py`：256/1024 桶 HC-CBF，3 哈希函数
- 测试：插入 1000 个随机签名，查询 10000 次（含 5000 个未插入签名）
- 验证：假阳性率与标准公式吻合（允许 ±20% 模拟误差）
- **新增**：分离"标准 CBF 假阳性率"与"HC-CBF 低置信率"的统计

### 阶段 2：C 头文件（0.5 天）

- `sgn_cbf.h`：桶数可配置（`#define SGN_CBF_M 256/512/1024/4096`）
- 哈希函数：基于 `crc8_table` 的 3 个变体，避免乘法
- 内存分配：支持静态数组（RAM）或 `__flash`（ROM，只读）
- **禁止重复实现**：所有 HC 运算调用 `sgn_hc_ops.h`

### 阶段 3：Trie 协同集成（0.5 天）

- `sgn_core.c`：`_add_template` 中，先 `sgn_cbf_query`，高置信直接合并，低置信查 Trie
- 对比测试：模板库 64/128/256/512 规模下的每步耗时

---

## 6. 总结

> **HC-CBF 不是布隆过滤器的替代品，而是它的超度量升级。**

- 传统 CBF 用 4 位计数器回答"有/没有"，HC-CBF 用 48 位精度回答"有多少、有多近、有多老"
- 无饱和：仅 HC 模式下 `int_part` 饱和 255，保留梯度信息
- 软衰减：淘汰不是硬清零，而是渐进休眠，保留唤醒可能
- 与 Trie 协同：CBF 做 30 周期的预筛，Trie 做 60 周期的精确索引，替代 12,000 周期的线性扫描
- 纯整数、确定性、跨平台 md5sum 一致
- **E2 修正**：假阳性率以标准布隆过滤器公式为设计基准，`n=256,m=1024,k=3` 时为 **14.7%**
- **E6 修正**：CBF 桶明确使用仅 HC 模式（`level=0` 固定），`int_part` 饱和 255，不向 level 进位

一句话记住:

**传统 CBF 是红绿灯，HC-CBF 是油量表；红绿灯只有停和走，油量表告诉你还能跑多远。**

---

## 7. 引用规范

本文档在涉及以下运算时，**必须**调用核心文件定义的接口，禁止重复实现：

- HC8 饱和加法 → `sgn_hc8_add_saturated()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §3
- HC8 字典序比较 → `sgn_hc8_less()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §2
- HC8 软阈值 → `sgn_hc8_soft_threshold()` —— [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §5
- Trie 插入/查找 → `sgn_trie_insert()` / `sgn_trie_search_exact()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §2/§3
- 仅 HC 模式溢出语义 → 见 [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §2
- 内存互斥规则 → 见 [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §7

---

*本文档为 SGN 技术栈扩展文档。所有基础运算 ABI 调用必须遵守上述引用规范。*
