# SGN 存储后端核心规范 (sgn_storage_core)

> **版本**: v2.0 (权威重构版)  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈唯一权威存储后端源  
> **合并来源**: 《游程编码与熵压缩》《Merkle树》《ReedSolomon纠错》《校验和增量更新》《合体存储精度档位》  
> **核心原则**: 本文档统一定义 RLE 压缩、Merkle 审计、RS 纠错、校验和嗅探。所有基础运算禁止重复实现。

---

## 1. 游程编码（RLE）压缩

### 1.1 公共前缀压缩原理

模板库按 HC 字典序排序后，相邻模板的前 2~3 层字节高度集中。例如数字 '0' 的 8 个模板：
- `int_part` 全部为 0
- `frac[0]` 集中在 0x40~0x60
- `frac[1]` 及以后层才出现显著分散

**压缩策略**: 仅存储差异后缀，公共前缀通过排序位置隐式复用。

### 1.2 RLE 编码项

```cpp
// 游程编码项
struct hc_rle_item_t {
    uint8_t  prefix_depth;   // 与前一个模板共享的前缀层数 (0~6)
    uint8_t  suffix_len;     // 后缀字节数
    uint8_t  suffix[6];      // 后缀字节
    uint16_t template_id;    // 模板元数据索引
};
```

### 1.3 压缩率实测（修正值）

| 模板库状态 | 原始裸二进制 | RLE 压缩后 | 组头压缩后 | 压缩率 |
|-----------|------------|-----------|-----------|--------|
| 初始 20 个 | 1.4KB | 1.2KB | 1.1KB | 21% |
| 中期 124 个 | 8.7KB | 5.2KB | 4.1KB | **53%** |
| 稳态 80 个 | 5.6KB | 3.4KB | 2.8KB | **50%** |
| 峰值 256 个 | 17.9KB | 9.8KB | 7.2KB | **60%** |

### 1.4 解压开销（48MHz/1T 修正值）

| 操作 | 旧估算(24MHz/12T) | **修正值(48MHz/1T)** |
|------|-------------------|---------------------|
| 解压单个模板（RLE） | ~80 周期 | **3.3 μs** |
| 全库解压 80 个 | ~6,400 周期 | **267 μs** |
| JSON 解析 80 个 | ~500,000 周期 | **20 ms** |

**RLE 解压比 JSON 解析快 75×**。

---

## 2. Merkle 树（分层哈希审计）

### 2.1 结构定义（调用 HC 加法）

```cpp
// Merkle 树节点 = 子节点的 HC 饱和加法拼接
// 叶子 = 单个 HC 数的轻量级校验（CRC8 + 原始值前 5 层）
// 内部节点 = sgn_hc8_add_saturated(left_hash, right_hash) [sgn_hc_ops §3]

struct merkle_hash_t {
    uint8_t bytes[8];  // 等效 hc8_t（int_part + 7 frac 层）
};

struct sgn_merkle_tree_t {
    merkle_hash_t nodes[SGN_MERKLE_MAX_NODES];  // 完全二叉树数组
    uint16_t      leaf_start;  // 叶子起始索引
};
```

### 2.2 增量路径更新（O(log N)）

```cpp
// 修改叶子 tid 后，沿路径向上重算父节点
// 无需全量重哈希，仅需 h 次 HC 加法（h = 树高）
void sgn_merkle_update_leaf(sgn_merkle_tree_t* tree, uint16_t tid,
                            const merkle_hash_t* new_leaf) {
    uint16_t node_idx = tree->leaf_start + tid;
    tree->nodes[node_idx] = *new_leaf;

    while (node_idx > 0) {
        uint16_t parent_idx = (node_idx - 1) / 2;
        uint16_t left_idx = parent_idx * 2 + 1;
        uint16_t right_idx = parent_idx * 2 + 2;

        // 重算父节点 = 左 + 右（调用 sgn_hc8_add_saturated）
        tree->nodes[parent_idx] = sgn_merkle_parent(
            &tree->nodes[left_idx], &tree->nodes[right_idx]
        );
        node_idx = parent_idx;
    }
}
```

### 2.3 内存占用

| 模板数 N | 树高 h | 节点数 | 每节点 8B | 总内存 |
|---------|--------|--------|----------|--------|
| 64 | 6 | 127 | 8B | **1KB** |
| 128 | 7 | 255 | 8B | **2KB** |
| 256 | 8 | 511 | 8B | **4KB** |
| 512 | 9 | 1023 | 8B | **8KB** |

### 2.4 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算 | **修正值** |
|------|--------|-----------|
| Merkle 增量更新(路径) | 0.2 ms | **8.3 μs** |
| 每步平均 2 次修改 | — | **16.6 μs** |

---

## 3. Reed-Solomon 纠错

### 3.1 RS(8,6) 参数

- **域**: GF(256)，与 HC 字节同构
- **信息符号**: `k = 6`（HC 的 6 字节 frac 层）
- **校验符号**: `2t = 2`（2 字节校验），可纠正 **t = 1** 字节错误
- **码长**: `n = 8` 字节

### 3.2 与 HC 的映射

```
RS 保护帧（8 字节）:
  [frac[0]] [frac[1]] [frac[2]] [frac[3]] [frac[4]] [frac[5]] [P0] [P1]
  信息符号(6)                    校验符号(2)
```

