#include "kernel/hashtable.h"
#include "kernel/kheap.h"
#include "kernel/string.h"
#include "libc/string.h"

static uint32_t hash_function(const char *str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (uint8_t)(*str); // hash * 33 + c
        str++;
    }
    return hash % HASH_TABLE_SIZE;
}

void delete_hash_table(hash_table_t *table) {
    if (!table) return;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_node_t *node = table->nodes[i];
        while (node) {
            hash_node_t *temp = node;
            node = node->next;
            kfree(temp->key);
            kfree(temp);
        }
    }
}

int hash_set(hash_table_t *table, const char *key, void *value) {
    if (!table || !key) return -1;

    uint32_t index = hash_function(key);
    hash_node_t *new_node = kmalloc(sizeof(hash_node_t));
    if (!new_node) return -1;

    // Copy key string
    new_node->key = strdup(key);
    if (!new_node->key) {
        kfree(new_node);
        return -1;
    }

    new_node->value = value;
    new_node->next = table->nodes[index];  // Insert at head
    table->nodes[index] = new_node;

    return 0;
}

void *hash_get(hash_table_t *table, const char *key) {
    if (!table || !key) return NULL;

    uint32_t index = hash_function(key);
    hash_node_t *current = table->nodes[index];

    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current->value;  // Found
        }
        current = current->next;
    }
    return NULL;  // Not found
}

int hash_remove(hash_table_t *table, const char *key) {
    if (!table || !key) return -1;

    uint32_t index = hash_function(key);
    hash_node_t *current = table->nodes[index];
    hash_node_t *prev = NULL;

    while (current) {
        if (strcmp(current->key, key) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                table->nodes[index] = current->next;
            }
            kfree(current->key);
            kfree(current);
            return 0;  // Success
        }
        prev = current;
        current = current->next;
    }
    return -1;  // Not found
}

void hash_print(hash_table_t *table) {
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_node_t *node = table->nodes[i];
        while (node) {
            printf("Key: %s, Value: %p\n", node->key, node->value);
            node = node->next;
        }
    }
}