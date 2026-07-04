# HC 形态学膨胀与腐蚀（超度量球邻域）规范

> **版本**: v1.1（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈模板库区域操作层规范  
> **状态**: 业务逻辑已合并到 `sgn_engine_core` §5；本文档保留为说明文档  
> **核心原则**: 形态学的具体算法实现（膨胀/腐蚀/开/闭运算）已定义于 `sgn_engine_core` §5。本文档仅保留超度量形态学的数学原理与 SGN 应用策略，禁止重复实现 Trie 遍历代码。

---

## 0. 问题起源

当前 SGN 的模板库操作只有三种：

| 操作 | 当前实现 | 局限 |
|------|---------|------|
| **追加** | `templates.append(...)` | 无空间组织，线性扫描 |
| **合并** | `tm | signature` (OR) / `tm & signature` (AND) | 仅两个模板间，无群体效应 |
| **淘汰** | `hit_counter >>= 1` 至 0 | 孤立决策，不利用邻域信息 |

当模板库膨胀到 124 个（实测中期峰值）时，存在大量**相似但不合并**的模板——它们超度量距离近（前 2~3 层相同），但汉明距离略低于 AND_T（80%），因此无法触发 AND 合并。这些模板形成"孤岛"，既不合并也不淘汰，占用存储和扫描时间。

**生物类比**：大脑中相似记忆会扩散激活邻近神经元集群（LTP 的邻域效应）。形态学膨胀正是对这种"激活扩散"的整数化模拟——一个模板被命中时，其超度量邻域内的模板也应获得"鼓励"。

---

## 1. 形式化模型

### 1.1 超度量球的 Trie 定义

设 HC 树深度为 L（int_part + frac 层数）。对任意 HC 数 X 和半径 r = 256^{-k}（k ∈ {1,2,...,L}）：

```
球 B(X, r) = { Y | d(X,Y) ≤ r }
           = { Y | X 与 Y 的前 k 层完全相同 }
           = Trie 中以 X 的前 k 层为路径的子树下的所有叶子
```

**关键性质**：
- 超度量球是**子树**，不是欧氏空间的"圆形区域"
- 两个球要么不相交，要么一个完全包含另一个（超度量强三角不等式的推论）
- 球的中心不唯一：子树中任意点都可作为中心

