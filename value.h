#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
}   ValueArray;


void init_value_array (ValueArray *arr);
void write_value_array (ValueArray *arr, Value val);
void free_value_array (ValueArray *arr);
void print_value (Value val);

#endif