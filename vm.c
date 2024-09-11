#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
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

/* Initiates the virtual machine. */
void init_VM() {
    reset_stack ();
    vm.objects = NULL;
    init_table (&vm.globals);
    init_table (&vm.strings);
}

void free_VM() {
    free_table (&vm.globals);
    free_table (&vm.strings);
    free_objects ();
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

static void concatenate () {
    ObjString *b = AS_STRING(pop ());
    ObjString *a = AS_STRING(pop ());

    int len = a->len + b->len;
    char *chars = ALLOCATE(char, len + 1);
    memcpy (chars, a->chars, a->len);
    memcpy (chars + a->len, b->chars, b->len);
    chars[len] = '\0';

    ObjString *res = take_string (chars, len);
    push (OBJ_VAL(res));

}

static InterpretRes run () {
#define READ_BYTE()     (*vm.ip++)
#define READ_CONSTANT() (vm.c->constants.values[READ_BYTE()])
#define READ_SHORT() \
    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_STRING()   AS_STRING(READ_CONSTANT())
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
            case OP_POP: pop (); break;
            case OP_EQUAL: {
                Value b = pop ();
                Value a = pop ();
                push (BOOL_VAL(values_equal (a, b)));
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                table_set (&vm.globals, name, peek (0));
                pop ();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value val;
                if (!table_get (&vm.globals, name, &val)) {
                    runtime_error ("Undefined variable '%s'.", 
                                   name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push (val);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (table_set (&vm.globals, name, peek (0))) {
                    table_delete (&vm.globals, name);
                    runtime_error ("Undefined variable '%s'.",
                                   name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push (vm.stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm.stack[slot] = peek (0);
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error ("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push (NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate ();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop ());
                    double a = AS_NUMBER(pop ());
                    push (NUMBER_VAL (a + b));
                } else {
                    runtime_error (
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }      
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push (BOOL_VAL(is_falsey (pop ())));
                break;
            case OP_PRINT: {
                print_value (pop ());
                printf ("\n");
                break;
            }
            case OP_JMP: {
                uint16_t offset = READ_SHORT();
                vm.ip += offset;
                break;
            }
            case OP_JMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey (peek (0))) vm.ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm.ip -= offset;
                break;
            }
            case OP_RETURN: return INTERPRET_OK;
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
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

