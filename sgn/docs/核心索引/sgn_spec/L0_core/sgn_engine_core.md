
```markdown
# SGN 核心引擎规范 (sgn_engine_core)

> **版本**: v2.1 (数学澄清版)  
> **日期**: 2026-06-09  
> **性质**: SGN 技术栈唯一权威核心引擎源  
> **合并来源**: 《WTA竞争算子》《软阈值稀疏诱导》《LRU计数器》《形态学膨胀腐蚀》《字典序排序网络》  
> **核心原则**: 本文档统一定义 WTA 竞争、软阈值衰减、LRU 淘汰、模板合并、形态学操作。所有 Trie/HC 基础运算禁止重复实现，必须引用 sgn_trie_core / sgn_hc_ops。

---

## 1. WTA 竞争算子（Winner-Take-All）

### 1.1 两级检索结构（调用 Trie 核心）

| 层级 | 作用 | 复杂度 | 输出 |
|------|------|--------|------|
| **第一级：Trie 前缀剪枝** | 按 HC 前 `k` 层定位候选子树 | `O(k)` = `O(L)` | `C` 个候选（通常 1~3） |
| **第二级：精确汉明排序** | 对候选计算 `match_bits` 并排序 | `O(C·D)` | Top-K 赢家 |

**总复杂度**: `O(L + C·D)`，与模板库总规模 `N` **无关**。

```cpp
// WTA 竞争 —— 仅保留业务逻辑，Trie 操作调用 sgn_trie_core
void sgn_wta_compete(const HCTrieNode* template_trie,
                     const hc8_t* query_sig,
                     uint8_t K,  // Top-K
                     uint16_t* winner_ids,
                     uint8_t* winner_sims) {
    // 第一级：Trie 剪枝（调用 sgn_trie_core §5.2）
    uint16_t candidates[SGN_MAX_CANDIDATES];
    uint16_t cand_count;
    sgn_trie_collect_candidates(template_trie, query_sig, 2, candidates, &cand_count);

    // 第二级：精确汉明竞争排序
    typedef struct {
        uint16_t tid;
        uint8_t  sim;
    } match_entry_t;
    match_entry_t buf[SGN_MAX_CANDIDATES];
    uint8_t n = 0;

    for (uint8_t i = 0; i < cand_count; ++i) {
        uint16_t tid = candidates[i];
        uint8_t sim = sgn_match_bits_hamming(tid, query_sig);  // popcount
        buf[n++] = (match_entry_t){tid, sim};
    }

    // 确定性选择排序（K 很小，通常 1~3，O(C·K) 足够）
    for (uint8_t i = 0; i < K && i < n; ++i) {
        uint8_t best_idx = i;
        for (uint8_t j = i + 1; j < n; ++j) {
            if (buf[j].sim > buf[best_idx].sim) best_idx = j;
        }
        match_entry_t tmp = buf[i];
        buf[i] = buf[best_idx];
        buf[best_idx] = tmp;
        winner_ids[i] = buf[i].tid;
        winner_sims[i] = buf[i].sim;
    }
}
```

关键: sgn_trie_collect_candidates() 定义见 [sgn_trie_core §5.2]。WTA 规范禁止重复实现 Trie 遍历代码。

1.2 性能基准（48MHz/1T 修正值）

操作 旧估算(24MHz/12T) 修正值(48MHz/1T)
Trie 单层查找 20.0 μs 0.83 μs
WTA Trie 剪枝(定位候选) 300.0 μs 12.5 μs
WTA 精确排序(Top3, C≈3) 100.0 μs 4.17 μs
WTA 总耗时 ~400 μs ~16.7 μs

每步 1~3 次 WTA = 50 μs（原估算严重过时）。

---

2. 软阈值稀疏诱导

2.1 单侧软阈值（调用 HC 运算核心）

```cpp
// 软阈值全局衰减 —— 调用 sgn_hc8_soft_threshold() [sgn_hc_ops §5]
void sgn_soft_decay_all(hc8_t* hit_counters, uint16_t N, const hc8_t* Lambda) {
    for (uint16_t i = 0; i < N; ++i) {
        hit_counters[i] = sgn_hc8_soft_threshold(hit_counters[i], *Lambda);
    }
}
```

超度量兼容性: 软阈值是单调平移，X > Y > Lambda ⇒ X-Lambda > Y-Lambda，竞争排序不变，不撕裂超度量球。证明见 [sgn_math_core §2.2]。

数学注记：饱和加法（见 sgn_hc8_add_saturated）构成半环，因此软阈值衰减在仅 HC 模式下的收敛性分析依赖于该半环性质，而非群结构。对于需要逆元的场景（如梯度更新），应使用补码 SHC8（阿贝尔群）。

2.2 休眠区模型

软阈值将计数器分为三区：

· 活跃区（C_i > Lambda）: 正常参与竞争，保留完整梯度
· 休眠区（0 < C_i ≤ Lambda）: 不参与 WTA，但保留唤醒可能
· 死区（C_i = 0）: 立即淘汰候选

2.3 性能基准（48MHz/1T 修正值）

策略 旧估算(24MHz/12T) 修正值(48MHz/1T)
软阈值 HC8 (Lambda=任意) 24.0 μs 1.0 μs
软阈值 HC8 (Lambda=2幂次,位掩码) 8.0 μs 0.33 μs
每步 80 模板总开销 0.67 ms 26.4 μs

严重修正: 旧文档声称"每步 0.67ms"是基于 24MHz/12T 的错误估算。48MHz/1T 下标准配置每步仅 115 μs。

---

3. LRU 淘汰计数器

3.1 命中更新（调用 HC 饱和加法）

```cpp
// 命中时：计数器 += Delta（调用 sgn_hc8_add_saturated() [sgn_hc_ops §3]）
void sgn_lru_hit(hc8_t* counter) {
    hc8_t delta = HC8_ZERO;
    delta.frac[0] = LRU_DELTA_FRAC0;  // 例如 4/256 ≈ 0.0156
    *counter = sgn_hc8_add_saturated(*counter, delta);
}
```

3.2 全局衰减（调用软阈值）

```cpp
// 每步训练后：所有计数器软阈值递减
void sgn_lru_decay_all(hc8_t* counters, uint16_t N) {
    hc8_t lambda = HC8_ZERO;
    lambda.frac[0] = LRU_LAMBDA_FRAC0;  // 例如 2/256 ≈ 0.0078
    for (uint16_t i = 0; i < N; ++i) {
        counters[i] = sgn_hc8_soft_threshold(counters[i], lambda);
    }
}
```

3.3 淘汰决策（字典序最小）

```cpp
// 查找淘汰候选：字典序最小且非核心（未饱和）
uint16_t sgn_lru_find_evict(const hc8_t* counters, uint16_t N, const uint8_t* is_core) {
    uint16_t best = 0xFFFF;
    hc8_t min_val = HC8_MAX;
    for (uint16_t i = 0; i < N; ++i) {
        if (is_core[i]) continue;
        if (sgn_hc8_less(counters[i], min_val)) {  // 调用 [sgn_hc_ops §2]
            min_val = counters[i];
            best = i;
        }
    }
    return best;
}
```

3.4 核心模板标记

```cpp
// 命中次数超过阈值或计数器饱和的模板标记为核心（长期记忆保护）
void sgn_lru_promote_core(const hc8_t* counters, uint8_t* is_core,
                          uint16_t N, uint8_t threshold_int) {
    for (uint16_t i = 0; i < N; ++i) {
        if (counters[i].int_part >= threshold_int) {
            is_core[i] = 1;
        }
    }
}
```

3.5 性能基准（48MHz/1T 修正值）

操作 旧估算 修正值
单次命中更新 ~30 周期 1.25 μs
全局衰减 80 模板 1.2 ms 50.0 μs
维护排序（插入排序） 0.8 ms 33.3 μs
淘汰查询 ~10 周期 0.4 μs
每步总开销 167 μs ~50 μs

---

4. 模板合并（OR/AND）

4.1 OR 合并（相似模板扩展覆盖）

```cpp
// sim >= OR_T (85%): 合并为并集
void sgn_merge_or(uint8_t* mask_a, const uint8_t* mask_b, uint8_t D) {
    for (uint8_t i = 0; i < D/8; ++i) {
        mask_a[i] |= mask_b[i];
    }
}
```

4.2 AND 合并（相似模板收缩核心）

```cpp
// sim >= AND_T (80%): 合并为交集
void sgn_merge_and(uint8_t* mask_a, const uint8_t* mask_b, uint8_t D) {
    for (uint8_t i = 0; i < D/8; ++i) {
        mask_a[i] &= mask_b[i];
    }
}
```

---

5. 形态学膨胀与腐蚀（调用 Trie 核心）

⚠️ 应用提示：形态学操作依赖于超度量球的子树定义。在仅 HC 模式下，球结构稳定；在合体模式下，若存在 level 进位，球可能撕裂，形态学结果需谨慎解读。详细性质见 [sgn_math_core §5.3]。

5.1 超度量球的 Trie 定义

```
球 B(X, r) = { Y | d(X,Y) ≤ r }
           = { Y | X 与 Y 的前 k 层完全相同 }
           = Trie 中以 X 的前 k 层为路径的子树下的所有叶子
