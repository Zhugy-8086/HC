# SGN 网络与分布式核心规范 (sgn_network_core)

> **版本**: v2.0 (权威重构版)  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈唯一权威分布式与硬件接口源  
> **合并来源**: 《UART串行通信编码》《I/O引脚并行编码》《Lamport时钟同步》《Quorum共识》《看门狗定时器》  
> **核心原则**: 本文档统一定义 UART 帧格式、GPIO 并行映射、Lamport 逻辑时钟、Quorum 中位数共识、看门狗超时。禁止重复定义帧结构或时钟语义。

---

## 1. UART 串行通信编码

### 1.1 基础帧结构（固定长度）

```
+--------+--------+--------+--------+--------+--------+--------+--------+
| 0xAA   | 0x55   | TYPE   | LEN    | HC[0]  | HC[1]  | ...    | CRC8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| 帧头0  | 帧头1  | 类型   | 长度   | HC字节0| HC字节1| HC字节N| 校验   |
```

- **帧头**: `0xAA 0x55`（双字节同步头）
- **TYPE**: 消息类型（`0x01`=神经元状态, `0x02`=模板追加, `0x03`=时间戳同步, `0x04`=模型同步, `0xFF`=心跳）
- **LEN**: 后续 HC 字节数（6/8/10/12，由精度档位决定）
- **HC[0..N]**: 原生 HC 字节数组，**零转换直接发送**
- **CRC8**: 多项式 `0x31`（x⁸ + x⁵ + x⁴ + 1），查表实现

### 1.2 扩展帧（合体模式）

```
+--------+--------+--------+--------+--------+--------+--------+--------+
| 0xAA   | 0x55   | TYPE   | LEN    | LEVEL  | INT    | HC[0]  | ...    |
+--------+--------+--------+--------+--------+--------+--------+--------+
| 帧头   | 帧头   | 类型   | 长度   | level  | int_p  | frac0  | fracN  |
```

- **LEVEL**: 4 字节无符号整数（小端），仅合体模式使用
- **INT**: 1 字节（HC8）或 2 字节（HC16）
- **总长度**: 固定 14 字节（HC8 合体）或 18 字节（HC16 合体）

### 1.3 COBS 透明传输

若 HC 字节中出现 `0x00`（帧头冲突），采用 COBS（Consistent Overhead Byte Stuffing）：

```
原始:  [0xAA, 0x55, TYPE, LEN, HC[0], 0x00, HC[2], ..., CRC8]
COBS:  [0xAA, 0x55, TYPE, LEN, HC[0], 0x02, HC[2], ..., CRC8, 0x00]
```

- COBS 开销：每 254 字节最多 1 字节填充
- HC8 帧（14 字节）COBS 后 ≤ 15 字节，带宽损失 < 8%
- **解码**：单遍扫描，无回溯，适合 ISR

### 1.4 帧大小对比

| 内容 | JSON | Protobuf | **HC 原生帧** | 压缩比 |
|------|------|----------|--------------|--------|
| 单神经元 base（HC8） | ~40 B | ~12 B | **8 B** | 5× / 1.5× |
| 模板签名（D=16） | ~60 B | ~18 B | **10 B** | 6× / 1.8× |
| 时间戳（HC16） | ~30 B | ~10 B | **10 B** | 3× / 1× |
| 心跳包 | ~20 B | ~8 B | **4 B** | 5× / 2× |

### 1.5 波特率与实时性

| 场景 | 帧大小 | 9600 baud | 115200 baud |
|------|--------|-----------|-------------|
| 单神经元增量更新 | 8 B | 8.3 ms | **0.69 ms** |
| 模板追加（HC8+label） | 10 B | 10.4 ms | **0.87 ms** |
| 时间戳广播 | 10 B | 10.4 ms | **0.87 ms** |
| 256 神经元批量同步 | 2048 B | 2.1 s | **178 ms** |
| 心跳 | 4 B | 4.2 ms | **0.35 ms** |

**关键约束**: 9600 baud 下 256 神经元全量同步需 2.1 秒，**不可接受**。必须使用 115200 baud（178 ms）或**增量同步**（仅传输变更模板）。

---

## 2. I/O 引脚并行编码

### 2.1 直接映射（零转换）

HC 的每一层字节可直接映射到 8 位 GPIO 并行总线：

```
P[i] = bit_i(b),  i = 0..7
```

- **LSB0**（推荐）: `P[0] = b & 0x01`, `P[7] = (b >> 7) & 0x01`
- **MSB0**: `P[0] = (b >> 7) & 0x01`, `P[7] = b & 0x01`

