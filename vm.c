#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm; 

static void reset_stack () {
    vm.sp = vm.stack;
}

void init_VM() {
    reset_stack ();
}

void free_VM() {
}

static InterpretRes run () {

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.c->constants.values[READ_BYTE()])

    uint8_t instruction;
    Value constant;

    for (;;) {
#ifdef DEBUG_TRACE_EXEC
        printf ("       ");
        for (Value *slot = vm.stack; slot < vm.sp; slot++) {
            printf ("[ ");
            print_value (*slot);
            printf (" ]");
        }
        printf ("\n");
        disassemble_instruction (vm.c, (int) (vm.ip - vm.c->code));
#endif
        
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: 
                constant = READ_CONSTANT();
                push (constant);
                break;
            case OP_NEGATE:
                push (-pop ());
                break;
            case OP_RETURN:
                print_value (pop ());
                printf ("\n");
                return INTERPRET_OK;
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
}

void push (Value val) {
    *vm.sp = val;
    vm.sp++;
}

Value pop () {
    vm.sp--;
    return *vm.sp;
}


InterpretRes interpret (Chunk *c) {
    vm.c = c;
    vm.ip = vm.c->code;
    return run (); 
}

