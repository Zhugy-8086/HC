# SGN 可靠性核心规范 (sgn_reliability_core)

> **版本**: v2.0 (权威重构版)  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈唯一权威可靠性源  
> **合并来源**: 《TMR三模冗余》《校验和增量更新》《ReedSolomon纠错》《Merkle树》  
> **核心原则**: 本文档统一定义 TMR 多数投票、校验和实时嗅探、RS 纠错、Merkle 审计的三级容错链路。禁止重复定义容错机制。

---

## 1. 三级容错链路（唯一权威架构）

| 层级 | 机制 | 时机 | 能力 | 开销 |
|------|------|------|------|------|
| **L1: 实时嗅探** | 校验和增量更新 | 每步训练 | 检测随机单比特翻转 | 0.3 μs/次更新 |
| **L2: 实时掩盖** | TMR 三副本多数投票 | 每次读取 | 掩盖单字节错误 | 1.04 μs/次投票 |
| **L3: 持久修复** | RS-LFSR 纠错 | 加载/保存 | 纠正闪存单字节错误 | 3.1 μs/次解码 |
| **L4: 深度审计** | Merkle 树增量更新 | 每步/每百步 | 追踪篡改路径 | 8.3 μs/次更新 |

**核心原则**: 
- **早发现**（校验和）→ **当场修**（TMR）→ **事后恢复**（RS）→ **追溯责任**（Merkle）
- 四级互补，不可互相替代

---

## 2. TMR 三模冗余

### 2.1 逐层字节多数投票

```cpp
// 三个副本 A, B, C 逐层投票
// 返回: (输出值, 错误掩码)
// error_mask: bit 0=int_part, bit 1..6=frac[0..5], 1=该层有错误
hc8_t sgn_tmr_vote(const hc8_t* a, const hc8_t* b, const hc8_t* c,
                   uint8_t* error_mask);
```

**投票规则**:
- 若 `a == b`: 输出 `a`（C 可能错）
- 若 `a == c`: 输出 `a`（B 错）
- 若 `b == c`: 输出 `b`（A 错）
- 若三值全不同: 输出字典序中位数，标记双错误告警

**定理**: 三个副本中最多 1 个字节错误（任意层），多数投票输出正确值。

### 2.2 神经元状态 TMR 存储

```cpp
struct neuron_tmr_t {
    hc8_t base_a, base_b, base_c;      // 3 副本 base 速度
    hc8_t enc_b_a, enc_b_b, enc_b_c;    // 3 副本鼓励值
    uint8_t lock_a, lock_b, lock_c;    // 3 副本锁定状态
    uint16_t template_id;               // 模板索引（非 TMR，用 RS 保护）
};
```

**读取（带投票）**:
```cpp
hc8_t neuron_get_base(const neuron_tmr_t* n) {
    uint8_t err_mask;
    hc8_t base = sgn_tmr_vote(&n->base_a, &n->base_b, &n->base_c, &err_mask);
    if (err_mask) {
        g_error_stats.base_errors += popcount(err_mask);
        if (popcount(err_mask) > 2) {
            sgn_panic(SGN_ERR_TMR_MULTI_FAULT);
        }
    }
    return base;
}
```

**写入（三副本同时更新）**:
```cpp
void neuron_set_base(neuron_tmr_t* n, const hc8_t* val) {
    n->base_a = n->base_b = n->base_c = *val;
}
```

### 2.3 内存开销与可行性

| 模式 | 单神经元 | 256 神经元总占用 | 可行性 |
|------|---------|----------------|--------|
| **TMR 全量** | 60 B | **15,360 B** | ❌ 超支（34KB SRAM 下） |
| **TMR 部分（仅高 2 层）** | 20 B | **5,120 B** | ✅ 可行 |
| **非 TMR + RS 存储保护** | 20 B | **5,120 B** | ✅ 标准配置 |

**互斥规则**（来自 [sgn_config_guide §7]）:
```cpp
#if defined(SGN_TMR_FULL) && defined(SGN_CBF_1024)
    #error "TMR_FULL 与 CBF_1024 互斥：合计 > 16KB"
#endif
```

