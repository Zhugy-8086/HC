#!/usr/bin/env python3
"""
pysgn Usage Examples - SGN ABI Python Binding

This file demonstrates how to use the SGN ABI from Python with a
natural, intuitive programming experience.

The key insight: Projection Sandbox lets you write NORMAL float code
while the library handles HC (Hierarchical Counter) representation.
"""

import pysgn

print("=" * 60)
print("SGN ABI Python Binding - Usage Examples")
print("=" * 60)

# ============================================================================
# SECTION 1: Basic HC8 Creation and Operations
# ============================================================================

print("\n--- Section 1: Basic HC8 ---")

# Create HC8 values from float
a = pysgn.HC8(3.14159)
b = pysgn.HC8(2.71828)

print(f"a = {a}  (from 3.14159)")
print(f"b = {b}  (from 2.71828)")

# Convert back to float
print(f"a.to_float() = {a.to_float():.5f}")
print(f"b.to_float() = {b.to_float():.5f}")

# Native HC operations (saturated arithmetic)
c = a + b
print(f"a + b = {c}  (saturated addition)")

d = a - b
print(f"a - b = {d}  (subtraction, saturate to 0)")

# Comparison
print(f"a < b = {a < b}")
print(f"a == b = {a == b}")

# Layer access
print(f"a[0] (int_part layer) = {a[0]}")
print(f"a[1] (frac[0] layer) = {a[1]}")

# Zero and max
zero = pysgn.HC8.zero()
mx = pysgn.HC8.max_val()
print(f"zero = {zero}")
print(f"max = {mx}")

# ============================================================================
# SECTION 2: Projection Sandbox (THE KEY FEATURE)
# ============================================================================

print("\n--- Section 2: Projection Sandbox (Float Programming) ---")

# Create sandbox with STD precision, 8 integer bits
sb = pysgn.Sandbox(pysgn.Precision.STD, 8)

# Project HC to float
phi_a = sb.project(a)
phi_b = sb.project(b)
print(f"project(a) = {phi_a:.6f}")
print(f"project(b) = {phi_b:.6f}")

# Do normal float math, then inverse back!
result = sb.inverse(phi_a / (phi_b + 0.001))
print(f"inverse(project(a) / (project(b) + 0.001)) = {result}")

# ============================================================================
# SECTION 3: Arbitrary Division (Why Sandbox is Normative)
# ============================================================================

print("\n--- Section 3: Arbitrary Division ---")

x = pysgn.HC8(10.0)
y = pysgn.HC8(3.0)

# Before sandbox: division was limited to odd divisors (Hensel)
# With sandbox: ANY divisor works!
q = sb.divide(x, y)
print(f"{x} / {y} = {q}")
print(f"Verify: {q}.to_float() = {q.to_float():.5f} (expected ~3.333)")

# Division by zero returns max (saturated)
z = pysgn.HC8(0.0)
q0 = sb.divide(x, z)
print(f"{x} / {z} (zero) = {q0} (saturated to max)")

# ============================================================================
# SECTION 4: Gradient Updates (Machine Learning)
# ============================================================================

print("\n--- Section 4: Gradient Descent ---")

h = pysgn.HC8(100.0)
grad = 0.5    # gradient
eta = 0.01    # learning rate

# H_new = H - eta * grad
h_new = sb.gradient(h, grad, eta)
print(f"gradient({h}, grad={grad}, eta={eta}) = {h_new}")
print(f"Before: {h.to_float():.2f}, After: {h_new.to_float():.2f}")

# Multiple steps
for step in range(5):
    h = sb.gradient(h, 0.3, 0.02)
    print(f"  Step {step+1}: {h.to_float():.4f}")

# ============================================================================
# SECTION 5: Scaling and Linear Transformations
# ============================================================================

print("\n--- Section 5: Scaling ---")

val = pysgn.HC8(50.0)
scaled = sb.scale(val, 2.5)
print(f"scale({val}, 2.5) = {scaled}  (expected ~125.0)")

halved = sb.scale(val, 0.5)
print(f"scale({val}, 0.5) = {halved}  (expected ~25.0)")

# ============================================================================
# SECTION 6: Batch Operations (Vectorized)
# ============================================================================

print("\n--- Section 6: Batch Operations ---")

# Create array of HC8 values
arr = [pysgn.HC8(float(i)) for i in range(10)]
print(f"Input array: {[h.to_float() for h in arr]}")

# Project all to float
floats = sb.project_batch(arr)
print(f"Projected:   {[f'{x:.3f}' for x in floats]}")

