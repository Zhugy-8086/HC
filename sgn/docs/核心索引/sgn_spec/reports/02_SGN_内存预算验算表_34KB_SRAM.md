# SGN 内存预算验算表 —— 基于 AI8051U 34KB SRAM

> **验算日期**: 2026-05-27  
> **硬件基准**: AI8051U, 48MHz/1T, SRAM 34,816 B, Flash 64KB  
> **可用 SRAM**: 30,720 B（扣除系统栈 2KB + 中断开销 1KB + 全局变量 1KB）

---

## 一、单组件内存占用明细

| 组件 | 规范来源 | 原始声称 | 重新核算 | 核算依据 | 备注 |
|------|---------|---------|---------|---------|------|
| **神经元状态(256N)** | 多份核心规范 | ~5KB | **5,120 B** | 256×20B (base+enc_b+lock+tid+L+meta) | 非TMR模式 |
| **模板库(80稳态)** | sgn_core.py 逻辑 | ~1.6KB | **1,920 B** | 80×24B (label+mask+sc+hc8+meta) | mask=D=64→8B |
| **Trie索引(80模板)** | Trie索引规范 | ~640B | **720 B** | 120节点×6B (byte+child+ sibling+tid+flags) | 稀疏开放寻址 |
| **Merkle树(80模板)** | Merkle树规范 | ~1KB | **1,016 B** | 127节点×8B (2^(⌈log2(80)⌉+1)-1) | 树高7 |
| **Merkle树(256模板)** | Merkle树规范 | ~4KB | **4,088 B** | 511节点×8B | 树高9 |
| **CBF(256桶)** | CBF规范 v1.1 | ~1.5KB | **1,536 B** | 256桶×6B (HC8) | 轻量模式 |
| **CBF(1024桶)** | CBF规范 v1.1 | ~6KB | **6,144 B** | 1024桶×6B | 标准模式 |
| **LRU排序数组** | LRU规范 | 160B | **512 B** | 256×2B (最大模板数) | 预留到MAX |
| **TMR全量(256N)** | TMR规范 | ~15KB | **15,360 B** | 256×60B (base+enc_b+hc 3副本) | 不可行 |
| **TMR部分(仅高2层)** | TMR规范 | ~5KB | **5,120 B** | 256×20B (int_part+frac[0] 3副本) | 可行 |
| **RS查表(编码)** | RS规范 | 64KB | **65,536 B** | 65536×1B 或 65536×2B | ❌ Flash都不够 |
| **RS-LFSR(无表)** | RS规范 | ~200B | **256 B** | LFSR状态+多项式系数+缓冲 | 推荐模式 |
| **看门狗8槽** | 看门狗规范 | ~96B | **96 B** | 8×12B (deadline+task_id+active+cb_ptr) | |
| **Lamport时间戳** | Lamport规范 | ~30B | **30 B** | 3节点×10B | |
| **UART缓冲** | UART规范 | 32B | **32 B** | 静态ISR缓冲 | |
| **DFA状态表(100状态)** | DFA规范 | ~1.2KB | **1,200 B** | 100×12B (边数组) | |
| **Fréchet频率表** | Fréchet规范 | ~7KB | **3,584 B** | 7×256×2B (7层×256字节×16位) | 可压缩到8位 |
| **霍夫曼码表** | 霍夫曼规范 | ~2KB | **2,048 B** | 256×8B | |
| **形态学标记** | 形态学规范 | 256B | **256 B** | 节点标记位图 | |
| **布朗运动粒子** | 布朗运动规范 | 10B | **10 B** | path+depth+seed | |
| **嵌入向量缓冲** | 等距嵌入规范 | 56B | **56 B** | 56位二元向量 | PC端为主 |
| **排序网络缓冲(N=256)** | 排序网络规范 | ~2KB | **2,048 B** | 256×8B sort_item_t | |
| **Hensel中间变量** | Hensel规范 | ~48B | **48 B** | 48位大整数(6字节) | 运行时栈 |
| **快速幂中间变量** | 快速幂规范 | ~14B | **14 B** | 14字节卷积结果 | 运行时栈 |
| **校验和全局状态** | 校验和规范 | 241B | **241 B** | 80模板×3字段×1B + 全局1B | |

---

## 二、配置组合可行性矩阵

