#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"


#define STACK_MAX 256


typedef struct {
    Chunk* c;
    uint8_t *ip;            // instruction pointer
    Value stack[STACK_MAX];
    Value *sp;              // points to the top of stack
}   VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
}   InterpretRes;

void init_VM();
void free_VM();
InterpretRes interpret (const char *source);
void push (Value val);
Value pop ();



#endif