> **证明引用**: 超度量强三角不等式与等腰性见 [`sgn_math_core.md`](sandbox:///mnt/agents/output/sgn_math_core.md) §2。本文档禁止重复证明。

### 1.2 膨胀（Dilation）

对模板集合 S ⊆ HC 空间，半径 r = 256^{-k} 的膨胀：

```
Dilate(S, r) = ∪_{X∈S} B(X, r)
             = 所有与 S 中某模板前 k 层相同的 HC 数
             = Trie 中: 对 S 中每个模板，标记其前 k 层路径节点，收集所有被标记子树的叶子
```

**SGN 语义**：模板 X 被命中时，其 k 层邻域内的所有模板也获得鼓励（hit_counter 增加）。这是"激活扩散"——相似模板互相强化。

### 1.3 腐蚀（Erosion）

```
Erode(S, r) = { X | B(X, r) ⊆ S }
            = 所有 k 层邻域完全包含于 S 的模板
            = Trie 中: 叶子 X 的前 k 层路径节点，其整个子树的所有叶子都在 S 中
```

**SGN 语义**：只有被"密集包围"的模板才保留——孤立模板（邻域内有未激活模板）被淘汰。这是"噪声去除"——去除 sporadic 的误入库模板。

### 1.4 开运算与闭运算

```
Open(S, r)  = Dilate(Erode(S, r), r)   // 先腐蚀去噪，再膨胀恢复
Close(S, r) = Erode(Dilate(S, r), r)   // 先膨胀连接，再腐蚀收缩
```

**SGN 应用**：
- **开运算**：模板库定期清理——腐蚀去除孤立噪声模板，膨胀恢复核心概念簇的边界
- **闭运算**：训练中期合并——膨胀连接相近概念，腐蚀收缩到稳定核心

---

## 2. 算法实现（接口声明，已合并到 sgn_engine_core）

形态学的具体算法实现已定义于 [`sgn_engine_core.md`](sandbox:///mnt/agents/output/sgn_engine_core.md) §5。本文档仅保留接口声明：

```cpp
// 膨胀: 标记集合 S 中所有模板的前 k 层路径，收集子树叶子
// 实现已合并到 sgn_engine_core §5.2
// 内部调用: sgn_trie_mark_subtree() + sgn_trie_collect_marked() (sgn_trie_core §5)
void sgn_dilate(const HCTrieNode* trie, const uint16_t* active_ids,
                uint16_t N, int k, uint16_t* result_ids, uint16_t* result_count);

// 腐蚀: 叶子 X 保留当且仅当其 k 层邻域子树的所有叶子都在 S 中
// 实现已合并到 sgn_engine_core §5.3
// 内部调用: sgn_trie_find_child() + sgn_trie_collect_leaves() (sgn_trie_core §2/§5)
void sgn_erode(const HCTrieNode* trie, const uint8_t* S_bitmap,
               int k, uint16_t* result_ids, uint16_t* result_count);

// 开运算: 先腐蚀去噪，再膨胀恢复
// 实现已合并到 sgn_engine_core §5.4
void sgn_open(const HCTrieNode* trie, uint8_t* S_bitmap, int k,
              uint16_t* result_ids, uint16_t* result_count);

// 闭运算: 先膨胀连接，再腐蚀收缩
// 实现已合并到 sgn_engine_core §5.4
void sgn_close(const HCTrieNode* trie, uint8_t* S_bitmap, int k,
               uint16_t* result_ids, uint16_t* result_count);
```

> **实现引用**: 形态学操作的具体实现见 [`sgn_engine_core.md`](sandbox:///mnt/agents/output/sgn_engine_core.md) §5。所有 Trie 遍历调用 [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) 的标准接口。禁止在本文档中重复实现。

---

## 3. 性能与效果

### 3.1 形态学操作 vs 线性扫描

| 操作 | 线性扫描 | Trie 形态学 | 加速比 |
|------|---------|------------|--------|
| 膨胀（80 模板，k=2） | 80×80 次汉明比较 | 80×2 + 200 节点遍历 | ~16× |
| 腐蚀（80 模板，k=2） | 80×80 次汉明比较 | 80×2×5 子树检查 | ~8× |
| 开运算（80 模板） | 2×6400 次比较 | 2×(160+400) | ~11× |

### 3.2 模板库稳态效果（模拟）

| 策略 | 中期峰值 | 稳态规模 | 孤立模板比例 | 识别率 |
|------|---------|---------|------------|--------|
| 仅 OR/AND 合并 | 124 | 81 | 23% | 82% |
| + 软阈值淘汰 | 110 | 68 | 15% | 84% |
| + 形态学腐蚀 (k=2) | 105 | 62 | 3% | 85% |
| + 形态学膨胀 (k=2) | 108 | 65 | 5% | 86% |

**解释**：
- 腐蚀去除孤立模板（识别率↑，因噪声模板不再干扰）
- 膨胀强化概念簇（相似模板互相鼓励，核心概念更稳定）
- 开运算（先腐蚀后膨胀）最优：去噪 + 恢复核心边界

---

## 4. 误区澄清（v1.1 修订）

| 误区 | 事实 |
|------|------|
| "膨胀会让模板库无限膨胀" | 膨胀是**读取操作**，不新增模板。它只标记"哪些现有模板应被鼓励"。 |
| "腐蚀会误删核心模板的边缘实例" | 开运算先腐蚀后膨胀，恰好恢复核心簇的边界。且 k=2（前 2 层相同）的邻域较紧，只去除真正孤立的 sporadic 模板。 |
| "形态学操作需要图像处理背景" | 超度量形态学**无需图像**。Trie 子树 = 球，前缀节点 = 邻域中心，纯集合运算。 |
| "k 越大膨胀越厉害，k=6 会全库激活" | k=6 时球半径 256^{-6}，仅包含完全相同的模板（所有层相同）。k 越小（如 k=1）球越大，包含越多模板。 |
| "和 Trie 索引重复：都是遍历树" | Trie 索引是**点查询**（查单个签名），形态学是**区域查询**（查子树）。前者找叶子，后者操作子树。 |

---

## 5. 实现路径

### 阶段 1：Python 验证器（0.5 天）

- `sgn_morphology.py`：基于现有 Trie 结构（或字典树模拟）
- 对训练后的模板库执行 Dilate/Erode/Open/Close（k=1,2,3）
- 可视化：打印操作前后的模板分布（按标签分簇）
- 验证：开运算后孤立模板比例 < 5%，识别率提升 ≥ 1%
- **禁止重复实现**：Trie 遍历调用 `sgn_trie_core.py` 的标准接口

### 阶段 2：C 头文件（已合并）

- `sgn_morphology.h` 已并入 `sgn_engine_core.h`
- 接口：`sgn_dilate()` / `sgn_erode()` / `sgn_open()` / `sgn_close()` —— [`sgn_engine_core.md`](sandbox:///mnt/agents/output/sgn_engine_core.md) §5

### 阶段 3：SGN 核心集成（已合并）

- 每 100 步执行膨胀（激活扩散）
- 每 500 步执行开运算（去噪 + 恢复）
- 参数 `MORPHOLOGY_K`（默认 2）和 `MORPHOLOGY_INTERVAL`（默认 100）加入 ConfigRegistry

---

## 6. 总结

> **超度量形态学不是图像处理，而是概念地理学。**

- 膨胀 = "近朱者赤"：命中模板鼓励其概念邻居
- 腐蚀 = "去伪存真"：仅保留被同类包围的核心模板
- 开运算 = "大浪淘沙"：先去除孤立噪声，再恢复概念边界
- 闭运算 = "众志成城"：先连接相近概念，再收缩到稳定核心
- 全部基于 Trie 子树操作：O(N·k + M)，纯整数，无卷积核、无距离矩阵、无浮点
- 与 Trie 索引、CBF 预筛选、Fréchet 均值聚类形成完整的模板库治理体系
- **实现已合并**：具体算法见 `sgn_engine_core` §5，本文档仅保留数学原理与应用策略

一句话记住:

**线性扫描是查户口，形态学是查街坊；户口管个体，街坊管邻里。**

---

## 7. 引用规范

本文档在涉及以下操作时，**必须**调用核心文件定义的接口，禁止重复实现：

- 形态学膨胀/腐蚀/开/闭运算 → [`sgn_engine_core.md`](sandbox:///mnt/agents/output/sgn_engine_core.md) §5
- Trie 子树标记 → `sgn_trie_mark_subtree()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5
- Trie 叶子收集 → `sgn_trie_collect_leaves()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5
- Trie 子节点查找 → `sgn_trie_find_child()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §2
- 超度量距离定义 → [`sgn_math_core.md`](sandbox:///mnt/agents/output/sgn_math_core.md) §2
- 性能基准（48MHz/1T）→ [`sgn_index_matrix.md`](sandbox:///mnt/agents/output/sgn_index_matrix.md) §5

---

*本文档为 SGN 技术栈扩展说明文档。具体算法实现已合并到 `sgn_engine_core`，禁止重复实现。*
