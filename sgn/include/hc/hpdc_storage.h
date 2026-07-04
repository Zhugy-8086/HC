/**
 * @file hpdc_storage.h
 * @brief HPDC 存储与可靠�?ABI - 文件格式、压缩、校验、纠�? * @version 1.0.0
 *
 * 提供 HPDC 模型文件的存储格式、RLE 压缩、Merkle 树、Reed-Solomon 纠错�? * CRC8 校验、TMR 投票等可靠性功能�? *
 * 依赖：hpdc_core.h
 */

#ifndef HPDC_STORAGE_H
#define HPDC_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hc/hpdc_core.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 模型文件格式（磁盘布局�? * ============================================================================ */

#pragma pack(1)

/**
 * 文件头（23 字节�? */
typedef struct {
    uint8_t  magic[4];      /**< "SGN\0" */
    uint8_t  version;       /**< 0x01 */
    uint32_t global_level;  /**< 精度等级�?/2/4/6/8�?*/
    uint32_t num_records;   /**< 模板记录总数 N */
    uint8_t  merkle_root[8];/**< HC Merkle 根哈希（8 字节�?*/
    uint8_t  rs_parity[2];  /**< Reed-Solomon 校验（预留） */
} file_header_t;

/**
 * 文件记录�?3 字节�? 合体模式存储格式
 */
typedef struct {
    uint8_t  mode;          /**< 0x02 = combined */
    uint8_t  sign;          /**< 0=�?1=�?*/
    uint8_t  int_part;      /**< 整数部分 0-255 */
    uint8_t  frac[6];       /**< 小数�?frac[0..5] */
    uint32_t level;         /**< 存储�?level（小端序�?*/
} file_record_t;

#pragma pack()

/* ============================================================================
 * 文件 I/O（平台相关，需用户实现底层读写�? * ============================================================================ */

/**
 * 写入模型文件�? * @param path    文件路径
 * @param hdr     文件�? * @param records 记录数组
 * @return 0 成功，非 0 失败
 */
int file_write(const char* path, const file_header_t* hdr,
                   const file_record_t* records);

/**
 * 读取模型文件�? * @param path         文件路径
 * @param hdr          输出文件�? * @param records      输出记录数组（调用方分配�? * @param max_records  最大记录数
 * @return 实际读取的记录数，负数表示错�? */
int file_read(const char* path, file_header_t* hdr,
                  file_record_t* records, uint32_t max_records);

/**
 * 精度升级：将记录从低精度转换为高精度（补零）�? * @param rec  记录指针
 * @param from 原精度（小数层数�? * @param to   目标精度（必须大�?from�? */
void file_grade_up(file_record_t* rec, precision_t from, precision_t to);

/**
 * 精度降级：将记录从高精度截断为低精度�? * @param rec  记录指针
 * @param from 原精�? * @param to   目标精度（必须小�?from�? */
void file_grade_down(file_record_t* rec, precision_t from, precision_t to);

/* ============================================================================
 * RLE 压缩（针对模板序列）
 * ============================================================================ */

#pragma pack(1)

/**
 * RLE 压缩项：记录与前一个模板的公共前缀长度及剩余后缀�? */
typedef struct {
    uint8_t  prefix_depth;  /**< 与前一个模板相同的字节层数�?-6�?*/
    uint8_t  suffix_len;    /**< 后缀字节数（6 - prefix_depth�?*/
    uint8_t  suffix[6];     /**< 后缀字节（实际只使用 suffix_len 个） */
    uint16_t template_id;   /**< 模板 ID */
} rle_item_t;

#pragma pack()

/**
 * RLE 编码：将 HC8 模板数组压缩�?RLE 项数组�? * @param templates 输入 HC8 数组
 * @param N         模板数量
 * @param out       输出 RLE 项数组（调用方分配足够空间）
 * @return 实际输出�?RLE 项数�? */
uint32_t rle_encode(const hc8_t* templates, uint16_t N, rle_item_t* out);

/**
 * RLE 解码：从 RLE 项恢�?HC8 模板数组�? * @param in       RLE 项数�? * @param n_items  RLE 项数�? * @param out      输出 HC8 数组（调用方分配至少对应模板数量�? * @return 恢复的模板数�? */
