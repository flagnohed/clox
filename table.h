#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString *key;
    Value val;
}   Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void init_table (Table *table);
void free_table (Table *table);
bool table_get (Table *table, ObjString *key, Value *val);
bool table_set (Table *table, ObjString *key, Value val);
bool table_delete (Table *table, ObjString *key);
void table_add_all (Table *from, Table *to);
ObjString *table_find_string (Table *table, const char *chars,
                              int len, uint32_t hash);

#endif