好的，我将对 sgn_trie_core.md 和 sgn_engine_core.md 进行最小必要联动修改，主要增加注释框和引用更新，不改变 ABI。

---

修改一：sgn_trie_core.md → v2.1

```markdown
# SGN Trie 核心规范 (sgn_trie_core)

> **版本**: v2.1 (数学澄清版)  
> **日期**: 2026-06-09  
> **性质**: SGN 技术栈唯一权威索引与检索源  
> **合并来源**: 《超度量哈希与Trie索引》《WTA竞争算子》《形态学膨胀腐蚀》《霍夫曼前缀码》《DFA状态编码》  
> **核心原则**: 本文档统一定义超度量 Trie 的节点结构、查找/插入/删除/遍历接口。上层规范禁止重复实现 Trie 操作代码。

---

## 1. 节点结构（唯一权威定义）

### 1.1 基础节点

```cpp
// sgn_trie.h —— 统一节点定义

namespace sgn {

    // Trie 节点：与 HC 超度量树同构
    struct HCTrieNode {
        uint8_t  byte;           // 本层字节值（0~255），即分支键
        uint16_t template_id;    // 叶子节点存储模板索引（0xFFFF = 非叶子）
        uint16_t child_count;    // 子节点数量（用于稀疏存储遍历）
        // 子节点存储策略三选一（编译期宏决定）
        union {
            // 策略 A: 开放寻址哈希表（推荐，平均 1~3 子节点）
            struct {
                HCTrieNode* children;   // 动态数组（需配合内存池）
                uint16_t    cap;        // 容量
            } hash;
            // 策略 B: 排序数组 + 二分查找
            struct {
                HCTrieNode** sorted;    // 按 byte 排序的指针数组
                uint16_t    n;          // 实际子节点数
            } sorted;
            // 策略 C: 固定 256 指针数组（最快，但内存大）
            HCTrieNode* direct[256];
        } storage;
    };

    // 内存池节点（用于静态分配，避免 malloc）
    struct TrieNodePool {
        HCTrieNode nodes[SGN_TRIE_MAX_NODES];
        uint16_t   next_free;
    };

} // namespace sgn
```

关键约束：

· byte 字段即 HC 路径上的第 k 层字节值
· template_id = 0xFFFF 标识内部节点；叶子节点存储实际模板索引
· 子节点数极少（通常 1~3），严禁使用 256 长度的全指针数组（除非编译期明确开启 SGN_TRIE_DIRECT_MODE）

1.2 节点操作宏

```cpp
#define IS_LEAF(node)     ((node)->template_id != 0xFFFF)
#define IS_INTERNAL(node) ((node)->template_id == 0xFFFF)
```

---

2. 查找接口（唯一权威实现）

2.1 按字节查找子节点

```cpp
// 在 node 的子节点中查找 byte == target 的节点
// 返回 nullptr 若不存在
HCTrieNode* sgn_trie_find_child(const HCTrieNode* node, uint8_t target);
```

实现策略差异：

策略 适用场景 时间复杂度 空间/节点
开放寻址 (默认) 子节点数 1~5 O(1) 平均 ~8B + 子节点指针
排序数组+二分 子节点数 5~20 O(log n) ~4B × n 指针
直接数组 子节点数 >50 或追求极致速度 O(1) 256 × 4B = 1KB

默认实现（开放寻址）：

```cpp
HCTrieNode* sgn_trie_find_child(const HCTrieNode* node, uint8_t target) {
    if (!node || node->child_count == 0) return nullptr;
    // 线性扫描（child_count 极小，线性比哈希更快）
    for (uint16_t i = 0; i < node->child_count; ++i) {
        if (node->storage.hash.children[i].byte == target) {
            return &node->storage.hash.children[i];
        }
    }
    return nullptr;
}
```

2.2 按 HC 路径查找叶子

```cpp
// 从 root 出发，沿 hc 的前 'depth' 层路径查找叶子
// 返回叶子节点指针；若路径中断，返回路径上最深的可达节点
HCTrieNode* sgn_trie_find_path(HCTrieNode* root, const hc8_t* hc, int depth);

