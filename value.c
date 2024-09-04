#include <stdio.h>
#include <string.h>

#include "object.h"
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
        case VAL_NIL:    printf ("nil"); break;
        case VAL_NUMBER: printf ("%g", AS_NUMBER(val)); break;
        case VAL_OBJ:    print_object (val); break;
    }
}

bool values_equal (Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:       return true;
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: {
            ObjString *a_str = AS_STRING(a);
            ObjString *b_str = AS_STRING(b);
            return a_str->len == b_str->len && 
                memcmp (a_str->chars, b_str->chars, a_str->len) == 0;
        }
            
        default:            return false;  /* Unreachable. */
    }
}