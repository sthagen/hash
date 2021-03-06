//
//  hash_set.h
//  ChainedHashSet
//
//  Created by Thomas Mailund on 19/11/2018.
//  Copyright © 2018 Thomas Mailund. All rights reserved.
//

#ifndef hash_set_h
#define hash_set_h

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t (*hash_func)(void *);
typedef void (*destructor_func)(void *);
typedef bool (*compare_func)(void *, void *);

struct hash_set {
    struct linked_list *table;
    uint32_t size;
    uint32_t used;
    hash_func hash;
    compare_func cmp;
    destructor_func destructor;
};

struct hash_set *
new_set        (uint32_t size, // Must be a power of two!
                    hash_func hash,
                    compare_func cmp,
                    destructor_func destructor);
void delete_set  (struct hash_set *table);

void insert_key  (struct hash_set *table,
                  void *key);
bool contains_key(struct hash_set *table,
                  void *key);
void delete_key  (struct hash_set *table,
                  void *key);

#endif /* hash_set_h */