### 2.2 单字节选通模式（8+3 位）

```c
// 输出 HC8 的指定层到并行总线
// layer: 0=int_part, 1..6=frac[0..5]
void sgn_hc8_parallel_out(const hc8_t* hc, uint8_t layer) {
    uint8_t b = (layer == 0) ? hc->int_part : hc->frac[layer - 1];
    P1 = b;           // 8 位数据
    P3_0_2 = layer;   // 3 位层地址
}
```

**时序**: 单周期输出（8051 @48MHz/1T = **20.83 ns**）。

### 2.3 与 UART 的分工

| 接口 | 场景 | 延迟 |
|------|------|------|
| **并行 I/O** | 板内传感器/执行器 | **单周期** |
| **UART** | 板间节点同步 | 毫秒级 |

**一句话**: 并行是血管，串行是神经；血管供血给器官，神经传信号到大脑。

---

## 3. Lamport 逻辑时钟

### 3.1 HC 时间戳结构

```cpp
struct hc16_lamport_t {
    uint32_t level;      // 世代 / 模型版本
    uint16_t sec;        // 秒（模 65536 回绕）
    uint16_t ss[4];      // 子秒层（1/65536, 1/4.29G, ...）
};
```

**全局比较规则（因果序）**:

```
T_a < T_b  当且仅当:
    level_a < level_b
    或 (level_a == level_b 且 sec_a < sec_b)
    或 (level_a == level_b 且 sec_a == sec_b 且 ss_a 字典序 < ss_b)
```

**性质**: 偏序、传递、与超度量兼容（首次差异层即决定因果序）。

### 3.2 更新规则（max + delta）

```cpp
// 节点 j 接收节点 i 的消息后更新本地时钟
void sgn_lamport_update(hc16_lamport_t* local, const hc16_lamport_t* remote) {
    // 取 max（HC 字典序比较）
    if (sgn_hc16_compare(local, remote) < 0) {
        *local = *remote;
    }
    // + delta（1ms 传输延迟估计）
    hc16_lamport_t delta = {0, 0, {65, 0, 0, 0}};  // 1ms ≈ 65/65536
    *local = sgn_hc16_add(local, &delta);  // 调用 [sgn_hc_ops §3]
}
```

### 3.3 与硬件看门狗共用 ISR

```cpp
// 每 1ms tick 调用（与看门狗共用）
void sgn_tick_isr(void) {
    // 看门狗 tick 逻辑
    g_now.ss[3] += HC16_TICK_US;  // 65 = 65536/1000 取整
    // ... 进位链传播 ...
    if (g_now.sec == 0) {
        g_now.level += 1;  // sec 回绕，世代推进
    }
}
```

### 3.4 因果一致性定理

**定理**: 若事件 e1 happens-before e2（通过消息传递），则 `T(e1) < T(e2)`。

*证明*: 消息携带发送时戳，接收方取 max 后加 delta，故接收时戳严格大于发送时戳。证毕。

---

## 4. Quorum 中位数共识

### 4.1 逐层中位数算法（O(N·L)）

```cpp
// 无需全量排序，逐层确定中位数字节
hc8_t sgn_median_layerwise(const hc8_t* values, uint16_t N) {
    candidate_set_t cand = {0, 1, 2, ..., N-1};  // 初始候选集 = 全部
    hc8_t median;

    for (int layer = 0; layer <= 6; ++layer) {
        uint16_t freq[256] = {0};
        // 统计当前候选集在该层的字节频率
        for (uint16_t i = 0; i < cand.count; ++i) {
            uint8_t b = get_byte(values[cand.ids[i]], layer);
            freq[b]++;
        }
        // 找累积频率首次超过 |候选集|/2 的字节值
        uint16_t half = cand.count / 2 + 1;
        uint16_t cum = 0;
        uint8_t median_byte = 0;
        for (int b = 0; b < 256; ++b) {
            cum += freq[b];
            if (cum >= half) { median_byte = b; break; }
        }
        // 收缩候选集
        uint16_t new_count = 0;
        for (uint16_t i = 0; i < cand.count; ++i) {
            if (get_byte(values[cand.ids[i]], layer) == median_byte) {
                cand.ids[new_count++] = cand.ids[i];
            }
        }
        cand.count = new_count;
        if (cand.count <= 1) break;
    }
    return median;
}
```

**复杂度**: `O(N·L)` = 常数级（L=6 固定），80 μs @48MHz/1T。

### 4.2 容错能力

