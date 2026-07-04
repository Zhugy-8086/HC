# SGN ONNX 映射规范

> **版本**: v0.1（大纲版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈 L2 桥接层规范  
> **状态**: 待编写  
> **目标**: 定义 ONNX 算子到 SGN 算子的映射表，使 ONNX Runtime / TVM / VTA 后端可识别并优化 SGN 整数网络。

---

## 待完成内容

1. **ONNX → HC 算子映射表**
   - `QuantizeLinear` → `HC8.from_float()`
   - `Conv` → `Trie索引 + WTA竞争`（概念映射，SGN 无卷积）
   - `MaxPool` → `WTA竞争算子`
   - `ReLU` → `软阈值稀疏诱导`
   - `Gemm` → `HC8 带进位加法链`

2. **MLIR Dialect 定义**
   - `sgn` MLIR dialect：包含 `sgn.trie`, `sgn.wta`, `sgn.soft_threshold` 等操作
   - TVM/VTA 后端识别并优化

3. **端到端迁移示例**
   - PyTorch QAT 模型 → ONNX → SGN 模板库 的完整 pipeline

4. **精度验证**
   - ONNX 浮点推理 vs SGN 整数推理的逐层对比

---

> **依赖核心文件**: `sgn_engine_core.md` §1-2 (WTA/软阈值), `sgn_trie_core.md` §1 (Trie节点), `sgn_hc_ops.md` §1 (类型定义)
