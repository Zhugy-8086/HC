# SGN 技术栈规范重构包

> **日期**: 2026-05-27  
> **版本**: v2.0 重构版  
> **验算基准**: AI8051U 48MHz/1T, SRAM 34KB, Flash 64KB

---

## 目录结构

```
sgn_spec/
├── L0_core/              ← 9 份核心地基文件（ABI 契约）
│   ├── sgn_math_core.md
│   ├── sgn_hc_ops.md
│   ├── sgn_trie_core.md
│   ├── sgn_config_guide.md
│   ├── sgn_engine_core.md
│   ├── sgn_storage_core.md
│   ├── sgn_network_core.md
│   ├── sgn_reliability_core.md
│   └── sgn_index_matrix.md
│
├── L1_extensions/        ← 14 份扩展文档（数学预判 + 高级功能）
│   ├── HC_2adic平方根_Hensel提升规范_v1.2.md
│   ├── HC_计数布隆过滤器规范_v1.2.md
│   ├── HC_投影除法与欧氏运算沙盒规范_v1.2.md
│   ├── HC_补码负数扩展规范_v1.2.md
│   ├── HC_整数次幂快速幂规范_v1.1.md
│   ├── HC_Hensel提升求根规范_v1.1.md
│   ├── HC_超度量布朗运动规范_v1.1.md
│   ├── HC_超度量球面反射规范_v1.1.md
│   ├── HC_超度量重心_Frechet均值规范_v1.1.md
│   ├── HC_等距嵌入欧氏空间规范_v1.2.md
│   ├── HC_霍夫曼前缀码规范_v1.1.md
│   ├── HC_球面坐标与距离场规范_v1.1.md
│   ├── HC_形态学膨胀与腐蚀规范_v1.1.md
│   └── HC_DFA状态编码规范_v1.1.md
│
├── L2_bridge/            ← 2 份桥接文档（待完成）
│   ├── pytorch_bridge.md     (v0.1 大纲)
│   └── onnx_mapping.md       (v0.1 大纲)
│
├── deprecated/           ← 26 份已合并原始文档（DEPRECATED）
│   └── ...
│
└── reports/              ← 5 份验算报告
    ├── 01_SGN_问题清单与解决路径_v1.0.md
    ├── 02_SGN_内存预算验算表_34KB_SRAM.md
    ├── 03_SGN_性能数据验算表_48MHz_1T.md
    ├── 04_SGN_文档重复与重构方案.md
    └── 05_SGN_真值验证报告.md
```

---

## 关键修正

| 编号 | 错误 | 修正位置 |
|------|------|---------|
| E1 | Hensel 步数 6→45 | L1/2-adic平方根 v1.2 |
| E2 | CBF 假阳性率 1.8%→14.7% | L1/CBF v1.2 |
| E3 | float128 对 HC32 "无损"声明 | L1/投影除法 v1.2 |
| E4 | 性能估算基于 24MHz/12T | 全部核心文件 |
| E5 | RS 查表法未说明 Flash 不可行 | L0/sgn_storage_core |
| E6 | CBF "向 level 进位"与"仅 HC 模式"矛盾 | L1/CBF v1.2 |

---

## 使用规则

1. **L0_core** 是 ABI 契约，所有上层实现必须调用其中定义的接口
2. **L1_extensions** 禁止重复实现 L0 中的基础运算，必须引用
3. **L2_bridge** 待完成，当前仅有大纲
4. **deprecated** 文件仅供历史追溯，不再维护

---

*重构完成日期: 2026-05-27*
