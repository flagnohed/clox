#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"


typedef enum {
    OP_CONSTANT,
    OP_NIL, 
    OP_TRUE,
    OP_FALSE,
    OP_NEGATE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_RETURN,
    
}   OpCode;

typedef struct {
    int capacity;
    int count;
    int *lines;
    uint8_t *code;
    ValueArray constants;
}   Chunk;


void init_chunk (Chunk *c);
void free_chunk (Chunk *c);
void write_chunk (Chunk *c, uint8_t byte, int line);
int add_constant (Chunk *c, Value val);

#endif

