#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

/* Might be optimized in the future, 
    now somewhat arbitrary. */
#define TABLE_MAX_LOAD 0.75

/* ##################################################################################### */

void init_table (Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

/* ##################################################################################### */

void free_table (Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table (table);
}

/* ##################################################################################### */

static Entry *find_entry (Entry *entries, int capacity,
                          ObjString *key) {
    uint32_t i = key->hash % capacity;
    Entry *tombstone = NULL;
    for (;;) {
        Entry *entry = &entries[i];
        if (entry->key == NULL) {
            if (IS_NIL(entry->val)) {
                /* Empty entry. */
                return tombstone != NULL ? tombstone : entry;
            } else {
                /* We found a tombstone. */
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            /* Found the key. */
            return entry;
        }

        i = (i + 1) % capacity;
    }
}

/* ##################################################################################### */

static void adjust_capacity (Table *table, int capacity) {
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].val = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry *dest = find_entry (entries, capacity, entry->key);
        dest->key = entry->key;
        dest->val = entry->val;
        table->count++;
    }

    /* Release the memory from the old array. */
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

/* ##################################################################################### */

/* Get the corresponding value of key if it exists and
    place it in val. */
bool table_get (Table *table, ObjString *key, Value *val) {
    if (table->count == 0) return false;

    Entry *entry = find_entry (table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *val = entry->val;
    return true;
}

/* ##################################################################################### */

/* Adds the given key/val pair to the given hash table. */
bool table_set (Table *table, ObjString *key, Value val) {
    /* Check if we have room. If not, grow capacity. */
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity (table, capacity);
    }
    Entry *entry = find_entry (table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    
    /* Count tombstones as well. */
    if (is_new_key && IS_NIL(entry->val)) table->count++;

    entry->key = key;
    entry->val = val;
    return is_new_key;
}

/* ##################################################################################### */

bool table_delete (Table *table, ObjString *key) {
    if (table->count == 0) return false;

    /* Find the entry. */
    Entry *entry = find_entry (table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    /* Place a tombstone in memory. */
    entry->key = NULL;
    entry->val = BOOL_VAL(true);
    return true;
}

/* ##################################################################################### */

void table_add_all (Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set (to, entry->key, entry->val);
        }
    }
}

/* ##################################################################################### */

ObjString *table_find_string (Table *table, const char *chars,
                              int len, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t i = hash % table->capacity;
    for (;;) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) {
            /* Stop if we find an empty non-tombstone entry. */
            if (IS_NIL(entry->val)) return NULL;
        } else if (entry->key->len == len &&
                   entry->key->hash == hash &&
                   memcmp (entry->key->chars, chars, len) == 0) {
            /* Found it! */
            return entry->key;
        }

        i = (i + 1) % table->capacity;
    }
}