uint32_t rle_decode(const rle_item_t* in, uint32_t n_items, hc8_t* out);

/* ============================================================================
 * Merkle 树（用于完整性校验）
 * ============================================================================ */

/**
 * Merkle 哈希�? 字节，实际为 HC8 的某种压缩或直接使用 HC8 �?8 字节�? */
typedef struct {
    uint8_t bytes[8];
} merkle_hash_t;

/**
 * Merkle 树（完全二叉树，数组存储�? */
typedef struct {
    merkle_hash_t nodes[512];  /**< 节点数组，最�?512 个叶�?*/
    uint16_t          leaf_start;  /**< 第一个叶子节点的索引 */
} merkle_tree_t;

/**
 * 初始�?Merkle 树�? * @param tree      树结�? * @param n_leaves  叶子节点数量（将被补齐到 2 的幂�? */
void merkle_init(merkle_tree_t* tree, uint16_t n_leaves);

/**
 * 更新指定叶子的哈希值，并向上重算父节点路径�? * @param tree      树结�? * @param tid       模板 ID（叶子索引）
 * @param new_leaf  新的叶子哈希�? */
void merkle_update_leaf(merkle_tree_t* tree, uint16_t tid,
                            const merkle_hash_t* new_leaf);

/**
 * 获取 Merkle 根哈希�? * @param tree 树结�? * @return 根哈希（只读，指�?tree->nodes[0]�? */
const merkle_hash_t* merkle_root(const merkle_tree_t* tree);

/* ============================================================================
 * Reed-Solomon (8,6) 纠错�? * ============================================================================ */

/**
 * RS(8,6) 编码：将 6 字节信息码编码为 8 字节码字�? * @param info     6 字节输入
 * @param codeword 8 字节输出（调用方分配�? */
void rs86_encode_lfsr(const uint8_t info[6], uint8_t codeword[8]);

/**
 * RS(8,6) 解码：尝试纠正最�?1 个错误字节�? * @param codeword  8 字节码字（输入输出，解码后前 6 字节为信息）
 * @return true 成功（无错误或已纠正），false 不可纠正
 */
bool rs86_decode_lfsr(uint8_t codeword[8]);

/* ============================================================================
 * CRC8 校验
 * ============================================================================ */

/**
 * 计算 CRC8（多项式 0x31，初�?0xFF）�? * @param data 数据指针
 * @param len  数据长度（最�?255�? * @return CRC8 �? */
uint8_t crc8_compute(const uint8_t* data, uint8_t len);

/* ============================================================================
 * HC8 校验�? * ============================================================================ */

/**
 * 计算 HC8 的加权校验和（用于运行时完整性检查）�? * @param hc HC8 指针
 * @return 校验和（0-255�? */
uint8_t hc8_checksum(const hc8_t* hc);

/**
 * 增量更新校验和（当修改某一层字节时）�? * @param old_cs    原校验和
 * @param layer     修改的层索引�?-5�? * @param old_byte  原字节�? * @param new_byte  新字节�? * @return 新校验和
 */
uint8_t checksum_delta(uint8_t old_cs, uint8_t layer,
                           uint8_t old_byte, uint8_t new_byte);

/**
 * 运行时全局嗅探：异或所有计数器的校验和是否匹配预期值�? * @param counters         计数器数�? * @param N                数组长度
 * @param expected_global_cs 预期全局校验�? * @return true 匹配，false 不匹配（可能内存损坏�? */
bool runtime_sniff(const hc8_t* counters, uint16_t N, uint8_t expected_global_cs);

/* ============================================================================
 * TMR 三模冗余投票
 * ============================================================================ */

/**
 * TMR 投票：三�?HC8 副本，按字节层多数投票�? * @param a,b,c      三个输入
 * @param error_mask 输出错误掩码（bit0=int_part? 实际 bit0 对应 layer0, bit5 对应 layer5�? * @return 投票结果
 */
hc8_t tmr_vote(const hc8_t* a, const hc8_t* b, const hc8_t* c,
                       uint8_t* error_mask);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HPDC_STORAGE_H */