```

其中 r = 256^{-k}。证明见 [sgn_math_core §2.1]。

5.2 膨胀（调用 Trie 收集）

```cpp
// 膨胀: 标记集合 S 中所有模板的前 k 层路径，收集子树叶子
// 调用 sgn_trie_mark_subtree() 和 sgn_trie_collect_marked() [sgn_trie_core §5]
void sgn_dilate(const HCTrieNode* trie, const uint16_t* active_ids,
                uint16_t N, int k, uint16_t* result_ids, uint16_t* result_count) {
    uint8_t marked[SGN_TRIE_MAX_NODES] = {0};
    for (uint16_t i = 0; i < N; ++i) {
        sgn_trie_mark_path(trie, active_ids[i], k, marked);
    }
    sgn_trie_collect_marked(trie, marked, result_ids, result_count);
}
```

5.3 腐蚀（调用 Trie 收集）

```cpp
// 腐蚀: 叶子 X 保留当且仅当其 k 层邻域子树的所有叶子都在 S 中
void sgn_erode(const HCTrieNode* trie, const uint8_t* S_bitmap,
               int k, uint16_t* result_ids, uint16_t* result_count);
```

5.4 开运算与闭运算

```cpp
// 开运算: 先腐蚀去噪，再膨胀恢复
void sgn_open(const HCTrieNode* trie, uint8_t* S_bitmap, int k,
              uint16_t* result_ids, uint16_t* result_count) {
    uint16_t tmp[SGN_MAX_TEMPLATES];
    uint16_t tmp_count;
    sgn_erode(trie, S_bitmap, k, tmp, &tmp_count);
    // 将 tmp 转回 bitmap 后膨胀...
}

