#include <stdio.h>

#include "memory.h"
#include "value.h"

void init_value_array (ValueArray *arr) {
  arr->capacity = 0;
  arr->count = 0;
  arr->values = NULL;
}

void write_value_array (ValueArray *arr, Value val) {
    if (arr->count == arr->capacity) {
        int old_capacity = arr->capacity;
        arr->capacity = GROW_CAPACITY(old_capacity);
        arr->values = GROW_ARRAY(Value, arr->values,
            old_capacity, arr->capacity);
    }

    arr->values[arr->count] = val;
    arr->count++;
}

void free_value_array (ValueArray *arr) {
    FREE_ARRAY(Value, arr->values, arr->capacity);
    init_value_array (arr);
}

void print_value (Value val) {
    switch (val.type) {
        case VAL_BOOL:
            printf (AS_BOOL(val) ? "true" : "false");
            break;
        case VAL_NIL: printf ("nil"); break;
        case VAL_NUMBER: printf ("%g", AS_NUMBER(val)); break;
    }
}