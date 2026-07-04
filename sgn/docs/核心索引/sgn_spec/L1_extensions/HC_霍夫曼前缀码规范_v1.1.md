# HC 霍夫曼前缀码规范

> **版本**: v1.1（重构版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈编码压缩层规范  
> **状态**: 已去重，引用核心文件  
> **核心原则**: 本文档仅保留前缀码的压缩策略与协同流程。所有 Trie 遍历、子树统计、叶子收集操作禁止重复实现，必须调用 `sgn_trie_core` 定义的标准接口。

---

## 0. 问题起源

当前 SGN 的模板索引使用**定长编码**：

```cpp
// 模板 ID 直接作为数组索引
uint16_t tid;  // 固定 2 字节
```

当模板库稳态 80 个时，定长索引效率良好。但当规模扩展到 1024+（分布式节点合并后），面临：

| 问题 | 定长索引 | 需求 |
|------|---------|------|
| **内存膨胀** | 每个引用模板的神经元存 2 字节 tid | 高频模板应更短 |
| **传输开销** | UART 帧中模板 ID 固定 2 字节 | 高频模板可用 1 字节 |
| **缓存局部性** | 所有模板等概率访问 | 高频模板应常驻热缓存 |
| **概念层次显式化** | 索引是扁平数字 | 索引应反映概念层级 |

**核心洞察**：超度量 Trie 的路径本身就是**前缀码**——从根到叶子的字节序列唯一标识模板。若高频概念分支靠近根（短路径），低频分支深入树底（长路径），则平均码长最小化。这正是霍夫曼最优前缀码的思想，但 HC 树已天然提供树结构，无需重新构建。

---

## 1. 形式化模型

### 1.1 HC 前缀码定义

设模板库构建的超度量 Trie 中，从根到模板 `T` 的路径为：
```
P(T) = [p_0, p_1, ..., p_{L-1}]
```

其中 `p_i` 为第 `i` 层的字节值，`L` 为模板深度（通常 2~6）。

**前缀码性质**：
- 无码字是其他码字的前缀（Trie 路径的唯一性保证）
- 码字长度可变：高频模板可能 `L=2`，低频模板可能 `L=6`

### 1.2 最优截断深度

**定理**：若第 `k` 层某字节值 `b` 的子树包含 `n_b` 个模板，则将该子树截断为"虚拟叶子"（用短码代表整个子树）可节省的存储为：
```
ΔS = n_b * (L - k) * 8  位
```

**最优截断策略**：对 `n_b > threshold` 的子树在深度 `k` 处截断，生成"概念簇码"。

### 1.3 与 Fréchet 均值的协同

Fréchet 均值计算出的"概念中心"模板，恰好可作为截断子树的代表：
- 概念中心 = 短码（簇代表）
- 簇内成员 = 长码（完整路径）
- 查询时先匹配短码（概念筛选），再匹配长码（精确实例）

