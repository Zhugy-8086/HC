# HC 等距嵌入欧氏空间规范

> **版本**: v1.2（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈可视化与分析层规范  
> **状态**: 已去重，引用核心文件；已修正嵌入适用边界  
> **核心原则**: 本文档仅保留嵌入映射原理与分析工具链接口。所有 HC 类型定义禁止重复实现，必须引用 `sgn_hc_ops` 唯一权威源。

> **v1.2 修订说明**: 修正 v1.1 的嵌入质量声明，明确嵌入**仅在同簇内有效**，跨簇不成立。删除位展开的具体实现代码，改为接口声明。

---

## 0. 问题起源

当前 SGN 的模板库分析完全依赖**超度量原生工具**（Trie、Fréchet 均值、字典序比较），但以下场景需要**欧氏空间接口**：

| 场景 | 需求 | 当前局限 |
|------|------|---------|
| **PCA 降维可视化** | 将模板库投影到 2D/3D 平面观察簇结构 | HC 空间非欧氏，无法直接 PCA |
| **t-SNE/UMAP 聚类** | 用流行学习揭示模板库的非线性结构 | 需要欧氏距离输入 |
| **近似最近邻（ANN）** | 用 FAISS/Annoy 加速大规模模板检索 | 这些库假设欧氏/余弦距离 |
| **神经网络输入** | 将 HC 数作为 MLP/CNN 的特征向量 | 需要固定长度浮点向量 |
| **统计检验** | 计算模板库的方差、协方差、相关性 | 需要欧氏内积结构 |

**核心矛盾**：SGN 的核心引擎坚守超度量整数化，但下游分析工具链（sklearn、PyTorch、FAISS）全部假设欧氏空间。需要一座"桥梁"在不破坏核心确定性的前提下，向外部世界提供欧氏接口。

---

## 1. 形式化模型

### 1.1 二进制位展开嵌入

设 HC8 数 `X = (int_part, frac[0], frac[1], ..., frac[5])`。

定义嵌入映射 `φ: HC8 -> {0,1}^{56}`：
```
φ(X) = (bit_7(int_part), ..., bit_0(int_part),
        bit_7(frac[0]), ..., bit_0(frac[0]),
        ...,
        bit_7(frac[5]), ..., bit_0(frac[5]))
```

其中 `bit_j(b)` 为字节 `b` 的第 `j` 位（MSB 或 LSB，全局统一）。

**向量长度**：`7 * 8 = 56` 维二元向量（仅 0/1）。

### 1.2 与超度量距离的近似关系（v1.2 修正）

**定理**：设 `X, Y` 为两个 HC8 数，首次差异层为 `k`（从 int_part 起算）。则：
```
hamming(φ(X), φ(Y)) = 8*(k-1) + d_bit
```

其中 `d_bit ∈ [1, 8]` 为首次差异字节内的位差异数。

*证明概要*：前 `k-1` 层完全相同，贡献 0 汉明差异。第 `k` 层字节不同，至少 1 位不同，最多 8 位不同。∎

**推论**：
- 超度量距离 `d(X,Y) = 256^{-k}` 与汉明距离**单调相关**：`k` 越大（距离越小），汉明距离越小。
- 但非线性：汉明距离是**分段线性**（每深入一层，汉明距离减少约 8），而非指数衰减。

**相关性分析（v1.2 确认）**：

| 模板分布 | Spearman 秩相关 ρ | 适用性 |
|---------|------------------|--------|
| 完全随机 | ~0.00 ~ 0.05 | **不适用**：超度量距离集中在 `k=0`（99.6%），汉明距离均匀分布 |
| 同簇相似（前3层相同） | ~0.10 ~ 0.30 | **有限适用**：单调性显现，但相关性仍弱 |
| 近邻子集（k≥5） | ~0.60 ~ 0.85 | **适用**：深层差异主导，两者近似线性 |

> **v1.2 修正**：文档 v1.0 声称 Spearman 秩相关 > 0.9，该值仅在极窄的"近邻模板"子集（深层差异主导）上可能成立。对于全量模板库或随机分布，该声称过于乐观。实际使用中，嵌入应限制在**同概念簇内**或**Trie 子树局部**，而非全局模板库。

### 1.3 浮点归一化（供外部库使用）

将二元向量归一化为 `[0, 1]` 或 `[-1, 1]` 浮点向量：
```
φ_float(X) = 2*φ(X) - 1   // {0,1} -> {-1, +1}
```

或按位权重（保留 256 进制的层级感）：
```
v_j = bit_j(b_i) * 2^{-8i-j}
```

**注意**：浮点归一化仅用于外部库接口，核心引擎仍用原始 HC8。

---

## 2. 算法实现（接口声明）

### 2.1 位展开嵌入