// 闭运算: 先膨胀连接，再腐蚀收缩
void sgn_close(const HCTrieNode* trie, uint8_t* S_bitmap, int k,
               uint16_t* result_ids, uint16_t* result_count);
```

5.5 性能基准（48MHz/1T 修正值）

操作 旧估算 修正值
形态学膨胀(80模板,k=2) 1.5 ms 62.5 μs
形态学腐蚀(80模板,k=2) 1.5 ms 62.5 μs

每 100 步执行 1 次 = 0.6 μs/步 均摊。

---

6. 字典序排序网络（硬件并行）

6.1 双调排序网络（Bitonic Sorter）

对于 N = 2^m 个元素：

· 深度: m(m+1)/2 = O(log²N)
· 比较器总数: N·log²N / 2
· 阶段数: m 个合并阶段

```cpp
// HC 字典序比较器 —— 调用 sgn_hc8_less() [sgn_hc_ops §2]
static inline void sgn_sortnet_compare_swap(hc8_t* a, hc8_t* b) {
    if (sgn_hc8_less(b, a)) {  // 若 b < a，交换
        hc8_t tmp = *a;
        *a = *b;
        *b = tmp;
    }
}
```

关键优势: 比较器排列完全固定，编译器可展开为无分支机器码，消除所有分支预测失败。

6.2 性能基准（48MHz/1T 修正值）

算法 比较器总数 深度 48MHz/1T 时间(N=256)
双调排序网络 2048（固定） 8 512 μs
奇偶转置(N=256) O(N²) O(N) ~5 ms
快速排序 O(N log N) O(log N) ~3 ms（分支预测失败）

修正: 旧文档"1.0ms"已按 24MHz 估算，48MHz/1T 下为 512 μs。

---

7. 功能选择决策矩阵（唯一权威）

场景 首选 备选 不选
模板检索(已知签名) Trie索引 — 霍夫曼码(仅用于传输压缩)
模板存在预筛 CBF — WTA(仅用于精确竞争)
全局衰减 软阈值 — LRU(仅用于淘汰决策)
淘汰决策 LRU — 软阈值(仅用于衰减)
在线聚类 Fréchet均值 — 形态学(仅用于离线清理)
离线清理 形态学开运算 — Fréchet(仅用于在线中心)

---

8. 引用规范

任何上层规范在涉及以下功能时，必须调用本文档定义的接口：

· WTA 竞争 → sgn_wta_compete() —— §1
· 软阈值衰减 → sgn_soft_decay_all() —— §2
· LRU 淘汰 → sgn_lru_find_evict() —— §3
· 模板合并 → sgn_merge_or() / sgn_merge_and() —— §4
· 形态学操作 → sgn_dilate() / sgn_erode() —— §5
· 排序网络 → sgn_sortnet_compare_swap() —— §6

---

本文档为 SGN 技术栈的核心引擎 ABI 契约。所有竞争、衰减、淘汰、合并操作必须遵守上述接口。

```

---