// 实现
HCTrieNode* sgn_trie_find_path(HCTrieNode* root, const hc8_t* hc, int depth) {
    HCTrieNode* node = root;
    for (int layer = 0; layer < depth; ++layer) {
        uint8_t b = (layer == 0) ? hc->int_part : hc->frac[layer - 1];
        HCTrieNode* child = sgn_trie_find_child(node, b);
        if (!child) return node;  // 路径中断，返回当前节点
        node = child;
    }
    return node;  // 到达深度 depth
}
```

---

3. 插入接口（唯一权威实现）

3.1 插入子节点

```cpp
// 在 parent 下插入 byte = b 的子节点（若已存在则返回现有节点）
// 使用内存池分配
HCTrieNode* sgn_trie_insert_child(HCTrieNode* parent, uint8_t b, TrieNodePool* pool);
```

实现（开放寻址扩容）：

```cpp
HCTrieNode* sgn_trie_insert_child(HCTrieNode* parent, uint8_t b, TrieNodePool* pool) {
    // 先查找是否已存在
    HCTrieNode* existing = sgn_trie_find_child(parent, b);
    if (existing) return existing;

    // 从内存池分配新节点
    if (pool->next_free >= SGN_TRIE_MAX_NODES) return nullptr;  // 池满
    HCTrieNode* child = &pool->nodes[pool->next_free++];
    child->byte = b;
    child->template_id = 0xFFFF;
    child->child_count = 0;

    // 追加到父节点的子节点数组
    uint16_t n = parent->child_count;
    // 假设 children 数组已预分配足够空间（由内存池策略保证）
    parent->storage.hash.children[n] = *child;
    parent->child_count++;

    return &parent->storage.hash.children[n];
}
```

3.2 插入模板签名

```cpp
// 将模板签名 sig 插入 Trie，挂载模板索引 tid
// 路径长度 = SGN_HPDC_FRAC_LAYERS + 1 (int_part + frac 层)
bool sgn_trie_insert_template(HCTrieNode* root, const hc8_t* sig,
                              uint16_t tid, TrieNodePool* pool);

// 实现
bool sgn_trie_insert_template(HCTrieNode* root, const hc8_t* sig,
                              uint16_t tid, TrieNodePool* pool) {
    HCTrieNode* node = root;
    for (int layer = 0; layer <= SGN_HPDC_FRAC_LAYERS; ++layer) {
        uint8_t b = (layer == 0) ? sig->int_part : sig->frac[layer - 1];
        HCTrieNode* child = sgn_trie_insert_child(node, b, pool);
        if (!child) return false;
        node = child;
    }
    node->template_id = tid;  // 到达叶子，挂载模板 ID
    return true;
}
```

复杂度：O(L)，L = 6（固定），与模板库总规模无关。

---

4. 删除与更新接口

4.1 删除子树（惰性删除）

```cpp
// 标记删除：将叶子 template_id 设为 0xFFFF（不回收内存）
// SGN 模板库以追加和合并为主，极少物理删除
void sgn_trie_lazy_delete(HCTrieNode* leaf) {
    leaf->template_id = 0xFFFF;
}

// 物理回收（仅在内存池紧张时由后台任务执行）
void sgn_trie_compact(TrieNodePool* pool, HCTrieNode* root);
```

4.2 更新叶子 ID

```cpp
// 合并模板时，更新叶子挂载的模板索引
void sgn_trie_update_tid(HCTrieNode* leaf, uint16_t new_tid) {
    leaf->template_id = new_tid;
}
```

---

5. 遍历与收集接口

5.1 收集子树所有叶子

```cpp
// DFS 收集 node 子树下的所有叶子模板 ID
void sgn_trie_collect_leaves(const HCTrieNode* node, uint16_t* out_ids, uint16_t* out_count);

// 实现（递归，深度 ≤ 7，栈安全）
void sgn_trie_collect_leaves(const HCTrieNode* node, uint16_t* out_ids, uint16_t* out_count) {
    if (!node) return;
    if (IS_LEAF(node)) {
        out_ids[(*out_count)++] = node->template_id;
        return;
    }
    for (uint16_t i = 0; i < node->child_count; ++i) {
        sgn_trie_collect_leaves(&node->storage.hash.children[i], out_ids, out_count);
    }
}
```

5.2 前缀剪枝候选收集

```cpp
// 第一级 WTA 剪枝：收集与 query 前 k 层路径相同的所有叶子
void sgn_trie_collect_candidates(const HCTrieNode* root, const hc8_t* query,
                                 int k, uint16_t* cand_ids, uint16_t* cand_count);

// 实现
void sgn_trie_collect_candidates(const HCTrieNode* root, const hc8_t* query,
                                 int k, uint16_t* cand_ids, uint16_t* cand_count) {
    *cand_count = 0;
    const HCTrieNode* node = root;
    for (int layer = 0; layer < k; ++layer) {
        uint8_t b = (layer == 0) ? query->int_part : query->frac[layer - 1];
        HCTrieNode* child = sgn_trie_find_child(node, b);
        if (!child) return;  // 前 k 层无匹配，候选集为空
        node = child;
    }
    // 到达深度 k，收集该子树所有叶子
    sgn_trie_collect_leaves(node, cand_ids, cand_count);
}
```

复杂度：O(k + C)，k 为剪枝深度（固定 2~3），C 为候选数（通常 1~3）。

---

6. 内存池管理

6.1 静态内存池（推荐）

```cpp
// 静态分配，无 malloc，编译期确定上限
#define SGN_TRIE_MAX_NODES  2048   // 支持 1024 模板 + 内部节点

