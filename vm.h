#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"


#define STACK_MAX 256


typedef struct {
    Chunk* c;
    uint8_t *ip;            /* Instruction pointer. */
    Value stack[STACK_MAX];
    Value *sp;              /* Points to the top of stack. */
    Table strings;          /* Used for string interning. */
    Obj *objects;           /* Used in garbage collection. */
}   VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
}   InterpretRes;

extern VM vm;

void init_VM();
void free_VM();
InterpretRes interpret (const char *source);
void push (Value val);
Value pop ();



#endif