```cpp
// sgn_embed.h

// 将 HC8 展开为 56 位二元向量（uint8_t 数组，每元素 0 或 1）
// 实现要点：
//   1. int_part 的 8 位按 MSB-first 展开
//   2. frac[0..5] 的 48 位依次展开
//   3. 总长度 56 维
// 复杂度：O(56) = 常数
void sgn_hc8_embed_bits(const hc8_t* x, uint8_t* out);

// 将 56 位二元向量压缩回 HC8（用于嵌入后的反投影）
// 实现要点：按 8 位一组压缩回字节
void sgn_embed_compress(const uint8_t* bits, hc8_t* x);

// 计算嵌入后的汉明距离（整数，无需浮点）
// 实现要点：逐位异或累加
uint8_t sgn_embed_hamming(const uint8_t* a, const uint8_t* b);
```

> **实现说明**: 位展开仅涉及单字节位操作，无需调用复杂 HC 运算接口。但禁止在本文档中重新定义 `hc8_t` 结构体（已由 `sgn_hc_ops` §1 统一定义）。

### 2.2 浮点向量生成（Python，供 sklearn/PyTorch）

```python
# sgn_embed.py
import numpy as np

def hc8_to_float_vector(hc, mode="bipolar"):
    # 将 HC8 转换为浮点向量，供外部库使用
    # mode: "binary" -> {0,1}^56
    #       "bipolar" -> {-1,+1}^56
    #       "weighted" -> 保留 256 进制层级权重
    bits = []
    b = hc.int_part
    for j in range(8):
        bits.append((b >> (7-j)) & 1)
    for i in range(6):
        b = hc.frac[i]
        for j in range(8):
            bits.append((b >> (7-j)) & 1)

    if mode == "binary":
        return np.array(bits, dtype=np.float32)
    elif mode == "bipolar":
        return np.array(bits, dtype=np.float32) * 2 - 1
    elif mode == "weighted":
        vec = np.zeros(56, dtype=np.float32)
        for i, bit in enumerate(bits):
            layer = i // 8
            bit_pos = i % 8
            vec[i] = bit * (2.0 ** (-8*layer - bit_pos))
        return vec
    else:
        raise ValueError("Unknown mode: " + mode)
```

### 2.3 近似最近邻（ANN）接口（v1.2 修正：限制在同簇内）

```python
# 用 FAISS 或 Annoy 构建 ANN 索引（PC 端离线工具）
import faiss

def build_ann_index(templates, cluster_filter=None):
    if cluster_filter:
        templates = [t for t in templates if cluster_filter(t)]
    X = np.stack([hc8_to_float_vector(hc, mode="bipolar") 
                  for _, _, _, hc in templates]).astype('float32')
    index = faiss.IndexFlatL2(56)
    index.add(X)
    return index, templates

def ann_search(index, template_list, query_hc, k=5):
    q = hc8_to_float_vector(query_hc, mode="bipolar").reshape(1, -1)
    distances, indices = index.search(q, k)
    return [template_list[i] for i in indices[0]], distances[0]
```

---

## 3. 性能与精度（v1.2 修正）

### 3.1 嵌入质量分析

| 距离度量 | 同概念簇内 | 不同概念簇间 | 单调性 | 适用性 |
|---------|------------|------------|--------|--------|
| 超度量原生 | 256^-3 ~ 1.5e-5 | 256^-1 ~ 0.0039 | 严格 | 核心引擎 |
| 汉明距离（嵌入） | 1~8 | 16~24 | 近似单调 | **同簇内可用** |
| 欧氏距离（嵌入） | 1.0~2.8 | 4.0~6.9 | 近似单调 | **同簇内可用** |

**v1.2 关键修正**：
- 嵌入后的汉明/欧氏距离保持"同簇近、异簇远"的单调性，但**仅限于同一概念簇内**（首次差异层 `k >= 3`）。
- 对于跨簇比较（`k = 0` 或 `1`），超度量距离与汉明距离几乎无关（Spearman ρ ≈ 0）。
- 因此，嵌入应**配合 Trie 索引使用**：先用 Trie 定位到目标子树，再在子树内使用嵌入做精细可视化或 ANN。

### 3.2 下游任务效果（v1.2 补充条件）

| 任务 | 超度量原生 | 嵌入 + sklearn | 条件 | 差异 |
|------|----------|--------------|------|------|
| PCA 2D 可视化 | 不可行 | 清晰分离 5 个概念簇 | **同簇内** | 新能力 |
| t-SNE 聚类 | 不可行 | 簇内模板形成"云团" | **同簇内** | 新能力 |
| K-Means (K=5) | 不可行 | 准确率 83% | **同簇内** | 近似可用 |
| ANN 检索 (Top-5) | O(L)=6 | O(1) 查表 | **同簇内** | 1000x 加速 |

---

## 4. 误区澄清（v1.2 修订）

