// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file pysgn.cpp
 * @brief Python Binding for HPDC ABI (pybind11)
 * @version 1.0.0
 *
 * Exposes the complete HPDC ABI to Python via pybind11.
 * Updated to use split header structure.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>

/* 引入拆分后的头文件 */
#include "hc/hpdc_core.h"
#include "hc/hc32.h"
#include "hc/hc64.h"
#include "hc/dc.h"
#include "hc/hpdc_sandbox.h"
#include "hc/hpdc_trie.h"
#include "hc/hpdc_engine.h"
#include "hc/hpdc_storage.h"
#include "hc/hpdc_network.h"
#include "hc/hpdc_plugin.h"
#include "hc/hpdc_normative.h"

#include <sstream>

namespace py = pybind11;

/* ============================================================================
 * HC8 Python Wrapper（与原来完全相同）
 * ============================================================================ */

struct PyHC8 {
    hc8_t raw;

    PyHC8() { raw = SGN_HC8_ZERO; }
    explicit PyHC8(hc8_t r) : raw(r) {}
    explicit PyHC8(double v) { raw = hc8_from_double(v, SGN_OVERFLOW_SATURATE); }

    double to_float() const { return hc8_to_double(raw); }

    std::string __repr__() const {
        std::ostringstream ss;
        ss << "HC8(" << to_float() << ")";
        return ss.str();
    }

    std::string __str__() const { return __repr__(); }

    py::bytes to_bytes() const {
        return py::bytes(reinterpret_cast<const char*>(&raw), sizeof(raw));
    }

    static PyHC8 from_bytes(const std::string& s) {
        PyHC8 h;
        if (s.size() >= sizeof(h.raw)) {
            memcpy(&h.raw, s.data(), sizeof(h.raw));
        }
        return h;
    }

    uint8_t getitem(size_t i) const {
        if (i >= 6) throw py::index_error("HC8 index out of range (0-5)");
        return raw.v[i];
    }
    void setitem(size_t i, uint8_t v) {
        if (i >= 6) throw py::index_error("HC8 index out of range (0-5)");
        raw.v[i] = v;
    }

    bool operator==(const PyHC8& o) const { return hc8_equal(&raw, &o.raw); }
    bool operator!=(const PyHC8& o) const { return !(*this == o); }
    bool operator<(const PyHC8& o)  const { return hc8_less(&raw, &o.raw); }
    bool operator<=(const PyHC8& o) const { return !(o < *this); }
    bool operator>(const PyHC8& o)  const { return o < *this; }
    bool operator>=(const PyHC8& o) const { return !(*this < o); }

    PyHC8 operator+(const PyHC8& o) const { return PyHC8(hc8_add_sat(&raw, &o.raw)); }
    PyHC8 operator-(const PyHC8& o) const { return PyHC8(hc8_sub(&raw, &o.raw)); }

    PyHC8& operator+=(const PyHC8& o) { raw = hc8_add_sat(&raw, &o.raw); return *this; }
    PyHC8& operator-=(const PyHC8& o) { raw = hc8_sub(&raw, &o.raw); return *this; }
};

/* ============================================================================
 * HC16 Python Wrapper（与原来相同，略作保留）
 * ============================================================================ */

struct PyHC16 {
    hc16_t raw;

    PyHC16() { raw = SGN_HC16_ZERO; }
    explicit PyHC16(hc16_t r) : raw(r) {}
    explicit PyHC16(double v) { raw = hc16_from_double(v, SGN_OVERFLOW_SATURATE); }

    double to_float() const { return hc16_to_double(raw); }

    std::string __repr__() const {
        std::ostringstream ss;
        ss << "HC16(" << to_float() << ")";
        return ss.str();
    }

    py::bytes to_bytes() const {
        return py::bytes(reinterpret_cast<const char*>(&raw), sizeof(raw));
    }

    static PyHC16 from_bytes(const std::string& s) {
        PyHC16 h;
        if (s.size() >= sizeof(h.raw)) {
            memcpy(&h.raw, s.data(), sizeof(h.raw));
        }
        return h;
    }

    uint16_t getitem(size_t i) const {
        if (i >= 4) throw py::index_error("HC16 index out of range (0-3)");
        return raw.v[i];
    }
    void setitem(size_t i, uint16_t v) {
        if (i >= 4) throw py::index_error("HC16 index out of range (0-3)");
        raw.v[i] = v;
    }

    bool operator==(const PyHC16& o) const { return hc16_equal(&raw, &o.raw); }
    bool operator<(const PyHC16& o) const { return hc16_less(&raw, &o.raw); }

    PyHC16 operator+(const PyHC16& o) const { return PyHC16(hc16_add_sat(&raw, &o.raw)); }
    PyHC16 operator-(const PyHC16& o) const { return PyHC16(hc16_sub(&raw, &o.raw)); }
    PyHC16& operator+=(const PyHC16& o) { raw = hc16_add_sat(&raw, &o.raw); return *this; }
    PyHC16& operator-=(const PyHC16& o) { raw = hc16_sub(&raw, &o.raw); return *this; }
};

/* ============================================================================
 * HC32 Python Wrapper
 * ============================================================================ */

struct PyHC32 {
    hc32_t raw;

    PyHC32() { raw = SGN_HC32_ZERO; }
    explicit PyHC32(hc32_t r) : raw(r) {}
    explicit PyHC32(double v) { raw = hc32_from_double(v, SGN_OVERFLOW_SATURATE); }