TrieNodePool g_trie_pool;

void sgn_trie_pool_init() {
    g_trie_pool.next_free = 1;  // 节点 0 保留为根节点
    g_trie_pool.nodes[0].byte = 0;
    g_trie_pool.nodes[0].template_id = 0xFFFF;
    g_trie_pool.nodes[0].child_count = 0;
}

HCTrieNode* sgn_trie_alloc_node() {
    if (g_trie_pool.next_free >= SGN_TRIE_MAX_NODES) return nullptr;
    return &g_trie_pool.nodes[g_trie_pool.next_free++];
}
```

6.2 内存占用估算

模板规模 节点数 (模板+内部) 每节点大小 总内存
64 ~120 ~8B ~960 B
128 ~255 ~8B ~2 KB
256 ~511 ~8B ~4 KB
512 ~1023 ~8B ~8 KB
1024 ~2047 ~8B ~16 KB

注意：上表假设开放寻址策略。若使用直接数组策略，内存膨胀 128 倍（每节点 1KB），不可接受。

---

7. 超度量性质的边界说明

⚠️ 重要提示

本文件定义的 Trie 索引依赖 HC 数的字典序比较和超度量球结构。以下性质必须明确：

· 仅 HC 模式（level=0）：树深度有限，超度量球完整，Trie 索引是完备且紧的。
· 合体模式（level 可变）：当 int_part 溢出进位到 level 时，超度量球可能撕裂，Trie 索引在该场景下不保证原有的邻域完备性。实际使用中，Trie 索引应限制在仅 HC 模式或合体模式下 level 固定不变的数据集。

详细数学证明见 sgn_math_core §5.2–5.3。

---

8. 与上层功能的协同接口

8.1 WTA 竞争算子调用方式

```cpp
// WTA 规范应删除所有 Trie 实现代码，仅保留以下调用：

// 第一级：Trie 前缀剪枝（O(k)）
uint16_t candidates[SGN_MAX_CANDIDATES];
uint16_t cand_count;
sgn_trie_collect_candidates(&g_template_trie, &query_hc, 2, candidates, &cand_count);

// 第二级：对 candidates 执行精确汉明排序（O(C·D)）
// ... WTA 规范仅保留此处的排序逻辑 ...
```

8.2 形态学膨胀/腐蚀调用方式

```cpp
// 形态学规范应删除所有 Trie 遍历代码，仅保留以下调用：

// 膨胀：标记活跃模板的前 k 层路径节点
sgn_trie_mark_subtree(&g_template_trie, active_sig, k, marked_bitmap);

// 收集被标记子树的叶子
sgn_trie_collect_marked(&g_template_trie, marked_bitmap, result_ids, result_count);
```

8.3 霍夫曼前缀码调用方式

```cpp
// 霍夫曼规范应删除所有 Trie 建树代码，仅保留以下调用：

// 遍历 Trie 统计子树模板数
uint16_t leaf_count = sgn_trie_count_leaves(subtree_root);
if (leaf_count >= threshold) {
    // 在该节点截断，生成簇短码
    // ... 霍夫曼规范仅保留截断决策和编码逻辑 ...
}
```

8.4 DFA 状态编码调用方式

```cpp
// DFA 规范应删除所有子节点查找代码，仅保留以下调用：

// DFA 转移 = Trie 子节点查找
dfa_state_t* next = sgn_trie_find_child(current_state, input_byte);
if (!next) next = default_state;
```

---

9. 性能基准（48MHz/1T 修正值）

操作 机器周期 48MHz/1T 时间
单层 Trie 查找 40 0.83 μs
模板插入（L=6） 240 5.0 μs
候选收集（k=2, C=3） 600 12.5 μs
叶子收集（单个子树） 80 1.67 μs
路径查找（深度 6） 240 5.0 μs

---

10. 引用规范

任何上层规范在涉及以下操作时，必须调用本文档定义的接口，禁止重复实现 Trie 节点结构或查找逻辑：

· 子节点查找 → sgn_trie_find_child() —— §2
· 模板插入 → sgn_trie_insert_template() —— §3
· 叶子收集 → sgn_trie_collect_leaves() —— §5
· 候选剪枝 → sgn_trie_collect_candidates() —— §5.2
· 内存分配 → sgn_trie_alloc_node() —— §6

---

本文档为 SGN 技术栈的索引层 ABI 契约。所有检索、聚类、协议解析模块必须遵守上述节点结构与接口签名。

```

