// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hpdc_plugin.cpp
 * @brief HPDC 插件架构 ABI 实现
 * @version 1.0.0
 */

#include "hc/hpdc_plugin.h"
#include "hc/hpdc_normative.h"
#include <cstring>
#include <cstdlib>

/* ============================================================================
 * 插件实现
 * ============================================================================ */

/* 平台相关的动态库加载 */
#if defined(_WIN32) || defined(_WIN64)
#   include <windows.h>
#   define SGN_PLUGIN_DLOPEN(path)  LoadLibraryA(path)
#   define SGN_PLUGIN_DLSYM(h, sym) GetProcAddress((HMODULE)(h), sym)
#   define SGN_PLUGIN_DLCLOSE(h)    FreeLibrary((HMODULE)(h))
#   define SGN_PLUGIN_DLERROR()     "Windows DLL error"
    typedef HMODULE plugin_dlhandle_t;
#else
#   include <dlfcn.h>
#   define SGN_PLUGIN_DLOPEN(path)  dlopen(path, RTLD_NOW | RTLD_LOCAL)
#   define SGN_PLUGIN_DLSYM(h, sym) dlsym(h, sym)
#   define SGN_PLUGIN_DLCLOSE(h)    dlclose(h)
#   define SGN_PLUGIN_DLERROR()     dlerror()
    typedef void* plugin_dlhandle_t;
#endif

/* 插件内部状态 */
typedef enum {
    SGN_PLUGIN_STATE_LOADED    = 0,
    SGN_PLUGIN_STATE_ACTIVE    = 1,
    SGN_PLUGIN_STATE_CANDIDATE = 2,
    SGN_PLUGIN_STATE_FROZEN    = 3,
    SGN_PLUGIN_STATE_FAILED    = 4,
} plugin_state_t;

/* 插件句柄实现 */
struct plugin_handle {
    plugin_desc_t   desc;
    plugin_ctx_t    ctx;
    plugin_dlhandle_t dlhandle;
    plugin_state_t  state;
    bool                is_static;
};

/* 全局插件槽位 */
#define SGN_MAX_PLUGINS 16
static plugin_handle_t plugin_slots[SGN_MAX_PLUGINS];
static uint32_t            plugin_count_val = 0;

/* 前向声明内部辅助函数 */
static bool plugin_validate_capabilities(const plugin_desc_t* desc);
static int  plugin_invoke_init(plugin_handle_t* h);
static int  plugin_invoke_register(plugin_handle_t* h);
static void plugin_fill_context(plugin_handle_t* h);

/* 内部辅助函数 */
static bool plugin_validate_capabilities(const plugin_desc_t* desc) {
    if (!desc) return false;
    uint64_t caps = desc->capabilities;
    switch (desc->type) {
        case SGN_PLUGIN_TYPE_NORMATIVE:
        case SGN_PLUGIN_TYPE_HYBRID:
            if (caps & ~(SGN_PLUGIN_CAP_NORMATIVE_MASK | SGN_PLUGIN_CAP_EXTENSION_MASK)) {
                return false;
            }
            return true;
        case SGN_PLUGIN_TYPE_EXTENSION:
            if (caps & SGN_PLUGIN_CAP_NORMATIVE_MASK) return false;
            if (caps & ~SGN_PLUGIN_CAP_EXTENSION_MASK) return false;
            return true;
        default:
            return false;
    }
}

static void plugin_fill_context(plugin_handle_t* h) {
    if (!h) return;
    memset(&h->ctx, 0, sizeof(h->ctx));
    h->ctx.sandbox    = nullptr;
    h->ctx.user_data  = nullptr;
    h->ctx.last_error = SGN_OK;
    memset(h->ctx.error_msg, 0, sizeof(h->ctx.error_msg));
}

static int plugin_invoke_init(plugin_handle_t* h) {
    if (!h || h->state != SGN_PLUGIN_STATE_LOADED) return -1;
    if (!h->desc.init) {
        h->state = SGN_PLUGIN_STATE_ACTIVE;
        return 0;
    }
    int rc = h->desc.init(&h->ctx);
    if (rc == 0) {
        h->state = SGN_PLUGIN_STATE_ACTIVE;
    } else {
        h->state = SGN_PLUGIN_STATE_FAILED;
        h->ctx.last_error = SGN_ERR_INVALID_ARG;
    }
    return rc;
}