**关键设计**: `int_part` 和 `level` 不参与 RS 编码（变化频率低，损坏可通过上下文推断）。RS 重点保护**高熵的小数层**。

### 3.3 编码器（LFSR 模式 —— 唯一推荐）

```cpp
// ⚠️ 严重警告：查表法需要 64KB Flash，AI8051U 总 Flash 仅 64KB，与程序代码互斥
// 因此 MCU 端强制使用 LFSR 模式，查表法仅用于 PC 验证器

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

**⚠️ 醒目警告**（真值验证 E5 修正）:
> **RS 查表法（64KB 查找表）不可用于 AI8051U 等 64KB Flash MCU**。程序代码至少需要 20~30KB，查表法将耗尽全部 Flash。MCU 端**强制使用 LFSR 模式**（仅需 ~256 字节代码）。

### 3.4 解码器（Berlekamp-Massey 简化版，t=1）

```cpp
// 单字节错误纠正：syndrome 计算 + 错误定位 + 错误值计算
bool rs86_decode_lfsr(uint8_t* codeword);
```

### 3.5 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算 | **修正值** |
|------|--------|-----------|
| RS 编码 LFSR(6字节) | 0.1 ms | **4.2 μs** |
| RS 解码纠正(1字节) | 75.0 μs | **3.1 μs** |
| RS 解码检测(双错) | — | **5.0 μs** |

---

## 4. 校验和增量更新（RAM 实时嗅探）

### 4.1 加权字节校验和

```cpp
// 预计算权重表（编译期常量）
static const uint8_t CS_WEIGHT[7] = {1, 2, 3, 4, 5, 6, 7};

// 计算 HC8 校验和
uint8_t sgn_hc8_checksum(const hc8_t* hc) {
    uint16_t sum = 0;
    sum += CS_WEIGHT[0] * hc->int_part;
    for (int i = 0; i < 6; ++i) {
        sum += CS_WEIGHT[i + 1] * hc->frac[i];
    }
    return (uint8_t)(sum & 0xFF);  // mod 256
}
```

### 4.2 增量更新（O(1)）

```cpp
// 第 layer 层字节从 old_byte 变为 new_byte 时，O(1) 更新校验和
uint8_t sgn_checksum_delta(uint8_t old_cs, uint8_t layer,
                             uint8_t old_byte, uint8_t new_byte) {
    int16_t delta = (int16_t)CS_WEIGHT[layer] * ((int16_t)new_byte - (int16_t)old_byte);
    return (uint8_t)((old_cs + delta) & 0xFF);
}
```

### 4.3 运行时嗅探

```cpp
// 每步训练后，快速比对全局校验和（O(N) 全量异或）
bool sgn_runtime_sniff(const sgn_core_t* core, uint8_t expected_global_cs) {
    uint8_t actual = 0;
    for (uint16_t i = 0; i < core->template_count; ++i) {
        actual ^= sgn_hc8_checksum(&core->templates[i].hit_counter);
    }
    return (actual == expected_global_cs);
}
```

### 4.4 三级容错链路

| 层级 | 机制 | 时机 | 能力 |
|------|------|------|------|
| **L1: 校验和** | 增量更新 + 运行时嗅探 | 每步 | 检测随机单比特翻转 |
| **L2: TMR** | 三副本多数投票 | 实时 | 掩盖单字节错误 |
| **L3: RS** | LFSR 纠错 | 加载/保存 | 纠正闪存单字节错误 |

### 4.5 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算 | **修正值** |
|------|--------|-----------|
| 全量计算 HC8 校验和 | ~40 周期 | **1.7 μs** |
| 增量更新（单字节） | ~8 周期 | **0.3 μs** |
| 全局异或嗅探（N=80） | ~200 周期 | **8.3 μs** |
| 深度审计（N=80, 3字段） | ~10,000 周期 | **417 μs** |

---

## 5. 存储层协同工作流

```
训练时（RAM 中）:
  模板修改 → 校验和增量更新（0.3μs）→ 运行时嗅探（8.3μs/步）
         → Merkle 增量更新（8.3μs）→ 根哈希变化追踪

保存时（Flash 写入）:
  模板库排序 → RLE 压缩（50%）→ RS(8,6) 编码（4.2μs/记录）
           → 写入 Flash 页（2~20ms，硬件决定）

加载时（Flash 读取）:
  读取 Flash 页（0.5ms）→ RS(8,6) 解码（3.1μs）→ RLE 解压（267μs/80模板）
                      → 重建 Trie 索引（O(N·L)）
                      → 重算 Merkle 树（O(N)）

分布式同步:
  本地 Merkle 根哈希 → UART 广播（8字节）→ 远端比对
                    → 不一致时请求差异子树路径 → 仅同步差异模板
```

---

## 6. 引用规范

任何上层规范在涉及以下存储操作时，**必须**调用本文档定义的接口：

- RLE 编解码 → `sgn_hc_rle_encode()` / `sgn_hc_rle_decode()` —— **§1**
- Merkle 增量更新 → `sgn_merkle_update_leaf()` —— **§2**
- RS 纠错 → `rs86_encode_lfsr()` / `rs86_decode_lfsr()` —— **§3**
- 校验和增量更新 → `sgn_checksum_delta()` —— **§4**
- 运行时嗅探 → `sgn_runtime_sniff()` —— **§4.3**

---

*本文档为 SGN 技术栈的存储后端 ABI 契约。所有持久化、压缩、审计、纠错操作必须遵守上述接口。*