# Process in float space (keep in [0, 1) Phi range!)
processed = [x * 0.5 + 0.1 for x in floats]
print(f"Processed:   {[f'{x:.3f}' for x in processed]}")

# Inverse back to HC
results = sb.inverse_batch(processed)
print(f"Back to HC:  {[h.to_float() for h in results]}")

# ============================================================================
# SECTION 7: Generic Float Programming with map()
# ============================================================================

print("\n--- Section 7: Generic Float Programming (map) ---")

val = pysgn.HC8(4.0)

# Apply any Python function!
squared = sb.map(val, lambda x: x ** 2)
print(f"map(x -> x^2, {val}) = {squared}")

sqrt_approx = sb.map(val, lambda x: x ** 0.5)
print(f"map(x -> sqrt(x), {val}) = {sqrt_approx}")

# Binary operations
avg = sb.map2(a, b, lambda x, y: (x + y) / 2.0)
print(f"map2((x,y) -> (x+y)/2, {a}, {b}) = {avg}")

# ============================================================================
# SECTION 8: Convenience Functions (One-shot)
# ============================================================================

print("\n--- Section 8: Convenience Functions ---")

# Quick divide without creating sandbox
q = pysgn.quick_divide(pysgn.HC8(7.0), pysgn.HC8(2.0))
print(f"quick_divide(7.0, 2.0) = {q}")

# Quick gradient
g = pysgn.quick_gradient(pysgn.HC8(50.0), 0.5, 0.01)
print(f"quick_gradient(50.0, 0.5, 0.01) = {g}")

# Array operations
nums = [pysgn.HC8(float(i * 10)) for i in range(1, 6)]  # 10,20,30,40,50
dens = [pysgn.HC8(float(i * 3)) for i in range(1, 6)]   # 3,6,9,12,15
quotients = pysgn.array_divide(nums, dens)
print(f"array_divide({[n.to_float() for n in nums]}, {[d.to_float() for d in dens]})")
print(f"  = {[q.to_float() for q in quotients]}")

scaled_arr = pysgn.array_scale(nums, 0.5)
print(f"array_scale(nums, 0.5) = {[s.to_float() for s in scaled_arr]}")

# ============================================================================
# SECTION 9: Soft Threshold (Sparse Induction)
# ============================================================================

print("\n--- Section 9: Soft Threshold ---")

X = pysgn.HC8(10.0)
Lambda = pysgn.HC8(3.0)

st = pysgn.soft_threshold(X, Lambda)
print(f"soft_threshold({X}, {Lambda}) = {st}  (expected ~7.0)")

# Below threshold -> zero
small = pysgn.HC8(2.0)
st_zero = pysgn.soft_threshold(small, Lambda)
print(f"soft_threshold({small}, {Lambda}) = {st_zero}  (below threshold -> zero)")

# Array version
arr = [pysgn.HC8(float(i)) for i in [1, 5, 10, 15, 20]]
st_arr = pysgn.soft_threshold_array(arr, Lambda)
print(f"soft_threshold_array({[h.to_float() for h in arr]}, {Lambda})")
print(f"  = {[h.to_float() for h in st_arr]}")

# ============================================================================
# SECTION 10: Reliability (TMR)
# ============================================================================

print("\n--- Section 10: TMR Voting ---")

# Three copies of a value
copy_a = pysgn.HC8(42.0)
copy_b = pysgn.HC8(42.0)
copy_c = pysgn.HC8(43.0)  # slightly different (error!)

result, err_mask = pysgn.tmr_vote(copy_a, copy_b, copy_c)
print(f"TMR vote on [{copy_a}, {copy_b}, {copy_c}]")
print(f"  Result: {result}")
print(f"  Error mask: 0x{err_mask:02X} (bit set = error detected)")

# ============================================================================
# SECTION 11: Checksum
# ============================================================================

print("\n--- Section 11: Checksum ---")

cs = pysgn.hc8_checksum(pysgn.HC8(123.456))
print(f"checksum(HC8(123.456)) = 0x{cs:02X}")

# ============================================================================
# SECTION 12: Version Info
# ============================================================================

print("\n--- Section 12: Version ---")

ver = pysgn.abi_version()
major = (ver >> 16) & 0xFF
minor = (ver >> 8) & 0xFF
patch = ver & 0xFF
print(f"ABI Version: {major}.{minor}.{patch}")
print(f"Precision: ARCHIVE={pysgn.Precision.ARCHIVE}, "
      f"STD={pysgn.Precision.STD}, PREC={pysgn.Precision.PREC}")

# ============================================================================
# SUMMARY
# ============================================================================