static int plugin_invoke_register(plugin_handle_t* h) {
    if (!h || h->state != SGN_PLUGIN_STATE_ACTIVE) return -1;
    int rc = 0;
    if ((h->desc.type == SGN_PLUGIN_TYPE_NORMATIVE || h->desc.type == SGN_PLUGIN_TYPE_HYBRID)
        && h->desc.register_normative) {
        normative_registry_t reg;
        memset(&reg, 0, sizeof(reg));
        rc = h->desc.register_normative(&h->ctx, &reg);
        if (rc != 0) {
            h->state = SGN_PLUGIN_STATE_FAILED;
            return rc;
        }
    }
    if ((h->desc.type == SGN_PLUGIN_TYPE_EXTENSION || h->desc.type == SGN_PLUGIN_TYPE_HYBRID)
        && h->desc.register_extension) {
        extension_registry_t reg;
        memset(&reg, 0, sizeof(reg));
        rc = h->desc.register_extension(&h->ctx, &reg);
        if (rc != 0) {
            h->state = SGN_PLUGIN_STATE_FAILED;
            return rc;
        }
    }
    return rc;
}

static int plugin_find_free_slot(void) {
    for (uint32_t i = 0; i < SGN_MAX_PLUGINS; ++i) {
        if (plugin_slots[i].state == SGN_PLUGIN_STATE_FAILED ||
            plugin_slots[i].state == SGN_PLUGIN_STATE_LOADED) {
            if (plugin_slots[i].dlhandle != nullptr && !plugin_slots[i].is_static) {
                SGN_PLUGIN_DLCLOSE(plugin_slots[i].dlhandle);
            }
            memset(&plugin_slots[i], 0, sizeof(plugin_slots[i]));
            return (int)i;
        }
    }
    return -1;
}

static plugin_handle_t* plugin_find_by_name(const char* name) {
    if (!name) return nullptr;
    for (uint32_t i = 0; i < SGN_MAX_PLUGINS; ++i) {
        if (plugin_slots[i].state == SGN_PLUGIN_STATE_ACTIVE ||
            plugin_slots[i].state == SGN_PLUGIN_STATE_CANDIDATE ||
            plugin_slots[i].state == SGN_PLUGIN_STATE_FROZEN) {
            if (plugin_slots[i].desc.name &&
                strcmp(plugin_slots[i].desc.name, name) == 0) {
                return &plugin_slots[i];
            }
        }
    }
    return nullptr;
}

/* 插件管理器 API 实现 */
plugin_handle_t* plugin_load(const char* path) {
    if (!path) return nullptr;
    int slot = plugin_find_free_slot();
    if (slot < 0) return nullptr;
    plugin_handle_t* h = &plugin_slots[slot];
    plugin_dlhandle_t dl = SGN_PLUGIN_DLOPEN(path);
    if (!dl) {
        h->state = SGN_PLUGIN_STATE_FAILED;
        strncpy(h->ctx.error_msg, SGN_PLUGIN_DLERROR(), sizeof(h->ctx.error_msg) - 1);
        return nullptr;
    }
    plugin_desc_t (*get_desc)(void) =
        (plugin_desc_t (*)(void))SGN_PLUGIN_DLSYM(dl, "plugin_get_desc");
    if (!get_desc) {
        SGN_PLUGIN_DLCLOSE(dl);
        h->state = SGN_PLUGIN_STATE_FAILED;
        strncpy(h->ctx.error_msg, "missing plugin_get_desc", sizeof(h->ctx.error_msg) - 1);
        return nullptr;
    }
    plugin_desc_t desc = get_desc();
    memcpy(&h->desc, &desc, sizeof(plugin_desc_t));
    h->dlhandle = dl;
    h->is_static = false;
    h->state = SGN_PLUGIN_STATE_LOADED;
    if (!plugin_validate_capabilities(&h->desc)) {
        SGN_PLUGIN_DLCLOSE(dl);
        h->state = SGN_PLUGIN_STATE_FAILED;
        strncpy(h->ctx.error_msg, "capability validation failed", sizeof(h->ctx.error_msg) - 1);
        return nullptr;
    }
    plugin_fill_context(h);
    if (plugin_invoke_init(h) != 0) {
        SGN_PLUGIN_DLCLOSE(dl);
        return nullptr;
    }
    if (plugin_invoke_register(h) != 0) {
        if (h->desc.shutdown) h->desc.shutdown(&h->ctx);
        SGN_PLUGIN_DLCLOSE(dl);
        return nullptr;
    }
    plugin_count_val++;
    return h;
}