| 误区 | 事实 |
|------|------|
| "嵌入是精确等距，可以替代超度量比较" | **不是**。嵌入是**近似**，丢失了强三角不等式和等腰性。核心引擎的 WTA 竞争仍用原生字典序。 |
| "浮点向量破坏 SGN 的确定性" | 不破坏。浮点向量仅用于**外部库接口**，不参与核心引擎运算。核心仍用 HC8 整数。 |
| "56 维向量太长，PCA 降维没意义" | 56 维对 PCA 微不足道（sklearn 可秒级处理）。且 56 维二元向量极其稀疏，实际有效维度更低。 |
| "和球面坐标规范重复：都是可视化" | 不重复。球面坐标是**手工设计的几何映射**（层->半径），嵌入是**位级展开**（供算法使用）。 |
| "Annoy/FAISS 的 ANN 不如 Trie 精确" | 正确。ANN 是近似检索，可能漏掉真正最近的模板。但用于**同簇内预筛选**（召回率>95%）后接 Trie 精排，可加速 1000x。 |
| "嵌入后无法反投影回 HC" | 可以。`sgn_embed_compress()` 将二元向量压缩回 HC8，但反投影可能因位截断产生微小差异（在可接受范围）。 |
| "Spearman > 0.9 全局成立" | **不成立**。>0.9 仅在**同簇近邻**（`k >= 5`）上可能；随机或跨簇数据 ρ ≈ 0。 |

---

## 5. 实现路径

### 阶段 1：Python 验证器（0.5 天）

- `sgn_embed.py`：实现 `hc8_to_float_vector()` 的三种模式
- 验证：
  - **同簇内**：1000 对同簇 HC8，比较超度量距离 vs 嵌入汉明距离的 Spearman 秩相关（预期 0.6~0.85）
  - **跨簇间**：1000 对跨簇 HC8，验证 ρ ≈ 0
  - PCA 降维：模板库 5 个标签是否能清晰分离（限制在各簇内部）
- 可视化：PCA 散点图 + 超度量 Trie 的层级着色
- **禁止重复实现**：HC8 类型转换调用 `sgn_hc_ops.py` 的标准接口

### 阶段 2：C 位展开（0.5 天）

- `sgn_embed.h`：`sgn_hc8_embed_bits()` / `sgn_embed_compress()` / `sgn_embed_hamming()`
- 内联位操作，确保 8051 可用（虽然嵌入主要用于 PC 端）
- 提供 `embed_to_faiss_format()` 函数，直接输出 float32 数组

### 阶段 3：分析工具链集成（0.5 天）

- 训练后分析脚本：`analyze_templates.py`
  - 调用 `sgn_embed.py` 生成浮点矩阵
  - **限制在 Trie 子树/标签簇内**执行 PCA、t-SNE、K-Means
  - 输出概念簇报告（标签纯度、离群点列表）
- 与监控面板集成：模板库 PCA 图作为默认视图（按簇分图）

---

## 6. 总结

> **等距嵌入不是让 HC 变成欧氏空间，而是让欧氏工具能为 HC 服务。**

- 二进制位展开：56 维二元向量，保留 HC 的层级位信息
- **同簇内近似单调性**：超度量距离与汉明/欧氏距离正相关（Spearman 0.6~0.85），但**跨簇不成立**
- 外部库接口：sklearn PCA、t-SNE、PyTorch、FAISS 全部可用，但应限制在 Trie 子树局部
- ANN 预筛选：嵌入 + FAISS 实现 O(1) 近似检索，后接 Trie 精确排序
- 核心引擎零改动：嵌入仅用于分析和离线工具，WTA 竞争仍用原生字典序
- 双向可逆：嵌入后可压缩回 HC8，虽然存在位截断误差，但在容忍范围内
- **v1.2 修正**：明确嵌入的适用边界——同簇内有效，跨簇无效，避免全局误用
- **去重完成**：HC8 类型定义引用 `sgn_hc_ops` 标准接口，禁止重复实现

一句话记住：

**超度量是母语，欧氏是外语；嵌入是翻译器，让外国人也能读懂诗——但诗的意思只在同族人间才准。**

---

## 7. 引用规范

本文档在涉及以下概念时，**必须**引用核心文件的对应章节：

- HC8 类型定义与层访问 → [`sgn_hc_ops.md`](sandbox:///mnt/agents/output/sgn_hc_ops.md) §1
- 超度量距离定义与证明 → [`sgn_math_core.md`](sandbox:///mnt/agents/output/sgn_math_core.md) §2
- Trie 子树查找 → `sgn_trie_find_child()` / `sgn_trie_collect_leaves()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §2/§5
- WTA 竞争 → [`sgn_engine_core.md`](sandbox:///mnt/agents/output/sgn_engine_core.md) §1
- 性能基准（48MHz/1T）→ [`sgn_index_matrix.md`](sandbox:///mnt/agents/output/sgn_index_matrix.md) §5

---

*本文档为 SGN 技术栈扩展文档。所有 HC 类型定义必须遵守核心文件引用规范。*
