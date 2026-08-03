/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 zhugy-8086
 */

/**
 * @file sgn.h
 * @brief SGN 技术栈统一入口（C 末尾版）
 * @version 2.0.0
 *
 * 本文件为 SGN C 语言实现的末尾版本，仅聚合本目录下实际存在的头文件：
 * HC8 / HC16 核心类型，以及可选的 SIMD 批量运算接口。
 * HC32 / HC64 / DC / 沙盒 / Trie / 引擎 / 存储 / 网络 / 插件 / C++ 包装
 * 等扩展在此版本中不再提供。
 *
 * 嵌入式项目也可按需包含单个头文件（如 hc8.h、hc16.h）以最小化链接体积。
 */

#ifndef SGN_H
#define SGN_H

#include "hc/hc.h"
#include "hc/hc8.h"
#include "hc/hc16.h"

#ifdef SGN_USE_SIMD
#include "hc/hc_simd.h"
#endif

#endif /* SGN_H */