    double to_float() const { return hc32_to_double(raw); }

    std::string __repr__() const {
        std::ostringstream ss;
        ss << "HC32(" << to_float() << ")";
        return ss.str();
    }

    py::bytes to_bytes() const {
        return py::bytes(reinterpret_cast<const char*>(&raw), sizeof(raw));
    }

    static PyHC32 from_bytes(const std::string& s) {
        PyHC32 h;
        if (s.size() >= sizeof(h.raw)) {
            memcpy(&h.raw, s.data(), sizeof(h.raw));
        }
        return h;
    }

    uint32_t getitem(size_t i) const {
        if (i >= 3) throw py::index_error("HC32 index out of range (0-2)");
        return raw.v[i];
    }
    void setitem(size_t i, uint32_t v) {
        if (i >= 3) throw py::index_error("HC32 index out of range (0-2)");
        raw.v[i] = v;
    }

    bool operator==(const PyHC32& o) const { return hc32_equal(&raw, &o.raw); }
    bool operator!=(const PyHC32& o) const { return !(*this == o); }
    bool operator<(const PyHC32& o)  const { return hc32_less(&raw, &o.raw); }
    bool operator<=(const PyHC32& o) const { return !(o < *this); }
    bool operator>(const PyHC32& o)  const { return o < *this; }
    bool operator>=(const PyHC32& o) const { return !(*this < o); }

    PyHC32 operator+(const PyHC32& o) const { return PyHC32(hc32_add_sat(&raw, &o.raw)); }
    PyHC32 operator-(const PyHC32& o) const { return PyHC32(hc32_sub(&raw, &o.raw)); }
    PyHC32& operator+=(const PyHC32& o) { raw = hc32_add_sat(&raw, &o.raw); return *this; }
    PyHC32& operator-=(const PyHC32& o) { raw = hc32_sub(&raw, &o.raw); return *this; }
};

/* ============================================================================
 * HC64 Python Wrapper
 * ============================================================================ */

struct PyHC64 {
    hc64_t raw;

    PyHC64() { raw = SGN_HC64_ZERO; }
    explicit PyHC64(hc64_t r) : raw(r) {}
    explicit PyHC64(double v) { raw = hc64_from_double(v, SGN_OVERFLOW_SATURATE); }

    double to_float() const { return hc64_to_double(raw); }

    std::string __repr__() const {
        std::ostringstream ss;
        ss << "HC64(" << to_float() << ")";
        return ss.str();
    }

    py::bytes to_bytes() const {
        return py::bytes(reinterpret_cast<const char*>(&raw), sizeof(raw));
    }

    static PyHC64 from_bytes(const std::string& s) {
        PyHC64 h;
        if (s.size() >= sizeof(h.raw)) {
            memcpy(&h.raw, s.data(), sizeof(h.raw));
        }
        return h;
    }

    uint64_t getitem(size_t i) const {
        if (i >= 2) throw py::index_error("HC64 index out of range (0-1)");
        return raw.v[i];
    }
    void setitem(size_t i, uint64_t v) {
        if (i >= 2) throw py::index_error("HC64 index out of range (0-1)");
        raw.v[i] = v;
    }

    bool operator==(const PyHC64& o) const { return hc64_equal(&raw, &o.raw); }
    bool operator!=(const PyHC64& o) const { return !(*this == o); }
    bool operator<(const PyHC64& o)  const { return hc64_less(&raw, &o.raw); }
    bool operator<=(const PyHC64& o) const { return !(o < *this); }
    bool operator>(const PyHC64& o)  const { return o < *this; }
    bool operator>=(const PyHC64& o) const { return !(*this < o); }

    PyHC64 operator+(const PyHC64& o) const { return PyHC64(hc64_add_sat(&raw, &o.raw)); }
    PyHC64 operator-(const PyHC64& o) const { return PyHC64(hc64_sub(&raw, &o.raw)); }
    PyHC64& operator+=(const PyHC64& o) { raw = hc64_add_sat(&raw, &o.raw); return *this; }
    PyHC64& operator-=(const PyHC64& o) { raw = hc64_sub(&raw, &o.raw); return *this; }
};

/* ============================================================================
 * DC Python Wrapper
 * ============================================================================ */

struct PyDC {
    dc_t raw;

    PyDC() { raw.index = 0; raw.level = 0; }
    explicit PyDC(dc_t r) : raw(r) {}
    PyDC(int64_t idx, uint32_t lvl) { raw.index = idx; raw.level = lvl; }
    explicit PyDC(double v, uint32_t lvl) { raw = dc_from_double(v, lvl); }

    double to_float() const { return dc_to_double(&raw); }
    int64_t get_index() const { return raw.index; }
    uint32_t get_level() const { return raw.level; }

    std::string __repr__() const {
        std::ostringstream ss;
        ss << "DC(index=" << raw.index << ", level=" << raw.level << ")";
        return ss.str();
    }

    std::string to_json() const {
        char buf[64];
        dc_serialize(&raw, buf, sizeof(buf));
        return std::string(buf);
    }

    PyDC to_level(uint32_t target) const {
        return PyDC(dc_to_level(&raw, target));
    }

    bool operator==(const PyDC& o) const { return dc_equal(&raw, &o.raw); }
    bool operator<(const PyDC& o)  const { return dc_less(&raw, &o.raw); }