print("\n" + "=" * 60)
print("Summary: Key Programming Patterns")
print("=" * 60)
print("""
1. Create HC values:    h = pysgn.HC8(3.14)
2. Native arithmetic:   c = a + b       (saturated)
3. Float programming:   sb = pysgn.Sandbox(pysgn.Precision.STD, 8)
4. Divide anything:     q = sb.divide(a, b)
5. Gradient descent:    h = sb.gradient(h, grad, eta)
6. Scale:               s = sb.scale(h, factor)
7. Custom float op:     r = sb.map(h, lambda x: x**2)
8. Batch ops:           arr_out = sb.inverse_batch([x*2 for x in sb.project_batch(arr)])
9. One-shot:            q = pysgn.quick_divide(a, b)
""")


# ============================================================================
# SECTION 13: 插件架构（Plugin Architecture）
# ============================================================================

print("
" + "=" * 60)
print("Section 13: 插件架构 - 官方与第三方插件")
print("=" * 60)

# ----------------------------------------------------------------------------
# 13.1 加载官方插件（Normative）- 对数域投影
# ----------------------------------------------------------------------------

print("
--- 13.1: 加载官方插件（对数域投影） ---")

# 官方插件拥有修改核心数值语义的权力，如注册新的沙盒投影方案
# 假设已编译好插件：sgn_plugin_logdomain.so
# h = pysgn.load_plugin("./sgn_plugin_logdomain.so")

# 演示：查询已加载插件（当前可能为空，仅展示API）
count = pysgn.plugin_count()
print(f"当前已加载插件数量: {count}")

# 遍历已加载插件名称
for i in range(count):
    name = pysgn.plugin_get_name(i)
    print(f"  插件[{i}]: {name}")

# ----------------------------------------------------------------------------
# 13.2 插件能力查询
# ----------------------------------------------------------------------------

print("
--- 13.2: 插件能力查询 ---")

# 能力掩码常量
print(f"PLUGIN_CAP_SANDBOX_CONVERTER = 0x{pysgn.PLUGIN_CAP_SANDBOX_CONVERTER:016X}")
print(f"PLUGIN_CAP_HC_OPERATOR       = 0x{pysgn.PLUGIN_CAP_HC_OPERATOR:016X}")
print(f"PLUGIN_CAP_PRECISION_GRADE   = 0x{pysgn.PLUGIN_CAP_PRECISION_GRADE:016X}")
print(f"PLUGIN_CAP_ENGINE_ALGORITHM  = 0x{pysgn.PLUGIN_CAP_ENGINE_ALGORITHM:016X}")
print(f"PLUGIN_CAP_STORAGE_DRIVER    = 0x{pysgn.PLUGIN_CAP_STORAGE_DRIVER:016X}")
print(f"PLUGIN_CAP_NETWORK_DRIVER    = 0x{pysgn.PLUGIN_CAP_NETWORK_DRIVER:016X}")
print(f"PLUGIN_CAP_MONITOR_HOOK      = 0x{pysgn.PLUGIN_CAP_MONITOR_HOOK:016X}")
print(f"PLUGIN_CAP_ENCODER           = 0x{pysgn.PLUGIN_CAP_ENCODER:016X}")

# 若已加载插件，可查询其能力
# caps = pysgn.plugin_get_capabilities("sgn.log_domain")
# if caps & pysgn.PLUGIN_CAP_SANDBOX_CONVERTER:
#     print("该插件拥有沙盒转换器能力（官方内核级）")

# ----------------------------------------------------------------------------
# 13.3 沙盒投影方案切换
# ----------------------------------------------------------------------------

print("
--- 13.3: 沙盒投影方案切换 ---")

# 默认方案为 "default"（float64 物理值投影）
sb = pysgn.Sandbox(pysgn.Precision.STD, 8)

# 若已加载对数域官方插件，可切换投影方案
# sb.set_scheme("log_domain")
# 此后 sb.project() / sb.inverse() 将使用对数域转换

print("默认方案: default（float64 物理值投影）")
print("可切换方案: log_domain（对数域，避免乘法下溢）")
print("            float128（高精度，需平台支持）")

# ----------------------------------------------------------------------------
# 13.4 规范晋升路径（从插件到核心ABI）
# ----------------------------------------------------------------------------

print("
--- 13.4: 规范晋升路径 ---")

# 投影沙盒最初只是一个扩展插件，后被提升为规范级（Normative）
# 任何官方插件在成熟后，都可以走这条晋升路径

# 步骤1：插件处于 ACTIVE 状态（已加载并初始化成功）
# 步骤2：维护者调用 propose_normative() 标记为候选
# pysgn.plugin_propose_normative("sgn.log_domain")

# 步骤3：查询候选状态
# is_candidate = pysgn.plugin_is_normative_candidate("sgn.log_domain")
# print(f"是否为规范候选: {is_candidate}")

# 步骤4：在后续 ABI Minor 版本中，候选插件被冻结进核心
# 冻结后状态变为 FROZEN，不可卸载，成为 ABI 永久组成部分

print("晋升流程: ACTIVE → CANDIDATE（propose_normative）→ FROZEN（Minor版本发布）")
print("参考案例: 投影沙盒（Sandbox）最初是插件，v1.0 起成为规范级组件")

# ----------------------------------------------------------------------------
# 13.5 加载第三方插件（Extension）- 存储驱动示例
# ----------------------------------------------------------------------------

print("
--- 13.5: 加载第三方插件（存储驱动） ---")

# 第三方插件被严格限制在外围扩展域，无法触及 HC 核心语义
# 假设社区贡献了 SQLite 存储驱动插件

# h_sqlite = pysgn.load_plugin("./community_sqlite.so")

# 第三方插件自动通过能力掩码校验：
# - 允许 STORAGE_DRIVER、NETWORK_DRIVER、MONITOR_HOOK、ENCODER
# - 禁止 SANDBOX_CONVERTER、HC_OPERATOR、PRECISION_GRADE、ENGINE_ALGORITHM

print("第三方插件能力限制：")
print("  ✅ 允许: 存储后端、网络协议、监控钩子、编解码器")
print("  ❌ 禁止: 沙盒转换器、HC运算符、精度等级、引擎策略")

# ----------------------------------------------------------------------------
# 13.6 插件生命周期管理
# ----------------------------------------------------------------------------

print("
--- 13.6: 插件生命周期管理 ---")

print("生命周期状态机:")
print("  LOADED  →  init()  →  register()  →  ACTIVE")
print("    ↑                                    │")
print("    └────────── shutdown() ←─────────────┘")
print("                                         │")
print("                              propose_normative()")
print("                                         │")
print("                                         ▼")
print("                                    CANDIDATE")
print("                                         │")
print("                              ABI Minor 发布")
print("                                         │")
print("                                         ▼")
print("                                    FROZEN（不可卸载）")

# 卸载插件（FROZEN 状态的插件不可卸载）
# pysgn.unload_plugin(h)

# ----------------------------------------------------------------------------
# 13.7 插件类型枚举
# ----------------------------------------------------------------------------

print("
--- 13.7: 插件类型 ---")

print(f"PluginType.NORMATIVE = {pysgn.PluginType.NORMATIVE}  (内核级，可修改核心)")
print(f"PluginType.EXTENSION = {pysgn.PluginType.EXTENSION}  (外围级，沙箱隔离)")
print(f"PluginType.HYBRID    = {pysgn.PluginType.HYBRID}     (混合级，官方全栈扩展)")

# ----------------------------------------------------------------------------
# 13.8 监控事件（用于遥测与调试）
# ----------------------------------------------------------------------------

print("
--- 13.8: 监控事件 ---")

print(f"MonitorEvent.HC_OP       = {pysgn.MonitorEvent.HC_OP}       (HC运算)")
print(f"MonitorEvent.WTA_COMPETE = {pysgn.MonitorEvent.WTA_COMPETE} (WTA竞争)")
print(f"MonitorEvent.STORAGE_IO  = {pysgn.MonitorEvent.STORAGE_IO}  (存储读写)")
print(f"MonitorEvent.PLUGIN_LOAD = {pysgn.MonitorEvent.PLUGIN_LOAD} (插件生命周期)")

print("
第三方插件可注册监控钩子，收集上述事件用于性能分析或调试面板。")

# ============================================================================
# 插件架构总结
# ============================================================================

print("
" + "=" * 60)
print("插件架构总结")
print("=" * 60)
print("""
1. 官方插件（Normative）：可修改核心数值语义，如投影方案、HC运算符
2. 第三方插件（Extension）：仅扩展存储/网络/监控，沙箱隔离
3. 加载插件：    h = pysgn.load_plugin("./plugin.so")
4. 查询能力：    caps = pysgn.plugin_get_capabilities(name)
5. 切换方案：    sb.set_scheme("log_domain")
6. 晋升候选：    pysgn.plugin_propose_normative(name)
7. 生命周期：    init → register → ACTIVE → shutdown → unload
8. 核心原则：   插件是规范的"孵化器"，规范是插件的"冷冻库"
""")


print("All examples completed successfully!")