error_t plugin_unload(plugin_handle_t* h) {
    if (!h) return SGN_ERR_INVALID_ARG;
    if (h->state == SGN_PLUGIN_STATE_FROZEN) {
        return SGN_ERR_INVALID_ARG;
    }
    if (h->desc.shutdown && (h->state == SGN_PLUGIN_STATE_ACTIVE ||
                              h->state == SGN_PLUGIN_STATE_CANDIDATE)) {
        h->desc.shutdown(&h->ctx);
    }
    if (!h->is_static && h->dlhandle) {
        SGN_PLUGIN_DLCLOSE(h->dlhandle);
    }
    memset(h, 0, sizeof(plugin_handle_t));
    if (plugin_count_val > 0) plugin_count_val--;
    return SGN_OK;
}

bool plugin_is_loaded(const char* name) {
    return plugin_find_by_name(name) != nullptr;
}

error_t plugin_register_static(const plugin_desc_t* desc) {
    if (!desc) return SGN_ERR_INVALID_ARG;
    if (!plugin_validate_capabilities(desc)) {
        return SGN_ERR_INVALID_ARG;
    }
    if (plugin_is_loaded(desc->name)) {
        return SGN_ERR_INVALID_ARG;
    }
    int slot = plugin_find_free_slot();
    if (slot < 0) return SGN_ERR_NOMEM;
    plugin_handle_t* h = &plugin_slots[slot];
    memcpy(&h->desc, desc, sizeof(plugin_desc_t));
    h->dlhandle = nullptr;
    h->is_static = true;
    h->state = SGN_PLUGIN_STATE_LOADED;
    plugin_fill_context(h);
    if (plugin_invoke_init(h) != 0) {
        memset(h, 0, sizeof(plugin_handle_t));
        return SGN_ERR_INVALID_ARG;
    }
    if (plugin_invoke_register(h) != 0) {
        if (h->desc.shutdown) h->desc.shutdown(&h->ctx);
        memset(h, 0, sizeof(plugin_handle_t));
        return SGN_ERR_INVALID_ARG;
    }
    plugin_count_val++;
    return SGN_OK;
}

uint32_t plugin_count(void) {
    return plugin_count_val;
}

const char* plugin_get_name(uint32_t idx) {
    uint32_t active = 0;
    for (uint32_t i = 0; i < SGN_MAX_PLUGINS; ++i) {
        if (plugin_slots[i].state == SGN_PLUGIN_STATE_ACTIVE ||
            plugin_slots[i].state == SGN_PLUGIN_STATE_CANDIDATE ||
            plugin_slots[i].state == SGN_PLUGIN_STATE_FROZEN) {
            if (active == idx) {
                return plugin_slots[i].desc.name;
            }
            active++;
        }
    }
    return nullptr;
}

uint64_t plugin_get_capabilities(const char* name) {
    plugin_handle_t* h = plugin_find_by_name(name);
    if (!h) return 0;
    return h->desc.capabilities;
}

error_t plugin_propose_normative(const char* name) {
    plugin_handle_t* h = plugin_find_by_name(name);
    if (!h) return SGN_ERR_INVALID_ARG;
    if (h->desc.type != SGN_PLUGIN_TYPE_NORMATIVE &&
        h->desc.type != SGN_PLUGIN_TYPE_HYBRID) {
        return SGN_ERR_INVALID_ARG;
    }
    if (h->state != SGN_PLUGIN_STATE_ACTIVE) {
        return SGN_ERR_INVALID_ARG;
    }
    h->state = SGN_PLUGIN_STATE_CANDIDATE;
    return SGN_OK;
}

bool plugin_is_normative_candidate(const char* name) {
    plugin_handle_t* h = plugin_find_by_name(name);
    if (!h) return false;
    return (h->state == SGN_PLUGIN_STATE_CANDIDATE);
}

error_t plugin_freeze_normative(const char* name) {
    plugin_handle_t* h = plugin_find_by_name(name);
    if (!h) return SGN_ERR_INVALID_ARG;
    if (h->state != SGN_PLUGIN_STATE_CANDIDATE) {
        return SGN_ERR_INVALID_ARG;
    }
    h->state = SGN_PLUGIN_STATE_FROZEN;
    return SGN_OK;
}