    PyDC operator+(const PyDC& o) const { return PyDC(dc_add(&raw, &o.raw)); }
    PyDC operator-(const PyDC& o) const { return PyDC(dc_sub(&raw, &o.raw)); }
    PyDC operator*(const PyDC& o) const { return PyDC(dc_mul(&raw, &o.raw)); }
};

/* ============================================================================
 * Sandbox Python Wrapper
 * ============================================================================ */

struct PySandbox {
    sandbox_t* sb;

    PySandbox(precision_t prec, uint32_t int_bits)
        : sb(sandbox_create(prec, int_bits)) {
        if (!sb) throw std::runtime_error("Failed to create sandbox");
    }
    ~PySandbox() { sandbox_destroy(sb); }

    PySandbox(const PySandbox&) = delete;
    PySandbox& operator=(const PySandbox&) = delete;

    double project(const PyHC8& h)  const { return sandbox_project_hc8(sb, h.raw); }
    double project16(const PyHC16& h) const { return sandbox_project_hc16(sb, h.raw); }
    double project32(const PyHC32& h) const { return sandbox_project_hc32(sb, h.raw); }

    PyHC8  inverse(double phi)     const { return PyHC8(sandbox_inverse_hc8(sb, phi)); }
    PyHC16 inverse16(double phi)   const { return PyHC16(sandbox_inverse_hc16(sb, phi)); }
    PyHC32 inverse32(double phi)   const { return PyHC32(sandbox_inverse_hc32(sb, phi)); }

    PyHC8  divide(const PyHC8& a, const PyHC8& b)  const {
        return PyHC8(sandbox_divide_hc8(sb, a.raw, b.raw));
    }
    PyHC16 divide16(const PyHC16& a, const PyHC16& b) const {
        return PyHC16(sandbox_divide_hc16(sb, a.raw, b.raw));
    }
    PyHC32 divide32(const PyHC32& a, const PyHC32& b) const {
        return PyHC32(sandbox_divide_hc32(sb, a.raw, b.raw));
    }

    PyHC8  gradient(const PyHC8& h, double grad, double eta) const {
        return PyHC8(sandbox_gradient_hc8(sb, h.raw, grad, eta));
    }
    PyHC16 gradient16(const PyHC16& h, double grad, double eta) const {
        return PyHC16(sandbox_gradient_hc16(sb, h.raw, grad, eta));
    }
    PyHC32 gradient32(const PyHC32& h, double grad, double eta) const {
        return PyHC32(sandbox_gradient_hc32(sb, h.raw, grad, eta));
    }

    PyHC8  scale(const PyHC8& h, double f)  const {
        return PyHC8(sandbox_scale_hc8(sb, h.raw, f));
    }
    PyHC16 scale16(const PyHC16& h, double f) const {
        return PyHC16(sandbox_scale_hc16(sb, h.raw, f));
    }
    PyHC32 scale32(const PyHC32& h, double f) const {
        return PyHC32(sandbox_scale_hc32(sb, h.raw, f));
    }

    std::vector<double> project_batch(const std::vector<PyHC8>& in) const {
        std::vector<double> out;
        out.reserve(in.size());
        for (const auto& h : in) out.push_back(project(h));
        return out;
    }

    std::vector<PyHC8> inverse_batch(const std::vector<double>& in) const {
        std::vector<PyHC8> out;
        out.reserve(in.size());
        for (double v : in) out.push_back(inverse(v));
        return out;
    }

    PyDC hc_to_dc(const PyHC8& h, uint32_t level) const {
        return PyDC(hc8_to_dc(&h.raw, level));
    }
    PyHC8 dc_to_hc(const PyDC& d, precision_t prec) const {
        return PyHC8(dc_to_hc8(&d.raw, prec));
    }

    PyHC8 map_float(const PyHC8& h, py::function fn) const {
        double phi = project(h);
        py::object result = fn(phi);
        return inverse(result.cast<double>());
    }

    PyHC8 map2_float(const PyHC8& a, const PyHC8& b, py::function fn) const {
        double pa = project(a);
        double pb = project(b);
        py::object result = fn(pa, pb);
        return inverse(result.cast<double>());
    }
};

/* ============================================================================
 * Trie Pool Wrapper
 * ============================================================================ */

struct PyTriePool {
    trie_pool_t pool;
    PyTriePool() { trie_pool_init(&pool); }
};

/* ============================================================================
 * Module Definition
 * ============================================================================ */

