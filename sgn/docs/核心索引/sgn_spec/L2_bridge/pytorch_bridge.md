# SGN PyTorch 训练钩子规范

> **版本**: v0.1（大纲版）  
> **日期**: 2026-05-27  
> **性质**: SGN 技术栈 L2 桥接层规范  
> **状态**: 待编写  
> **目标**: 允许用户在 PyTorch 训练循环中插入 SGN 模板库作为"竞争后处理层"，实现 PyTorch QAT 模型到 SGN 模板库的端到端迁移。

---

## 待完成内容

1. **PyTorch 训练钩子接口**
   - `SGNCompetitiveLayer` 类：nn.Module 子类
   - 前向传播：输入浮点特征 → 量化 → HC8 签名 → WTA 竞争 → 输出赢家 ID
   - 反向传播：无（SGN 无梯度），通过命中计数器更新模板

2. **模型导出流程**
   - PyTorch QAT 模型 → ONNX → SGN 模板库
   - 权重二值化 → HC8 编码 → 模板追加

3. **示例代码**
   - ResNet-8 二值化后导入 SGN 的完整示例

4. **精度映射**
   - PyTorch 量化参数（scale, zero_point）→ HC8 物理值

---

> **依赖核心文件**: `sgn_engine_core.md` §1 (WTA), `sgn_hc_ops.md` §1 (类型定义)
