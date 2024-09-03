#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm; 

static void reset_stack () {
    vm.sp = vm.stack;
}

static void runtime_error (const char *format, ...) {
    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
    fputs ("\n", stderr);

    size_t instruction = vm.ip - vm.c->code - 1;
    int line = vm.c->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

void init_VM() {
    reset_stack ();
}

void free_VM() {
}

void push (Value val) {
    *vm.sp = val;
    vm.sp++;
}

Value pop () {
    vm.sp--;
    return *vm.sp;
}

static Value peek (int distance) {
    return vm.sp[-1 - distance];
}

static bool is_falsey (Value val) {
    return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

static InterpretRes run () {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.c->constants.values[READ_BYTE()])

#define BINARY_OP(value_type, op)                         \
    do {                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error ("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;               \
        }                                                 \
      double b = AS_NUMBER(pop());                        \
      double a = AS_NUMBER(pop());                        \
      push(value_type(a op b));                           \
    } while (false)

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
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push (constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error ("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push (NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_ADD:
                BINARY_OP(NUMBER_VAL, +);
                break;
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_NOT:
                push (BOOL_VAL(is_falsey (pop ())));
                break;
            case OP_RETURN:
                print_value (pop ());
                printf ("\n");
                return INTERPRET_OK;
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}


InterpretRes interpret (const char *source) {
    Chunk c;
    init_chunk (&c);
    if (!compile (source, &c)) {
        free_chunk (&c);
        return INTERPRET_COMPILE_ERROR;
    }
    
    vm.c = &c;
    vm.ip = vm.c->code;

    InterpretRes res = run ();

    free_chunk (&c);
    return res;
}