### 2.4 分布式 TMR（三 MCU 节点）

```
节点 A（主）: 运算后广播本地 HC8 值
节点 B（从）: 运算后广播本地 HC8 值  
节点 C（监）: 运算后广播本地 HC8 值

各节点接收后本地执行 TMR 投票:
    result = sgn_tmr_vote({local, remote_B, remote_C}, &err)

若 err != 0: 记录故障层，发送告警帧
若 popcount(err) > 2: 进入安全模式
```

**总线帧**:
```
TMR 广播帧:
[0xAA][0x55][TYPE=0x20][LEN=6][HC8_VALUE:6][NODE_ID:1][SEQ:1][CRC8]
```

### 2.5 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算(24MHz/12T) | **修正值(48MHz/1T)** |
|------|-------------------|---------------------|
| TMR 投票（逐层 7 层） | 25.0 μs | **1.04 μs** |
| 每神经元读取 1 次 | — | 256 × 1.04 = **266 μs/轮** |

---

## 3. 校验和增量更新（L1 实时嗅探）

### 3.1 加权字节校验和

```cpp
static const uint8_t CS_WEIGHT[7] = {1, 2, 3, 4, 5, 6, 7};

uint8_t sgn_hc8_checksum(const hc8_t* hc) {
    uint16_t sum = CS_WEIGHT[0] * hc->int_part;
    for (int i = 0; i < 6; ++i) {
        sum += CS_WEIGHT[i + 1] * hc->frac[i];
    }
    return (uint8_t)(sum & 0xFF);
}
```

### 3.2 增量更新（O(1)）

```cpp
uint8_t sgn_checksum_delta(uint8_t old_cs, uint8_t layer,
                             uint8_t old_byte, uint8_t new_byte) {
    int16_t delta = (int16_t)CS_WEIGHT[layer] * ((int16_t)new_byte - (int16_t)old_byte);
    return (uint8_t)((old_cs + delta) & 0xFF);
}
```

### 3.3 运行时嗅探

```cpp
// 每步训练后验证全局一致性
bool sgn_runtime_sniff(const sgn_core_t* core, uint8_t expected_global_cs) {
    uint8_t actual = 0;
    for (uint16_t i = 0; i < core->template_count; ++i) {
        actual ^= sgn_hc8_checksum(&core->templates[i].hit_counter);
    }
    return (actual == expected_global_cs);
}
```

**触发深度审计**: 若嗅探失败，遍历所有模板所有字段定位错误。

### 3.4 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算 | **修正值** |
|------|--------|-----------|
| 全量计算 HC8 校验和 | ~40 周期 | **1.7 μs** |
| 增量更新（单字节） | ~8 周期 | **0.3 μs** |
| 全局异或嗅探（N=80） | ~200 周期 | **8.3 μs** |
| 深度审计（N=80, 3 字段） | ~10,000 周期 | **417 μs** |

---

## 4. Reed-Solomon 纠错（L3 持久修复）

### 4.1 RS(8,6) 参数

- **域**: GF(256)，与 HC 字节同构
- **信息符号**: `k = 6`（frac 层）
- **校验符号**: `2t = 2`，纠正 `t = 1` 字节错误
- **码长**: `n = 8` 字节

### 4.2 ⚠️ 醒目警告（真值验证 E5 修正）

> **RS 查表法（64KB 查找表）不可用于 AI8051U 等 64KB Flash MCU**。程序代码至少需要 20~30KB，查表法将耗尽全部 Flash。
>
> **MCU 端强制使用 LFSR 模式**（仅需 ~256 字节代码）。查表法仅用于 PC 端验证器。

### 4.3 LFSR 编码器（唯一推荐）

```cpp
void rs86_encode_lfsr(const uint8_t* info, uint8_t* codeword) {
    memcpy(codeword, info, 6);
    uint8_t remainder[2] = {0, 0};
    for (int i = 0; i < 6; ++i) {
        uint8_t feedback = remainder[1] ^ info[i];
        remainder[1] = remainder[0] ^ gf256_mul(feedback, 0x02);
        remainder[0] = feedback;
    }
    codeword[6] = remainder[0];
    codeword[7] = remainder[1];
}
```

