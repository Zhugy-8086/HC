/**
 * @file hpdc_trie.cpp
 * @brief HPDC Trie 索引 ABI 实现
 * @version 1.0.0
 */

#include "hc/hpdc_trie.h"
#include <cstring>

/* ============================================================================
 *  Trie 实现
 * ============================================================================ */

void trie_pool_init(trie_pool_t* pool) {
    memset(pool, 0, sizeof(*pool));
    pool->next_free = 1;
    pool->nodes[0].byte = 0;
    pool->nodes[0].template_id = 0xFFFF;
    pool->nodes[0].child_count = 0;
    pool->nodes[0].children = nullptr;
}

trie_node_t* trie_pool_alloc(trie_pool_t* pool) {
    if (pool->next_free >= 2048) return nullptr;
    trie_node_t* node = &pool->nodes[pool->next_free++];
    memset(node, 0, sizeof(*node));
    node->template_id = 0xFFFF;
    return node;
}

trie_node_t* trie_find_child(const trie_node_t* node, uint8_t target) {
    if (!node || node->child_count == 0 || !node->children) return nullptr;
    for (uint16_t i = 0; i < node->child_count; ++i) {
        if (node->children[i].byte == target) {
            return &node->children[i];
        }
    }
    return nullptr;
}

trie_node_t* trie_find_path(trie_node_t* root, const hc8_t* hc, int depth) {
    trie_node_t* node = root;
    for (int layer = 0; layer < depth && node; ++layer) {
        uint8_t b = hc->v[layer];
        trie_node_t* child = trie_find_child(node, b);
        if (!child) return node;
        node = child;
    }
    return node;
}

trie_node_t* trie_insert_child(trie_node_t* parent, uint8_t b,
                                        trie_pool_t* pool) {
    trie_node_t* existing = trie_find_child(parent, b);
    if (existing) return existing;

    if (parent->child_count == 0) {
        if (pool->next_free + 16 >= 2048) return nullptr;
        parent->children = &pool->nodes[pool->next_free];
        pool->next_free += 16;
    }

    if (parent->child_count >= 16) return nullptr;
    trie_node_t* child = &parent->children[parent->child_count];
    child->byte = b;
    child->template_id = 0xFFFF;
    child->child_count = 0;
    child->children = nullptr;
    parent->child_count++;
    return child;
}

bool trie_insert_template(trie_node_t* root, const hc8_t* sig,
                               uint16_t tid, trie_pool_t* pool) {
    trie_node_t* node = root;
    for (int layer = 0; layer < 6; ++layer) {
        uint8_t b = sig->v[layer];
        trie_node_t* child = trie_insert_child(node, b, pool);
        if (!child) return false;
        node = child;
    }
    node->template_id = tid;
    return true;
}

void trie_lazy_delete(trie_node_t* leaf) {
    if (leaf) leaf->template_id = 0xFFFF;
}

void trie_collect_leaves(const trie_node_t* node,
                              uint16_t* out_ids, uint16_t* out_count,
                              uint16_t max_count) {
    if (!node || *out_count >= max_count) return;
    if (SGN_TRIE_IS_LEAF(node)) {
        out_ids[(*out_count)++] = node->template_id;
        return;
    }
    for (uint16_t i = 0; i < node->child_count && *out_count < max_count; ++i) {
        trie_collect_leaves(&node->children[i], out_ids, out_count, max_count);
    }
}

void trie_collect_candidates(const trie_node_t* root, const hc8_t* query,
                                  int k, uint16_t* cand_ids, uint16_t* cand_count) {
    *cand_count = 0;
    if (!root || !query || k <= 0 || k > 6) return;
    const trie_node_t* node = root;
    for (int layer = 0; layer < k; ++layer) {
        uint8_t b = query->v[layer];
        trie_node_t* child = trie_find_child(node, b);
        if (!child) return;
        node = child;
    }
    trie_collect_leaves(node, cand_ids, cand_count, SGN_MAX_CANDIDATES);
}