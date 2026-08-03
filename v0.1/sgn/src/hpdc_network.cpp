// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 zhugy-8086

/**
 * @file hpdc_network.cpp
 * @brief HPDC 网络与分布式 ABI 实现
 * @version 1.0.0
 */

#include "hc/hpdc_network.h"
#include "hc/hpdc_storage.h"
#include <cstring>

/* ============================================================================
 * 网络实现
 * ============================================================================ */

/* UART 帧打包 */
void uart_pack(uint8_t type, const uint8_t* hc_data, uint8_t len, uint8_t* out_frame) {
    if (!hc_data || !out_frame) return;
    if (len > 251) return;  /* 帧最大长度: 4 (头) + 251 (数据) + 1 (CRC) = 256 */
    out_frame[0] = 0xAA;
    out_frame[1] = 0x55;
    out_frame[2] = type;
    out_frame[3] = len;
    memcpy(&out_frame[4], hc_data, len);
    out_frame[4 + len] = crc8_compute(&out_frame[2], 2 + len);
}

/* UART 帧解包 */
bool uart_parse(const uint8_t* frame, uint8_t frame_len, uart_frame_t* out) {
    if (!frame || !out || frame_len < 5) return false;
    if (frame[0] != 0xAA || frame[1] != 0x55) return false;
    out->sync[0] = frame[0];
    out->sync[1] = frame[1];
    out->type = frame[2];
    out->len = frame[3];
    if (4 + out->len >= frame_len) return false;
    out->payload = (uint8_t*)&frame[4];
    out->crc8 = frame[4 + out->len];
    uint8_t computed = crc8_compute(&frame[2], 2 + out->len);
    return computed == out->crc8;
}

/* COBS 编码 */
uint8_t cobs_encode(const uint8_t* in, uint8_t len, uint8_t* out) {
    if (!in || !out || len + 2 > 255) return 0;
    uint8_t out_idx = 1;
    uint8_t code = 1;
    uint8_t code_idx = 0;
    for (uint8_t i = 0; i < len; ++i) {
        if (in[i] == 0) {
            out[code_idx] = code;
            code_idx = out_idx++;
            code = 1;
        } else {
            out[out_idx++] = in[i];
            code++;
            if (code == 0xFF) {
                out[code_idx] = code;
                code_idx = out_idx++;
                code = 1;
            }
        }
    }
    out[code_idx] = code;
    return out_idx;
}

/* COBS 解码 */
uint8_t cobs_decode(const uint8_t* in, uint8_t len, uint8_t* out) {
    if (!in || !out || len == 0) return 0;
    uint8_t out_idx = 0;
    uint8_t in_idx = 0;
    while (in_idx < len) {
        uint8_t code = in[in_idx++];
        for (uint8_t i = 1; i < code && in_idx < len; ++i) {
            out[out_idx++] = in[in_idx++];
        }
        if (code < 0xFF && in_idx < len && out_idx > 0) {
            out[out_idx++] = 0;
        }
    }
    return out_idx;
}

/* Lamport 逻辑时钟 */
void lamport_update(hc16_lamport_t* local, const hc16_lamport_t* remote) {
    if (!local || !remote) return;
    if (lamport_compare(local, remote) < 0) {
        *local = *remote;
    }
    uint16_t old_ss0 = local->ss[0];
    local->ss[0] += 65;
    if (local->ss[0] < old_ss0) {
        for (int i = 1; i < 4; ++i) {
            if (++local->ss[i] != 0) break;
        }
        if (local->ss[3] == 0) {
            if (++local->sec == 0) local->level++;
        }
    }
}

int lamport_compare(const hc16_lamport_t* a, const hc16_lamport_t* b) {
    if (!a || !b) return 0;
    if (a->level != b->level) return (a->level < b->level) ? -1 : 1;
    if (a->sec != b->sec) return (a->sec < b->sec) ? -1 : 1;
    for (int i = 0; i < 4; ++i) {
        if (a->ss[i] != b->ss[i]) return (a->ss[i] < b->ss[i]) ? -1 : 1;
    }
    return 0;
}

/* 看门狗定时器 */
bool is_timeout(const hc16_time_t* deadline, const hc16_time_t* now) {
    if (!deadline || !now) return false;
    /* 返回 true 表示 now >= deadline（截止时刻已到/已过）
     * 用 now - deadline：若 now 远早于 deadline，uint16 下溢为大值 */
    uint16_t diff = now->sec - deadline->sec;
    if (diff > 32768) return false;   /* now 远在 deadline 之前 */
    if (diff != 0) return true;       /* now 在 deadline 之后 */
    for (int i = 0; i < 4; ++i) {
        if (now->ss[i] != deadline->ss[i]) {
            return now->ss[i] > deadline->ss[i];
        }
    }
    return true;  /* now == deadline，算已到 */
}

void wdt_start(wdt_entry_t* slots, uint8_t task_id,
                    const hc16_time_t* deadline, void (*callback)(uint8_t)) {
    if (!slots || !deadline) return;
    if (task_id >= SGN_WDT_SLOTS) return;
    slots[task_id].deadline = *deadline;
    slots[task_id].task_id = task_id;
    slots[task_id].active = 1;
    slots[task_id].on_timeout = callback;
}

void wdt_stop(wdt_entry_t* slots, uint8_t task_id) {
    if (!slots) return;
    if (task_id < SGN_WDT_SLOTS) slots[task_id].active = 0;
}

void wdt_poll(wdt_entry_t* slots, uint8_t n_slots, const hc16_time_t* now) {
    if (!slots || !now) return;
    if (n_slots > SGN_WDT_SLOTS) n_slots = SGN_WDT_SLOTS;
    for (uint8_t i = 0; i < n_slots; ++i) {
        if (slots[i].active && is_timeout(&slots[i].deadline, now)) {
            slots[i].active = 0;
            if (slots[i].on_timeout) slots[i].on_timeout(slots[i].task_id);
        }
    }
}