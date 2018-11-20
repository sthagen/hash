//
//  hash_map.c
//  LinearProbeUniversalHashMap
//
//  Created by Thomas Mailund on 20/11/2018.
//  Copyright © 2018 Thomas Mailund. All rights reserved.
//

#include <stdlib.h>
#include "hash_map.h"

#pragma mark universal hashing

void tabulation_sample(uint32_t *start, uint32_t *end)
{
    while (start != end)
        *(start++) = rand();
}

// tabulation hashing, r=4, q=32
static uint32_t tabhash(uint32_t x, uint8_t *T)
{
    const int r = 4;
    const uint32_t no_cols = 1 << r;
    const uint32_t mask = (1 << r) - 1;
    
    uint32_t *T_ = (uint32_t*)T;
    uint32_t y = T_[0 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[1 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[2 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[3 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[4 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[5 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[6 * no_cols + (x & mask)]; x >>= r;
    y ^= T_[7 * no_cols + (x & mask)];
    
    return y;
}


#pragma mark hash table

struct bin {
    bool is_free : 1;
    bool is_deleted : 1;
    uint32_t hash_key;
    void *key;
    void *val;
};

static uint32_t
p(uint32_t k, unsigned int i, unsigned int m)
{
    return (k + i) & (m - 1);
}

static void resize(struct hash_map *table, uint32_t new_size);
static void insert_key_hashed(struct hash_map *table,
                              uint32_t hash_key, uint32_t uhash_key,
                              void *key, void *val);

static void resize(struct hash_map *table, uint32_t new_size)
{
    if (new_size == 0) return;
    
    // Remember the old bins until we have moved them.
    struct bin *old_bins = table->table;
    uint32_t old_size = table->size;
    
    // Update table so it now contains the new bins
    table->table =
    (struct bin *)malloc(new_size * sizeof(struct bin));
    struct bin *end = table->table + new_size;
    for (struct bin *bin = table->table; bin != end; ++bin) {
        bin->is_free = true;
        bin->is_deleted = false;
    }
    table->size = new_size;
    table->active = table->used = 0;
    
    // Update hash function
    tabulation_sample((uint32_t*)table->T, (uint32_t*)table->T_end);
    
    // Update rehash limit
    table->probe_limit = table->rehash_factor * new_size;
    table->operations_since_rehash = 0;
    
    
    // Move the values from the old bins to the new,
    // using the table's insertion function
    end = old_bins + old_size;
    for (struct bin *bin = old_bins; bin != end; ++bin) {
        if (bin->is_free || bin->is_deleted) continue;
        uint32_t uhash_key = tabhash(bin->hash_key, table->T);
        insert_key_hashed(table, bin->hash_key, uhash_key,
                          bin->key, bin->val);
    }
    
    // Finally, free memory for old bins
    free(old_bins);
}

static void rehash(struct hash_map *table)
{
    // Remember the old bins until we have moved them.
    struct bin *old_bins = table->table;
    uint32_t old_size = table->size;
    
    // Update table so it now contains the new bins
    table->table =
    (struct bin *)malloc(table->size * sizeof(struct bin));
    struct bin *end = table->table + table->size;
    for (struct bin *bin = table->table; bin != end; ++bin) {
        bin->is_free = true;
        bin->is_deleted = false;
    }
    
    // Update hash function
    tabulation_sample((uint32_t*)table->T, (uint32_t*)table->T_end);
    
    // Update rehash limit
    table->probe_limit = table->rehash_factor * table->size;
    table->operations_since_rehash = 0;
    
    
    // Move the values from the old bins to the new,
    // using the table's insertion function
    end = old_bins + old_size;
    for (struct bin *bin = old_bins; bin != end; ++bin) {
        if (bin->is_free || bin->is_deleted) continue;
        uint32_t uhash_key = tabhash(bin->hash_key, table->T);
        insert_key_hashed(table, bin->hash_key, uhash_key,
                          bin->key, bin->val);
    }
    
    // Finally, free memory for old bins
    free(old_bins);
}

struct hash_map *new_map(uint32_t size,
                         float rehash_factor,
                         hash_func  hash,
                         compare_func key_cmp,
                         destructor_func key_destructor,
                         destructor_func val_destructor)
{
    struct hash_map *table =
    (struct hash_map*)malloc(sizeof(struct hash_map));
    table->table =
    (struct bin *)malloc(size * sizeof(struct bin));
    struct bin *end = table->table + size;
    for (struct bin *bin = table->table; bin != end; ++bin) {
        bin->is_free = true;
        bin->is_deleted = false;
    }
    table->size = size;
    table->active = table->used = 0;
    table->hash = hash;
    table->key_cmp = key_cmp;
    table->key_destructor = key_destructor;
    table->val_destructor = val_destructor;
    
    // setting up tabulation hashing table
    int p = 32;
    int r = 4;
    int q = 32;
    int no_cols = (1 << r);
    int t = p / r;
    int bytes = t * no_cols * q / 8;
    table->T = malloc(bytes);
    table->T_end = table->T + bytes;
    tabulation_sample((uint32_t*)table->T,
                      (uint32_t*)table->T_end);
    
    table->rehash_factor = rehash_factor;
    table->probe_limit = rehash_factor * size;
    table->operations_since_rehash = 0;
    
    return table;
}

void delete_map(struct hash_map *table)
{
    struct bin *end = table->table + table->size;
    for (struct bin *bin = table->table; bin != end; ++bin) {
        if (bin->is_free || bin->is_deleted) continue;
        table->key_destructor(bin->key);
        table->val_destructor(bin->val);
    }
    free(table->table);
    free(table->T);
    free(table);
}

// Inserts when we already have the hash key. We have this to avoid
// hashing when we resize. This function does not trigger rehashing
// or resizing
static void insert_key_hashed(struct hash_map *table,
                              uint32_t hash_key, uint32_t uhash_key,
                              void *key, void *val)
{
    uint32_t index;
    for (uint32_t i = 0; i < table->size; ++i) {
        index = p(uhash_key, i, table->size);
        struct bin *bin = & table->table[index];
        
        if (bin->is_free) {
            bin->hash_key = hash_key;
            bin->key = key;
            bin->val = val;
            bin->is_free = bin->is_deleted = false;
            
            // we have one more active element
            // and one more unused cell changes character
            table->active++; table->used++;
            break;
        }
        
        if (bin->is_deleted) {
            bin->hash_key = hash_key; bin->key = key;
            bin->is_free = bin->is_deleted = false;
            
            // we have one more active element
            // but we do not use more cells since the
            // deleted cell was already used.
            table->active++;
            break;
        }
        
        if (bin->hash_key == hash_key) {
            if (table->key_cmp(bin->key, key)) {
                table->key_destructor(bin->key);
                table->val_destructor(bin->val);
                bin->key = key;
                bin->val = val;
                return; // Done
            } else {
                // we have found the key but with as
                // different key... keep on searching
                continue;
            }
        }
    }
    
}
void map(struct hash_map *table, void *key, void *val)
{
    table->operations_since_rehash++;
    if (table->operations_since_rehash > table->probe_limit) {
        rehash(table);
    }
    
    uint32_t hash_key = table->hash(key);
    uint32_t uhash_key = tabhash(hash_key, table->T);
    insert_key_hashed(table, hash_key, uhash_key, key, val);
    
    if (table->used > table->size / 2)
        resize(table, table->size * 2);
}

bool contains_key(struct hash_map *table, void *key)
{
    table->operations_since_rehash++;
    if (table->operations_since_rehash > table->probe_limit) {
        rehash(table);
    }
    
    uint32_t hash_key = table->hash(key);
    uint32_t uhash_key = tabhash(hash_key, table->T);
    for (uint32_t i = 0; i < table->size; ++i) {
        uint32_t index = p(uhash_key, i, table->size);
        struct bin *bin = & table->table[index];
        if (bin->is_free)
            return false;
        if (!bin->is_deleted && bin->hash_key == hash_key &&
            table->key_cmp(bin->key, key))
            return true;
    }
    return false;
}

void *lookup(struct hash_map *table, void *key)
{
    table->operations_since_rehash++;
    if (table->operations_since_rehash > table->probe_limit) {
        rehash(table);
    }
    
    uint32_t hash_key = table->hash(key);
    uint32_t uhash_key = tabhash(hash_key, table->T);
    for (uint32_t i = 0; i < table->size; ++i) {
        uint32_t index = p(uhash_key, i, table->size);
        struct bin *bin = & table->table[index];
        if (bin->is_free)
            return 0;
        if (!bin->is_deleted && bin->hash_key == hash_key &&
            table->key_cmp(bin->key, key))
            return bin->val;
    }
    return 0;
}


void delete_key(struct hash_map *table, void *key)
{
    table->operations_since_rehash++;
    if (table->operations_since_rehash > table->probe_limit) {
        rehash(table);
    }
    
    uint32_t hash_key = table->hash(key);
    uint32_t uhash_key = tabhash(hash_key, table->T);
    for (uint32_t i = 0; i < table->size; ++i) {
        uint32_t index = p(uhash_key, i, table->size);
        struct bin * bin = & table->table[index];
        
        if (bin->is_free) return;
        
        if (!bin->is_deleted && bin->hash_key == hash_key &&
            table->key_cmp(bin->key, key)) {
            bin->is_deleted = true;
            table->key_destructor(table->table[index].key);
            table->val_destructor(table->table[index].val);
            table->active--;
            break;
        }
    }
    
    if (table->active < table->size / 8)
        resize(table, table->size / 2);
}