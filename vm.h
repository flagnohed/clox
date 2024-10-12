#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX + 1))

typedef struct {
    ObjFunction *function;
    uint8_t *ip;            /* Instruction pointer. */
    Value *slots;
}   CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value *sp;              /* Points to the top of stack. */
    Table globals;          /* Global variables. */
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