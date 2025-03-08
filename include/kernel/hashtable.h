#pragma once

#include <stdint.h>

#define HASH_TABLE_SIZE 256

typedef struct hash_node {
    char *key;
    void *value;
    struct hash_node *next;
} hash_node_t;

typedef struct hash_table {
    hash_node_t *nodes[HASH_TABLE_SIZE];
} hash_table_t;

void delete_hash_table(hash_table_t *table);
int hash_set(hash_table_t *table, const char *key, void *value);
void *hash_get(hash_table_t *table, const char *key);
int hash_remove(hash_table_t *table, const char *key);
void hash_print(hash_table_t *table);