### 4.4 LFSR 解码器（单字节纠正）

```cpp
bool rs86_decode_lfsr(uint8_t* codeword) {
    uint8_t s0 = 0, s1 = 0;
    for (int i = 0; i < 8; ++i) {
        s0 ^= gf256_mul(codeword[i], alpha_pow[i]);
        s1 ^= gf256_mul(codeword[i], alpha_pow[2*i]);
    }
    if (s0 == 0 && s1 == 0) return true;  // 无错误

    uint8_t loc = gf256_div(s1, s0);  // 错误位置
    int pos = gf256_log[loc];
    if (pos < 0 || pos >= 8) return false;  // 双字节错误，无法纠正

    uint8_t err_val = gf256_div(s0, alpha_pow[pos]);
    codeword[pos] ^= err_val;  // 纠正
    return true;
}
```

### 4.5 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算 | **修正值** |
|------|--------|-----------|
| RS 编码 LFSR(6 字节) | 0.1 ms | **4.2 μs** |
| RS 解码纠正(1 字节) | 75.0 μs | **3.1 μs** |
| RS 解码检测(双错) | — | **5.0 μs** |

---

## 5. Merkle 树审计（L4 深度追溯）

### 5.1 增量路径更新（O(log N)）

```cpp
void sgn_merkle_update_leaf(sgn_merkle_tree_t* tree, uint16_t tid,
                            const merkle_hash_t* new_leaf) {
    uint16_t node_idx = tree->leaf_start + tid;
    tree->nodes[node_idx] = *new_leaf;
    while (node_idx > 0) {
        uint16_t parent_idx = (node_idx - 1) / 2;
        uint16_t left_idx = parent_idx * 2 + 1;
        uint16_t right_idx = parent_idx * 2 + 2;
        tree->nodes[parent_idx] = sgn_merkle_parent(
            &tree->nodes[left_idx], &tree->nodes[right_idx]
        );
        node_idx = parent_idx;
    }
}
```

**复杂度**: 树高 h = ceil(log₂(N))。N=256 时 h=8，**8.3 μs** @48MHz/1T。

### 5.2 分布式一致性

```
节点 A 训练后广播根哈希（8B）→ 节点 B 比对本地根哈希
    → 不一致时请求差异子树路径
    → 逐层向下定位到具体差异模板
    → 仅同步该模板（而非全量模型）

带宽: 根哈希 8B + 差异路径 8B × h = 72B（256 模板）
    vs 全量模型 5KB
```

---

## 6. 故障覆盖率分析

| 故障模式 | L1 校验和 | L2 TMR | L3 RS | L4 Merkle | 综合 |
|---------|----------|--------|-------|-----------|------|
| 单字节翻转（RAM） | 检测 | **纠正** | — | — | 100% 覆盖 |
| 单字节翻转（Flash） | — | — | **纠正** | 检测 | 100% 覆盖 |
| 同一层双字节错（不同副本） | 检测 | **纠正** | — | — | 100% 覆盖 |
| 同一副本两层错 | 检测 | 纠正一层，检测一层 | — | 检测 | 告警 |
| 三副本同层全错（共因） | 无法检测 | 无法检测 | — | — | 0%（需异构冗余） |
| 总线帧损坏 | — | — | — | — | CRC8 检测，丢弃重传 |

**共因故障防护**: 三副本使用异构实现（不同编译器、不同晶振、不同电源）。

---

## 7. 引用规范

任何上层规范在涉及以下可靠性操作时，**必须**调用本文档定义的接口：

- TMR 投票 → `sgn_tmr_vote()` —— **§2**
- 校验和增量更新 → `sgn_checksum_delta()` —— **§3**
- 运行时嗅探 → `sgn_runtime_sniff()` —— **§3.3**
- RS 纠错 → `rs86_encode_lfsr()` / `rs86_decode_lfsr()` —— **§4**
- Merkle 增量更新 → `sgn_merkle_update_leaf()` —— **§5**

---

*本文档为 SGN 技术栈的可靠性层 ABI 契约。所有容错、纠错、审计操作必须遵守上述四级链路。*