| 节点数 N | 最大故障 f | 容错率 | 中位数位置 |
|---------|-----------|--------|-----------|
| 3 | 1 | 33% | 第 2 位 |
| 5 | 2 | 40% | 第 3 位 |
| 7 | 3 | 43% | 第 4 位 |
| 9 | 4 | 44% | 第 5 位 |

**定理**: N ≥ 2f + 1 时，中位数落在正常节点的值域内，不会被故障节点扭曲。

### 4.3 与 Raft/PBFT 的对比

| 维度 | Raft | PBFT | **HC Quorum** |
|------|------|------|---------------|
| 消息轮次 | 2~3 轮 | 3 轮 | **1 轮广播** |
| 消息复杂度 | O(N) | O(N²) | **O(N)** |
| Leader | 需要 | 需要 | **无** |
| 确定性 | 选举随机超时 | 视图切换随机 | **完全确定** |
| 拜占庭容错 | 否 | 是（f < N/3） | **是（f < N/2）** |
| MCU 内存 | 需日志队列 | 需多轮状态缓存 | **仅需 N 个 HC8 数组** |

---

## 5. 看门狗定时器

### 5.1 HC16 时间戳结构

```cpp
struct hc16_time_t {
    uint16_t sec;       // 秒，0~65535，模 65536 回绕
    uint16_t ss[4];     // 子秒层，每层 1/65536 秒 ≈ 15.26 μs
};
```

**物理值**: `T = sec + ss[0]/65536 + ss[1]/65536² + ...`

### 5.2 超时判定（逐层短路）

```cpp
// 返回 true = 已超时
bool sgn_is_timeout(const hc16_time_t* deadline) {
    uint16_t diff = deadline->sec - g_now.sec;
    if (diff > 32768) return false;   // deadline 在遥远未来
    if (diff != 0) return (diff < 32768);  // sec 已分出大小
    // sec 相等，比较 subsec（概率极低，1/65536）
    for (int i = 0; i < 4; ++i) {
        if (g_now.ss[i] != deadline->ss[i]) {
            return g_now.ss[i] > deadline->ss[i];
        }
    }
    return true;  // 完全相等，视为超时
}
```

**平均比较次数**: 1.01 次（subsec 仅在 sec 相等时比较）。

### 5.3 多任务独立超时

```cpp
struct sgn_wdt_entry_t {
    hc16_time_t deadline;
    uint8_t     task_id;
    uint8_t     active;
    void        (*on_timeout)(uint8_t);
};

#define SGN_WDT_SLOTS 8
sgn_wdt_entry_t g_wdt_slots[SGN_WDT_SLOTS];
```

**超时回调示例**:
- slot 0: 训练步超时 → 强制进入下一竞争循环
- slot 1: 模板同步超时 → 标记分布式节点失联
- slot 2: 自动保存超时 → 触发 autosave_check()
- slot 3: 神经元死锁检测 → 若 10 秒无 winner，重置网络

### 5.4 性能基准（48MHz/1T 修正值）

| 操作 | 旧估算(24MHz/12T) | **修正值(48MHz/1T)** |
|------|-------------------|---------------------|
| 看门狗超时判定 | 6.0 μs | **0.25 μs** |
| tick ISR | 25.0 μs | **1.04 μs** |

---

## 6. 分布式工作流

```
节点 A（训练）:
  每 100 步 → 生成本地 Merkle 根哈希（8B）
         → UART 广播 Top-5 模板 + Lamport 时间戳

节点 B（推理）:
  接收模板 → 用 RS-LFSR 解码校验
         → 更新本地 Lamport 时钟（max + delta）
         → 追加到本地 Trie 索引
         → 参与 WTA 竞争

节点 C（监控）:
  收集三节点 Merkle 根哈希
         → 若不一致，触发 Quorum 中位数共识
         → 定位差异子树，仅同步差异模板
```

---

## 7. 引用规范

任何上层规范在涉及以下网络/分布式操作时，**必须**调用本文档定义的接口：

- UART 帧打包 → `sgn_uart_pack_frame()` —— **§1**
- GPIO 并行输出 → `sgn_hc8_parallel_out()` —— **§2**
- Lamport 时钟更新 → `sgn_lamport_update()` —— **§3**
- Quorum 共识 → `sgn_median_layerwise()` —— **§4**
- 看门狗启动 → `sgn_wdt_start()` —— **§5**

---

*本文档为 SGN 技术栈的网络与分布式层 ABI 契约。所有节点通信、时钟同步、共识操作必须遵守上述接口。*