> **引用**: Fréchet 均值计算见 [`HC_超度量重心_Frechet均值规范_v1.1.md`](sandbox:///mnt/agents/output/HC_超度量重心_Frechet均值规范_v1.1.md)。

---

## 2. 算法实现（接口声明）

### 2.1 前缀码生成

```cpp
// sgn_huffman.h

// 前缀码表项
typedef struct {
    uint8_t  code[6];      // 码字节序列（路径）
    uint8_t  len;          // 码长（层数）
    uint16_t template_id;  // 叶子模板 ID（0xFFFF = 概念簇代表）
    uint8_t  is_cluster;   // 1 = 簇短码，0 = 实例长码
} hc_prefix_code_t;

// 统计各层字节频率，决定截断深度
// 实现要点：
//   1. 遍历 Trie，调用 sgn_trie_count_leaves() 统计子树模板数（sgn_trie_core §5）
//   2. 对子树模板数 > threshold 的节点生成短码
//   3. 收集路径时调用 sgn_trie_find_path()（sgn_trie_core §2.2）
// 禁止重复实现：所有 Trie 遍历调用 sgn_trie_core 标准接口
void sgn_hc_prefix_build(const HCTrieNode* trie_root,
                         uint16_t threshold,  // 截断阈值
                         hc_prefix_code_t* table,
                         uint16_t* table_size);
```

> **实现引用**: `sgn_hc_prefix_build()` 内部的子树模板统计调用 [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5 的 `sgn_trie_count_leaves()` 或 `sgn_trie_collect_leaves()`。禁止在本文档中重复实现 Trie 遍历代码。

### 2.2 编码/解码

```cpp
// 编码：模板 ID -> 前缀码
// 实现要点：查表匹配，O(table_size)
bool sgn_hc_prefix_encode(const hc_prefix_code_t* table, uint16_t table_size,
                          uint16_t tid, uint8_t* code, uint8_t* len);

// 解码：前缀码 -> 模板 ID（或簇代表）
// 实现要点：查表匹配，O(table_size)
bool sgn_hc_prefix_decode(const hc_prefix_code_t* table, uint16_t table_size,
                          const uint8_t* code, uint8_t len,
                          uint16_t* tid, uint8_t* is_cluster);
```

### 2.3 概念簇查询（Python 原型）

```python
# sgn_huffman.py

class HCPrefixCodec:
    def __init__(self, trie, threshold=5):
        self.table = []
        self._build(trie.root, [], threshold)

    def _build(self, node, path, threshold):
        # 调用 sgn_trie_count_leaves() 统计子树模板数（sgn_trie_core §5）
        leaf_count = sgn_trie_count_leaves(node)
        if leaf_count >= threshold and len(path) > 0:
            # 截断为簇短码
            self.table.append({
                'code': bytes(path),
                'len': len(path),
                'is_cluster': True,
                'members': sgn_trie_collect_leaves(node)  # sgn_trie_core §5
            })
            return

        for child in node.children:
            self._build(child, path + [child.byte], threshold)

    def encode(self, tid):
        # 找到包含 tid 的最短码（优先簇短码）
        for entry in sorted(self.table, key=lambda x: x['len']):
            if tid in entry.get('members', [entry.get('tid')]):
                return entry['code']
        raise KeyError("Template " + str(tid) + " not found")

    def decode(self, code):
        for entry in self.table:
            if entry['code'] == code:
                return entry
        raise KeyError("Code not found")

# 应用：神经元存储模板引用时，存短码而非 tid
# 高频概念（如数字 '0' 有 12 个模板）用 2 字节短码
# 低频概念（如特殊符号）用 6 字节长码
```

> **实现引用**: Python 原型中的 `sgn_trie_count_leaves()` 和 `sgn_trie_collect_leaves()` 调用 [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5 的标准接口。禁止重复实现。

---

## 3. 性能与压缩率

### 3.1 码长分布（模拟 256 模板）

| 概念 | 模板数 | 平均码长 | 定长对比 | 节省 |
|------|--------|----------|---------|------|
| '0'（高频） | 32 | 2 字节 | 2 字节 | 0% |
| '1'（中频） | 16 | 3 字节 | 2 字节 | -50% |
| 'A'（低频） | 4 | 5 字节 | 2 字节 | -150% |
| 异常（极低频） | 1 | 6 字节 | 2 字节 | -200% |
| **全局平均** | — | **2.8 字节** | **2 字节** | **-40%** |

**注意**：霍夫曼前缀码对**非均匀分布**有效。若所有模板等频访问，变长码反而膨胀。SGN 模板库通常符合幂律分布（少数概念高频），因此有效。

### 3.2 查询加速

| 查询方式 | 比较次数 | 缓存命中 | 适用场景 |
|---------|---------|---------|---------|
| 定长 tid 数组 | O(1) | 中 | 通用 |
| 前缀码 Trie | O(L_avg) | 高（短码常驻） | 高频概念 |
| 簇短码 + 实例长码 | O(1) + O(C) | 极高 | 概念筛选 |

---

## 4. 误区澄清（v1.1 修订）

| 误区 | 事实 |
|------|------|
| "前缀码比定长索引慢，因为变长" | 是，但高频模板用短码，实际平均查询更快（缓存局部性）。 |
| "和游程编码重复：都是压缩" | 不重复。RLE 压缩**相邻模板的空间冗余**，前缀码压缩**模板索引的统计冗余**。两者正交，可叠加。 |
| "截断子树会丢失精确模板信息" | 不会。短码仅用于**索引引用**，精确匹配仍需完整 HC 比较。短码是"信封"，长码是"信纸"。 |
| "需要构建霍夫曼树，增加复杂度" | 不需要。HC Trie 本身就是树，只需统计子树大小决定截断深度，无额外建树。 |
| "所有模板等频时前缀码失效" | 正确。此时应关闭前缀码，回退定长索引。通过 `threshold` 参数控制。 |
| "和 Trie 索引冲突：都是树" | 不冲突。Trie 索引用于**检索**，前缀码用于**存储/传输**。同一棵树两种用法。 |

---

## 5. 实现路径

### 阶段 1：Python 验证器（0.5 天）

- `sgn_huffman.py`：实现 HC Trie 前缀码生成 + 编解码
- 验证：
  - 对训练后的模板库统计访问频率，生成最优截断表
  - 计算平均码长 vs 熵下界（验证最优性）
  - 对比：定长 2 字节 vs 变长前缀码的总存储
- **禁止重复实现**：Trie 遍历调用 `sgn_trie_core.py` 的标准接口

### 阶段 2：C 头文件（0.5 天）

- `sgn_huffman.h`：`sgn_hc_prefix_build()` / `sgn_hc_prefix_encode()` / `sgn_hc_prefix_decode()`
- 静态码表（编译期或启动期生成），运行时查表 O(1)
- **禁止重复实现**：子树统计调用 `sgn_trie_collect_leaves()`（`sgn_trie_core.h` §5）

### 阶段 3：神经元存储集成（0.5 天）

- 修改神经元结构：模板引用字段从 `uint16_t tid` 改为 `uint8_t code[6]` + `uint8_t code_len`
- 高频神经元（核心概念）存 2~3 字节短码，节省 RAM
- 配置项：`PREFIX_CODEC_ENABLE`（bool）、`PREFIX_THRESHOLD`（默认 5）

---

## 6. 总结

> **霍夫曼前缀码不是重新发明树，而是让现有树多长一层叶子。**

- HC Trie 路径即前缀码：从根到叶子的字节序列天然满足前缀性质
- 截断策略：子树模板数 > threshold 时生成簇短码，平均码长最小化
- 与 Fréchet 均值协同：均值是簇中心，短码是簇信封
- 变长存储：高频概念 2 字节，低频概念 6 字节，符合幂律分布的模板库
- 纯整数查表：编码/解码均为字节比较，无浮点、无哈希
- 与 RLE（水平压缩）、Merkle（完整性）形成完整压缩栈
- **去重完成**：所有 Trie 遍历调用 `sgn_trie_core` 标准接口，禁止重复实现

一句话记住：

**定长索引是均码衣服，前缀码是量身定做；胖子（高频）穿宽松短衣，瘦子（低频）穿合身长袍。**

---

## 7. 引用规范

本文档在涉及以下操作时，**必须**调用核心文件定义的接口，禁止重复实现：

- Trie 子树统计 → `sgn_trie_count_leaves()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5
- Trie 叶子收集 → `sgn_trie_collect_leaves()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §5
- Trie 路径查找 → `sgn_trie_find_path()` —— [`sgn_trie_core.md`](sandbox:///mnt/agents/output/sgn_trie_core.md) §2.2
- Fréchet 均值 → [`HC_超度量重心_Frechet均值规范_v1.1.md`](sandbox:///mnt/agents/output/HC_超度量重心_Frechet均值规范_v1.1.md)
- 内存互斥规则 → [`sgn_config_guide.md`](sandbox:///mnt/agents/output/sgn_config_guide.md) §7

---

*本文档为 SGN 技术栈扩展文档。所有 Trie 操作 ABI 调用必须遵守上述引用规范。*