PYBIND11_MODULE(pysgn, m) {
    /* 初始化 ABI 常量（通过调用版本函数触发）*/
    abi_version();

    m.doc() = R"(
        pysgn - Python binding for HPDC (Hierarchical Positional Decimal Counter) ABI

        Core concept: HC numbers are discrete storage types. The Projection Sandbox
        lets you write normal float Python code while the library handles HC conversion.

        Quick start:
            import pysgn

            sb = pysgn.Sandbox(pysgn.Precision.STD, 8)
            a = pysgn.HC8(3.14159)
            b = pysgn.HC8(2.71828)
            result = sb.divide(a, b)
    )";

    /* 枚举定义（与原相同） */
    py::enum_<precision_t>(m, "Precision", R"(精度档位枚举。

ARCHIVE: 归档精度（0 层），FAST: 快速精度（2 层），STD: 标准精度（4 层），
PREC: 高精度（6 层），FUTURE: 未来扩展精度。)")
        .value("ARCHIVE", SGN_PREC_ARCHIVE)
        .value("FAST",    SGN_PREC_FAST)
        .value("STD",     SGN_PREC_STD)
        .value("PREC",    SGN_PREC_PREC)
        .value("FUTURE",  SGN_PREC_FUTURE)
        .export_values();

    py::enum_<sgn_mode_t>(m, "Mode", "运行模式：HC_ONLY 仅 HC，LEVEL_ONLY 仅 level，COMBINED 合体模式。")
        .value("HC_ONLY",    SGN_MODE_HC_ONLY)
        .value("LEVEL_ONLY", SGN_MODE_LEVEL_ONLY)
        .value("COMBINED",   SGN_MODE_COMBINED)
        .export_values();

    py::enum_<overflow_t>(m, "Overflow", "溢出处理策略：SATURATE 饱和，WRAP 回绕，CARRY 进位。")
        .value("SATURATE", SGN_OVERFLOW_SATURATE)
        .value("WRAP",     SGN_OVERFLOW_WRAP)
        .value("CARRY",    SGN_OVERFLOW_CARRY)
        .export_values();

    py::enum_<error_t>(m, "Error", R"(错误码枚举。

OK: 成功；NOMEM: 内存不足；OUT_OF_RANGE: 越界；INVALID_ARG: 非法参数；
TMR_FAULT: TMR 单点故障；TMR_MULTI_FAULT: TMR 多点故障；
RS_UNCORRECTABLE: RS 不可纠正；FILE_CORRUPT: 文件损坏；VERSION_MISMATCH: 版本不匹配。)")
        .value("OK",                SGN_OK)
        .value("NOMEM",             SGN_ERR_NOMEM)
        .value("OUT_OF_RANGE",      SGN_ERR_OUT_OF_RANGE)
        .value("INVALID_ARG",       SGN_ERR_INVALID_ARG)
        .value("TMR_FAULT",         SGN_ERR_TMR_FAULT)
        .value("TMR_MULTI_FAULT",   SGN_ERR_TMR_MULTI_FAULT)
        .value("RS_UNCORRECTABLE",  SGN_ERR_RS_UNCORRECTABLE)
        .value("FILE_CORRUPT",      SGN_ERR_FILE_CORRUPT)
        .value("VERSION_MISMATCH",  SGN_ERR_VERSION_MISMATCH)
        .export_values();

    py::enum_<plugin_type_t>(m, "PluginType", "插件类型：NORMATIVE 规范插件，EXTENSION 扩展插件，HYBRID 混合插件。")
        .value("NORMATIVE", SGN_PLUGIN_TYPE_NORMATIVE)
        .value("EXTENSION", SGN_PLUGIN_TYPE_EXTENSION)
        .value("HYBRID",    SGN_PLUGIN_TYPE_HYBRID)
        .export_values();

    py::enum_<monitor_event_t>(m, "MonitorEvent", "监控事件类型：HC_OP/WTA_COMPETE/STORAGE_IO/PLUGIN_LOAD。")
        .value("HC_OP",       SGN_EVENT_HC_OP)
        .value("WTA_COMPETE", SGN_EVENT_WTA_COMPETE)
        .value("STORAGE_IO",  SGN_EVENT_STORAGE_IO)
        .value("PLUGIN_LOAD", SGN_EVENT_PLUGIN_LOAD)
        .export_values();

    /* 版本 */
    m.def("abi_version", &abi_version, "返回 ABI 版本号（打包为 uint32：major<<16 | minor<<8 | patch）。");
    m.attr("ABI_VERSION_MAJOR") = HPDC_CORE_ABI_MAJOR;
    m.attr("ABI_VERSION_MINOR") = HPDC_CORE_ABI_MINOR;
    m.attr("ABI_VERSION_PATCH") = HPDC_CORE_ABI_PATCH;

    /* HC8 等 */
    py::class_<PyHC8>(m, "HC8", R"(HC8 超度量数：6 层 × 8 位，48 位小数精度，范围 [0, 256)。

支持加（饱和）、减、比较、序列化。可作为容器按下标访问 6 个层。
示例:
    a = pysgn.HC8(3.14)
    b = pysgn.HC8(2.71)
    c = a + b  # 饱和加法)")
        .def(py::init<>(), "构造零值 HC8。")
        .def(py::init<double>(), py::arg("value"), "由 double 构造 HC8（饱和溢出策略）。")
        .def("to_float", &PyHC8::to_float, "转换为 double（可能损失低位精度）。")
        .def("to_bytes", &PyHC8::to_bytes, "序列化为 6 字节 bytes 对象。")
        .def_static("from_bytes", &PyHC8::from_bytes, py::arg("data"), "从 bytes 反序列化为 HC8。")
        .def_static("zero", []() { return PyHC8(SGN_HC8_ZERO); }, "返回零值常量。")
        .def_static("max_val", []() { return PyHC8(SGN_HC8_MAX); }, "返回最大值常量。")
        .def_static("from_float", [](double v) { return PyHC8(v); }, py::arg("value"), "由 double 构造 HC8（等价于 HC8(value)）。")
        .def("__getitem__", &PyHC8::getitem, py::arg("index"), "按下标读取层（0-5）。")
        .def("__setitem__", &PyHC8::setitem, py::arg("index"), py::arg("value"), "按下标写入层（0-5）。")
        .def("__len__", [](const PyHC8&) { return 6; }, "返回层数 6。")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self <  py::self)
        .def(py::self <= py::self)
        .def(py::self >  py::self)
        .def(py::self >= py::self)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def("__hash__", [](const PyHC8& h) {
            return py::hash(py::bytes(reinterpret_cast<const char*>(&h.raw), sizeof(h.raw)));
        }, "基于字节内容的哈希。")
        .def("__repr__", &PyHC8::__repr__)
        .def("__str__", &PyHC8::__str__);

    /* HC16 */
    py::class_<PyHC16>(m, "HC16", "HC16 超度量数：4 层 × 16 位，范围 [0, 65536)。支持加（饱和）、减、比较、序列化。")
        .def(py::init<>(), "构造零值 HC16。")
        .def(py::init<double>(), py::arg("value"), "由 double 构造 HC16（饱和溢出策略）。")
        .def("to_float", &PyHC16::to_float, "转换为 double。")
        .def("to_bytes", &PyHC16::to_bytes, "序列化为 8 字节 bytes 对象。")
        .def_static("from_bytes", &PyHC16::from_bytes, py::arg("data"), "从 bytes 反序列化为 HC16。")
        .def_static("zero", []() { return PyHC16(SGN_HC16_ZERO); }, "返回零值常量。")
        .def_static("max_val", []() { return PyHC16(SGN_HC16_MAX); }, "返回最大值常量。")
        .def_static("from_float", [](double v) { return PyHC16(v); }, py::arg("value"), "由 double 构造 HC16（等价于 HC16(value)）。")
        .def("__getitem__", &PyHC16::getitem, py::arg("index"), "按下标读取层（0-3）。")
        .def("__setitem__", &PyHC16::setitem, py::arg("index"), py::arg("value"), "按下标写入层（0-3）。")
        .def("__len__", [](const PyHC16&) { return 4; }, "返回层数 4。")
        .def("__repr__", &PyHC16::__repr__)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self <  py::self)
        .def(py::self <= py::self)
        .def(py::self >  py::self)
        .def(py::self >= py::self)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def("__hash__", [](const PyHC16& h) {
            return py::hash(py::bytes(reinterpret_cast<const char*>(&h.raw), sizeof(h.raw)));
        }, "基于字节内容的哈希。");

    /* HC32 */
    py::class_<PyHC32>(m, "HC32", "HC32 超度量数：3 层 × 32 位，范围 [0, 2^96)。支持加（饱和）、减、比较、序列化、按下标访问。")
        .def(py::init<>(), "构造零值 HC32。")
        .def(py::init<double>(), py::arg("value"), "由 double 构造 HC32（饱和溢出策略）。")
        .def("to_float", &PyHC32::to_float, "转换为 double。")
        .def("to_bytes", &PyHC32::to_bytes, "序列化为 12 字节 bytes 对象。")
        .def_static("from_bytes", &PyHC32::from_bytes, py::arg("data"), "从 bytes 反序列化为 HC32。")
        .def_static("zero", []() { return PyHC32(SGN_HC32_ZERO); }, "返回零值常量。")
        .def_static("max_val", []() { return PyHC32(SGN_HC32_MAX); }, "返回最大值常量。")
        .def_static("from_float", [](double v) { return PyHC32(v); }, py::arg("value"), "由 double 构造 HC32（等价于 HC32(value)）。")
        .def("__getitem__", &PyHC32::getitem, py::arg("index"), "按下标读取层（0-2）。")
        .def("__setitem__", &PyHC32::setitem, py::arg("index"), py::arg("value"), "按下标写入层（0-2）。")
        .def("__len__", [](const PyHC32&) { return 3; }, "返回层数 3。")
        .def("__repr__", &PyHC32::__repr__)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self <  py::self)
        .def(py::self <= py::self)
        .def(py::self >  py::self)
        .def(py::self >= py::self)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def("__hash__", [](const PyHC32& h) {
            return py::hash(py::bytes(reinterpret_cast<const char*>(&h.raw), sizeof(h.raw)));
        }, "基于字节内容的哈希。");

    /* HC64 */
    py::class_<PyHC64>(m, "HC64", "HC64 超度量数：2 层 × 64 位，范围 [0, 2^128)。支持加（饱和）、减、比较、序列化、按下标访问。")
        .def(py::init<>(), "构造零值 HC64。")
        .def(py::init<double>(), py::arg("value"), "由 double 构造 HC64（饱和溢出策略）。")
        .def("to_float", &PyHC64::to_float, "转换为 double（v[0] 超 2^53 时会损失低位精度）。")
        .def("to_bytes", &PyHC64::to_bytes, "序列化为 16 字节 bytes 对象。")
        .def_static("from_bytes", &PyHC64::from_bytes, py::arg("data"), "从 bytes 反序列化为 HC64。")
        .def_static("zero", []() { return PyHC64(SGN_HC64_ZERO); }, "返回零值常量。")
        .def_static("max_val", []() { return PyHC64(SGN_HC64_MAX); }, "返回最大值常量。")
        .def_static("from_float", [](double v) { return PyHC64(v); }, py::arg("value"), "由 double 构造 HC64（等价于 HC64(value)）。")
        .def("__getitem__", &PyHC64::getitem, py::arg("index"), "按下标读取层（0-1）。")
        .def("__setitem__", &PyHC64::setitem, py::arg("index"), py::arg("value"), "按下标写入层（0-1）。")
        .def("__len__", [](const PyHC64&) { return 2; }, "返回层数 2。")
        .def("__repr__", &PyHC64::__repr__)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self <  py::self)
        .def(py::self <= py::self)
        .def(py::self >  py::self)
        .def(py::self >= py::self)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def("__hash__", [](const PyHC64& h) {
            return py::hash(py::bytes(reinterpret_cast<const char*>(&h.raw), sizeof(h.raw)));
        }, "基于字节内容的哈希。");

    /* DC */
    py::class_<PyDC>(m, "DC", R"(DC（Decimal Counter）：level + index 的合体计数器。

支持加减乘、比较、跨 level 转换与序列化。)")
        .def(py::init<>(), "构造零值 DC。")
        .def(py::init<int64_t, uint32_t>(), py::arg("index"), py::arg("level"), "由 index 和 level 构造 DC。")
        .def(py::init<double, uint32_t>(), py::arg("value"), py::arg("level"), "由 double 和 level 构造 DC。")
        .def("to_float", &PyDC::to_float, "转换为 double。")
        .def("to_json", &PyDC::to_json, "序列化为 JSON 字符串。")
        .def("to_level", &PyDC::to_level, py::arg("target_level"), "转换到目标 level。")
        .def_property_readonly("index", &PyDC::get_index, "index 字段。")
        .def_property_readonly("level", &PyDC::get_level, "level 字段。")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self < py::self)
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * py::self)
        .def("__repr__", &PyDC::__repr__)
        .def("__str__", &PyDC::__repr__);

    /* Sandbox 等 */
    py::class_<PySandbox>(m, "Sandbox", R"(投影沙盒：在 HC 与 float 之间转换。

构造时指定精度档位与整数位数，提供 project/inverse/divide/gradient/scale 等操作。
HC8/HC16/HC32 各有对应方法（如 project16、divide32）。)")
        .def(py::init<precision_t, uint32_t>(),
             py::arg("precision") = SGN_PREC_STD,
             py::arg("int_bits") = 8,
             "构造沙盒：precision 精度档位，int_bits 整数位数。")
        .def("project", &PySandbox::project, py::arg("h"), "HC8 投影到 double。")
        .def("project16", &PySandbox::project16, py::arg("h"), "HC16 投影到 double。")
        .def("project32", &PySandbox::project32, py::arg("h"), "HC32 投影到 double。")
        .def("inverse", &PySandbox::inverse, py::arg("phi"), "double 反投影到 HC8。")
        .def("inverse16", &PySandbox::inverse16, py::arg("phi"), "double 反投影到 HC16。")
        .def("inverse32", &PySandbox::inverse32, py::arg("phi"), "double 反投影到 HC32。")
        .def("divide", &PySandbox::divide, py::arg("a"), py::arg("b"), "HC8 除法 a/b。")
        .def("divide16", &PySandbox::divide16, py::arg("a"), py::arg("b"), "HC16 除法 a/b。")
        .def("divide32", &PySandbox::divide32, py::arg("a"), py::arg("b"), "HC32 除法 a/b。")
        .def("gradient", &PySandbox::gradient, py::arg("h"), py::arg("grad"), py::arg("eta"), "HC8 梯度下降一步。")
        .def("gradient16", &PySandbox::gradient16, py::arg("h"), py::arg("grad"), py::arg("eta"), "HC16 梯度下降一步。")
        .def("gradient32", &PySandbox::gradient32, py::arg("h"), py::arg("grad"), py::arg("eta"), "HC32 梯度下降一步。")
        .def("scale", &PySandbox::scale, py::arg("h"), py::arg("factor"), "HC8 乘以浮点因子。")
        .def("scale16", &PySandbox::scale16, py::arg("h"), py::arg("factor"), "HC16 乘以浮点因子。")
        .def("scale32", &PySandbox::scale32, py::arg("h"), py::arg("factor"), "HC32 乘以浮点因子。")
        .def("project_batch", &PySandbox::project_batch, py::arg("arr"), "批量 HC8 投影到 double 数组。")
        .def("inverse_batch", &PySandbox::inverse_batch, py::arg("arr"), "批量 double 反投影到 HC8 数组。")
        .def("map", &PySandbox::map_float, py::arg("h"), py::arg("fn"), "对 HC8 应用浮点函数 fn（project→fn→inverse）。")
        .def("map2", &PySandbox::map2_float, py::arg("a"), py::arg("b"), py::arg("fn"), "对两个 HC8 应用二元浮点函数 fn。")
        .def("hc_to_dc", &PySandbox::hc_to_dc, py::arg("h"), py::arg("level"), "HC8 转 DC（指定 level）。")
        .def("dc_to_hc", &PySandbox::dc_to_hc, py::arg("d"), py::arg("prec"), "DC 转 HC8（指定精度档位）。")
        .def("set_scheme", [](PySandbox& sb, const std::string& name) {
            (void)sb; (void)name;
            /* TODO: 在 sandbox_t 中扩展 scheme 字段后实现 */
        }, py::arg("name"), "设置投影方案（预留，当前为空实现）。");

    /* 功能函数 */
    m.def("hc8_add", [](const PyHC8& a, const PyHC8& b) {
        return PyHC8(hc8_add_sat(&a.raw, &b.raw));
    }, py::arg("a"), py::arg("b"), "HC8 饱和加法 a + b。");
    m.def("hc8_sub", [](const PyHC8& a, const PyHC8& b) {
        return PyHC8(hc8_sub(&a.raw, &b.raw));
    }, py::arg("a"), py::arg("b"), "HC8 减法 a - b（下溢饱和到 0）。");
    m.def("hc8_less", [](const PyHC8& a, const PyHC8& b) {
        return hc8_less(&a.raw, &b.raw);
    }, py::arg("a"), py::arg("b"), "HC8 字典序小于比较 a < b。");
    m.def("hc8_equal", [](const PyHC8& a, const PyHC8& b) {
        return hc8_equal(&a.raw, &b.raw);
    }, py::arg("a"), py::arg("b"), "HC8 相等比较 a == b。");
    m.def("soft_threshold", [](const PyHC8& X, const PyHC8& Lambda) {
        return PyHC8(hc8_soft_threshold(&X.raw, &Lambda.raw));
    }, py::arg("X"), py::arg("Lambda"), "HC8 软阈值 max(X - Lambda, 0)。");
    m.def("soft_threshold_array", [](const std::vector<PyHC8>& arr, const PyHC8& Lambda) {
        std::vector<PyHC8> out;
        out.reserve(arr.size());
        for (const auto& x : arr) {
            out.push_back(PyHC8(hc8_soft_threshold(&x.raw, &Lambda.raw)));
        }
        return out;
    }, py::arg("array"), py::arg("Lambda"), "对 HC8 数组逐元素软阈值。");
    m.def("hc8_checksum", [](const PyHC8& h) {
        return hc8_checksum(&h.raw);
    }, py::arg("h"), "计算 HC8 加权校验和（0-255）。");
    m.def("hc8_to_double", [](const PyHC8& h) { return hc8_to_double(h.raw); },
          py::arg("h"), "HC8 转 double。");
    m.def("double_to_hc8", [](double v) { return PyHC8(hc8_from_double(v, SGN_OVERFLOW_SATURATE)); },
          py::arg("v"), "double 转 HC8（饱和溢出）。");
    m.def("crc8_compute", [](py::buffer data) {
        py::buffer_info info = data.request();
        return crc8_compute((const uint8_t*)info.ptr, (uint8_t)info.size);
    }, py::arg("data"), "计算 buffer 的 CRC8（多项式 0x31，初值 0xFF，长度最大 255）。");
    m.def("tmr_vote", [](const PyHC8& a, const PyHC8& b, const PyHC8& c) {
        uint8_t err_mask;
        PyHC8 result(tmr_vote(&a.raw, &b.raw, &c.raw, &err_mask));
        return py::make_tuple(result, err_mask);
    }, py::arg("a"), py::arg("b"), py::arg("c"),
       "TMR 三模冗余投票，返回 (结果, 错误掩码)。");
    m.def("rs86_encode", [](py::buffer data) -> py::bytes {
        py::buffer_info info = data.request();
        if (info.size < 6) throw py::value_error("Need at least 6 bytes");
        uint8_t codeword[8];
        rs86_encode_lfsr((const uint8_t*)info.ptr, codeword);
        return py::bytes((const char*)codeword, 8);
    }, py::arg("data"), "RS(8,6) 编码：6 字节信息 → 8 字节码字。");
    m.def("rs86_decode", [](py::buffer data) -> py::tuple {
        py::buffer_info info = data.request();
        if (info.size < 8) throw py::value_error("Need at least 8 bytes");
        uint8_t codeword[8];
        memcpy(codeword, info.ptr, 8);
        bool ok = rs86_decode_lfsr(codeword);
        return py::make_tuple(ok, py::bytes((const char*)codeword, 6));
    }, py::arg("data"), "RS(8,6) 解码：返回 (是否成功, 6 字节信息)。");
    m.def("error_string", &error_string, py::arg("err"), "错误码转可读字符串。");
    m.def("quick_divide", [](const PyHC8& a, const PyHC8& b, precision_t prec, uint32_t int_bits) {
        PySandbox sb(prec, int_bits);
        return sb.divide(a, b);
    }, py::arg("a"), py::arg("b"), py::arg("precision") = SGN_PREC_STD, py::arg("int_bits") = 8,
       "便捷 HC8 除法：临时构造沙盒完成 a/b。");
    m.def("quick_gradient", [](const PyHC8& h, double grad, double eta,
                               precision_t prec, uint32_t int_bits) {
        PySandbox sb(prec, int_bits);
        return sb.gradient(h, grad, eta);
    }, py::arg("h"), py::arg("grad"), py::arg("eta"),
       py::arg("precision") = SGN_PREC_STD, py::arg("int_bits") = 8,
       "便捷 HC8 梯度下降：临时构造沙盒完成一步。");
    m.def("array_divide", [](const std::vector<PyHC8>& nums,
                             const std::vector<PyHC8>& dens,
                             precision_t prec, uint32_t int_bits) {
        if (nums.size() != dens.size()) throw py::value_error("Arrays must have same length");
        PySandbox sb(prec, int_bits);
        std::vector<PyHC8> out;
        out.reserve(nums.size());
        for (size_t i = 0; i < nums.size(); ++i)
            out.push_back(sb.divide(nums[i], dens[i]));
        return out;
    }, py::arg("numerators"), py::arg("denominators"),
       py::arg("precision") = SGN_PREC_STD, py::arg("int_bits") = 8,
       "批量 HC8 除法（两个等长数组逐元素相除）。");
    m.def("array_scale", [](const std::vector<PyHC8>& arr, double factor,
                            precision_t prec, uint32_t int_bits) {
        PySandbox sb(prec, int_bits);
        std::vector<PyHC8> out;
        out.reserve(arr.size());
        for (const auto& h : arr) out.push_back(sb.scale(h, factor));
        return out;
    }, py::arg("array"), py::arg("factor"),
       py::arg("precision") = SGN_PREC_STD, py::arg("int_bits") = 8,
       "批量 HC8 缩放（数组每个元素乘以 factor）。");
    m.def("match_bits_hamming", [](const PyHC8& a, const PyHC8& b) {
        return match_bits_hamming(&a.raw, &b.raw);
    }, py::arg("a"), py::arg("b"), "计算两个 HC8 的逐层汉明匹配数。");
    m.def("median_layerwise", [](const std::vector<PyHC8>& values) {
        std::vector<hc8_t> raw;
        raw.reserve(values.size());
        for (const auto& v : values) raw.push_back(v.raw);
        return PyHC8(median_layerwise(raw.data(), (uint16_t)raw.size()));
    }, py::arg("values"), "逐层中位数聚合 HC8 数组。");

    /* 插件相关 API */
    m.def("load_plugin", [](const std::string& path) -> plugin_handle_t* {
        plugin_handle_t* h = plugin_load(path.c_str());
        if (!h) throw py::runtime_error("Failed to load plugin: " + path);
        return h;
    }, py::arg("path"), "加载插件（返回句柄，失败抛异常）。");
    m.def("unload_plugin", [](plugin_handle_t* h) {
        if (!h) throw py::value_error("Invalid plugin handle");
        error_t err = plugin_unload(h);
        if (err != SGN_OK) throw py::runtime_error("Failed to unload plugin");
    }, py::arg("handle"), "卸载插件（失败抛异常）。");
    m.def("plugin_count", &plugin_count, "返回已加载插件数量。");
    m.def("plugin_get_name", &plugin_get_name, py::arg("handle"), "获取插件名称。");
    m.def("plugin_is_loaded", &plugin_is_loaded, py::arg("name"), "判断指定名称插件是否已加载。");
    m.def("plugin_get_capabilities", &plugin_get_capabilities, py::arg("handle"), "获取插件能力位掩码。");
    m.def("plugin_propose_normative", [](const std::string& name) {
        error_t err = plugin_propose_normative(name.c_str());
        if (err != SGN_OK) throw py::runtime_error("Failed to propose normative");
    }, py::arg("name"), "提议规范插件（失败抛异常）。");
    m.def("plugin_is_normative_candidate", &plugin_is_normative_candidate, py::arg("name"), "判断是否为规范候选插件。");
    m.def("register_static_plugin", [](const std::string& name,
                                       plugin_type_t type,
                                       uint64_t capabilities) {
        plugin_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.api_version = 1;
        desc.type = type;
        desc.name = name.c_str();
        desc.version = "1.0.0";
        desc.author = "Python binding";
        desc.abi_requirement = ">=1.0.0";
        desc.capabilities = capabilities;
        error_t err = plugin_register_static(&desc);
        if (err != SGN_OK) throw py::runtime_error("Static plugin registration failed");
    }, py::arg("name"), py::arg("type") = SGN_PLUGIN_TYPE_EXTENSION, py::arg("capabilities") = 0ULL,
       "注册静态插件（失败抛异常）。");

    /* 插件能力常量 */
    m.attr("PLUGIN_CAP_SANDBOX_CONVERTER") = (uint64_t)SGN_PLUGIN_CAP_SANDBOX_CONVERTER;
    m.attr("PLUGIN_CAP_HC_OPERATOR")        = (uint64_t)SGN_PLUGIN_CAP_HC_OPERATOR;
    m.attr("PLUGIN_CAP_PRECISION_GRADE")    = (uint64_t)SGN_PLUGIN_CAP_PRECISION_GRADE;
    m.attr("PLUGIN_CAP_ENGINE_ALGORITHM")   = (uint64_t)SGN_PLUGIN_CAP_ENGINE_ALGORITHM;
    m.attr("PLUGIN_CAP_STORAGE_DRIVER")     = (uint64_t)SGN_PLUGIN_CAP_STORAGE_DRIVER;
    m.attr("PLUGIN_CAP_NETWORK_DRIVER")     = (uint64_t)SGN_PLUGIN_CAP_NETWORK_DRIVER;
    m.attr("PLUGIN_CAP_MONITOR_HOOK")       = (uint64_t)SGN_PLUGIN_CAP_MONITOR_HOOK;
    m.attr("PLUGIN_CAP_ENCODER")            = (uint64_t)SGN_PLUGIN_CAP_ENCODER;

    /* Trie Pool 类（简化） */
    py::class_<PyTriePool>(m, "TriePool", "HC8 模板 Trie 池（简化封装）。")
        .def(py::init<>(), "初始化空 Trie 池。");
    m.def("trie_insert_template", [](PyTriePool& pool, PyHC8& sig, uint16_t tid) {
        return trie_insert_template(&pool.pool.nodes[0], &sig.raw, tid, &pool.pool);
    }, py::arg("pool"), py::arg("sig"), py::arg("tid"),
       "向 Trie 插入模板：sig 为 HC8 签名，tid 为模板 ID。");
}