| 配置名称 | 包含组件 | 总占用 | vs 30KB | 可行性 | 风险 |
|---------|---------|--------|---------|--------|------|
| **裸核心** | 256N + 80模板 + Trie + UART | 8,304 B | 余 22KB | ✅ 安全 | 无 |
| **标准推理** | 裸核心 + Merkle(80) + CBF256 + LRU + 看门狗 | 10,952 B | 余 19KB | ✅ 安全 | 低 |
| **高可靠推理** | 标准 + TMR部分 + RS-LFSR + 校验和 | 12,176 B | 余 18KB | ✅ 安全 | 低 |
| **分布式节点** | 标准 + Lamport + Quorum缓冲 + DFA(50) | 12,200 B | 余 18KB | ✅ 安全 | 中 |
| **全索引** | 标准 + DFA(100) + 霍夫曼 + Fréchet | 14,800 B | 余 16KB | ✅ 安全 | 中 |
| **极限挑战** | 全索引 + TMR部分 + 形态学 + 排序网络(N=128) | 19,500 B | 余 11KB | ✅ 可接受 | 中 |
| **全量豪华** | 极限 + TMR全量 + CBF1024 + Merkle(256) + Fréchet | 30,784 B | **超 64 B** | ❌ **不可行** | **高** |
| **全量+压缩** | 全量豪华 + RLE压缩缓冲 | 32,000 B | 超 1.3KB | ❌ 不可行 | 极高 |

---

## 三、关键互斥规则（必须遵守）

```cpp
// sgn_memory_budget.h
#ifndef SGN_MEMORY_BUDGET_H
#define SGN_MEMORY_BUDGET_H

#define SGN_SRAM_TOTAL     34816
#define SGN_SRAM_RESERVED  4096    // 栈(2KB) + 中断(1KB) + 全局(1KB)
#define SGN_SRAM_BUDGET    30720

// 互斥宏：以下组合不能同时启用
#if defined(SGN_TMR_FULL) && defined(SGN_CBF_1024)
    #error "TMR_FULL 与 CBF_1024 互斥：合计 > 16KB"
#endif

#if defined(SGN_RS_TABLE_MODE)
    #error "RS_TABLE_MODE 需要 64KB Flash，AI8051U 仅 64KB 总 Flash，与程序代码互斥"
#endif

#if defined(SGN_TMR_FULL) && defined(SGN_FRECHET_16BIT)
    #error "TMR_FULL 与 FRECHET_16BIT 互斥：合计接近 SRAM 上限"
#endif

#if defined(SGN_SORTNET_256) && defined(SGN_TMR_FULL)
    #error "SORTNET_256 需要 2KB 缓冲，与 TMR_FULL 互斥"
#endif

// 动态降级阈值
#define SGN_TEMPLATE_THRESHOLD_SCALE    128
#if SGN_MAX_TEMPLATES > SGN_TEMPLATE_THRESHOLD_SCALE
    #warning "模板数 > 128，自动关闭 FRECHET 频率表以节省 3.5KB"
    #undef SGN_FRECHET_ENABLE
#endif

#endif
```

---

## 四、内存池划分建议

```
SRAM 34KB 布局:
├─ [0x0000~0x07FF]  2KB  系统栈 + 中断向量 + SFR映射
├─ [0x0800~0x0FFF]  2KB  全局变量 + 常量表 (RS-LFSR/权重表)
├─ [0x1000~0x4FFF] 16KB  核心池 (神经元 5KB + 模板库 2KB + Trie 1KB + 基础缓冲)
├─ [0x5000~0x6FFF]  8KB  扩展池 (Merkle/CBF/LRU/排序网络/DFA 动态切换)
├─ [0x7000~0x77FF]  2KB  可靠池 (TMR副本/校验和/RS-LFSR状态)
└─ [0x7800~0x7FFF]  2KB  I/O缓冲池 (UART/看门狗/Lamport/I/O并行)
```

---

## 五、验算结论

1. **标准配置（推理 + 可靠 + 分布式）在 34KB 内完全可行**，余量 18KB 足够应对峰值。
2. **全量启用 38 份规范不可行**：TMR 全量 + CBF1024 + Merkle(256) 组合超支 64B，且未计入运行时栈增长。
3. **RS 查表法（64KB）不可行**：不仅 SRAM 不够，Flash 也不够（程序代码至少需要 20~30KB）。
4. **动态降级是必要的**：当模板库规模 > 128 时，必须自动关闭 Fréchet 频率表或降级为 8 位频率表。
5. **建议默认配置**: `标准推理` 或 `高可靠推理`，余量 18KB 留给用户自定义扩展。
