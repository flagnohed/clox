#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

/* ##################################################################################### */

VM vm; 

/* ##################################################################################### */

static Value clock_native (int arg_count, Value *args) {
    return NUMBER_VAL((double) clock () / CLOCKS_PER_SEC);
}

/* ##################################################################################### */

static void reset_stack () {
    vm.sp = vm.stack;
    vm.frame_count = 0;
}

/* ##################################################################################### */

static void runtime_error (const char *format, ...) {
    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
    fputs ("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->function;
        size_t instruction = frame->ip - function->c.code - 1;
        fprintf(stderr, "[line %d] in ", 
            function->c.lines[instruction]);
        if (function->name == NULL) {
            fprintf (stderr, "script\n");
        } 
        else {
            fprintf (stderr, "%s()\n", function->name->chars);
        }
    }
    reset_stack();
}

/* ##################################################################################### */

/* Takes a pointer to a C function and the name it will be given in clox. */
static void define_native (const char *name, NativeFn function) {
    push (OBJ_VAL(copy_string (name, (int) strlen (name))));
    push (OBJ_VAL(new_native (function)));
    table_set (&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop (); 
    pop ();
}

/* ##################################################################################### */

/* Initiates the virtual machine. */
void init_VM() {
    reset_stack ();
    vm.objects = NULL;
    init_table (&vm.globals);
    init_table (&vm.strings);

    define_native ("clock", clock_native);
}

/* ##################################################################################### */

void free_VM() {
    free_table (&vm.globals);
    free_table (&vm.strings);
    free_objects ();
}

/* ##################################################################################### */

void push (Value val) {
    *vm.sp = val;
    vm.sp++;
}

/* ##################################################################################### */

Value pop () {
    vm.sp--;
    return *vm.sp;
}

/* ##################################################################################### */

static Value peek (int distance) {
    return vm.sp[-1 - distance];
}

/* ##################################################################################### */

/* Puts the called function into a new frame. */
static bool call (ObjFunction *function, int arg_count) {
    if (arg_count != function->arity) {
        runtime_error ("Expected %d arguments but got %d.", 
                       function->arity, arg_count);
        return false;
    }
    /* Probably a bug in some runaway recursive code. */
    if (vm.frame_count == FRAMES_MAX) {
        runtime_error ("Stack overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frame_count++];
    frame->function = function;
    frame->ip = function->c.code;
    frame->slots = vm.sp - arg_count - 1;
    return true;
}

/* ##################################################################################### */

static bool call_value (Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION:
                return call (AS_FUNCTION(callee), arg_count);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native (arg_count, vm.sp - arg_count);
                vm.sp -= arg_count + 1;
                push (result);
                return true;
            }
            default:
                break;  /* Non-callable object type. */
        }
    }
    runtime_error ("Can only call functions and classes.");
    return false;
}

/* ##################################################################################### */

static bool is_falsey (Value val) {
    return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

/* ##################################################################################### */

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

/* ##################################################################################### */

static InterpretRes run () {
    /* Current topmost CallFrame. */
    CallFrame *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_CONSTANT() (frame->function->c.constants.values[READ_BYTE()])
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
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
        disassemble_instruction (&frame->function->c, 
                                (int) (frame->ip - frame->function->c.code));
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
            /* Accesses the current frame's slots array, which means
               it accesses the given numbered slot relative to the 
               beginning of that frame. */
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push (frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek (0);
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
                frame->ip += offset;
                break;
            }
            case OP_JMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey (peek (0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_BYTE();
                if (!call_value (peek (arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OP_RETURN: {
                Value result = pop ();
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    /* We have finished executing the top-level code, 
                      so the entire program is done. */
                    pop ();
                    return INTERPRET_OK;
                }
                vm.sp = frame->slots;
                push (result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

/* ##################################################################################### */

InterpretRes interpret (const char *source) {
    ObjFunction *function = compile (source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    /* Set up call frame for code executed at top level. */
    call (function, 0);
    return run ();
}

/* ##################################################################################### */