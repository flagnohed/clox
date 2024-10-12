#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token cur;
    Token prev;
    bool had_error;
    bool panic_mode;
}   Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
}   Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence prec;
}   ParseRule;

typedef struct {
    Token name;
    int depth;
}   Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
}   Function_t;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    Function_t type;
    Local locals[UINT8_MAX + 1];
    int local_count;
    int scope_depth;
}   Compiler;


Parser parser;
Compiler *current = NULL;
// Chunk *compiling_chunk;


static Chunk *current_chunk () { return &current->function->c; }

/* Handles error at TOKEN. */
static void error_at (Token *token, const char *msg) {
    /* If we are already panicking, we have already done this. */
    if (parser.panic_mode) return;

    parser.panic_mode = true;
    fprintf (stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) 
        fprintf (stderr, " at end");
    else if (token->type == TOKEN_ERROR) {/* NOOP. */} 
    else 
        fprintf (stderr, " at '%.*s'", token->len, token->start);

    fprintf (stderr, ": %s\n", msg);
    parser.had_error = true;
}

/* Handles error at previous token. */
static void error (const char *msg) {
    error_at (&parser.prev, msg);
}

/* Handles error at current token. */
static void error_at_current (const char *msg) {
    error_at (&parser.cur, msg);
}

/* Advances the parser one token. */
static void advance () {
    parser.prev = parser.cur;

    for (;;) {
        parser.cur = scan_token ();
        if (parser.cur.type != TOKEN_ERROR) break;

        error_at_current (parser.cur.start);
    }
}

/* Consumes a token. */
static void consume (Token_t type, const char *msg) {
    if (parser.cur.type == type) {
        advance ();
        return;
    }

    error_at_current (msg);
}

static bool check (Token_t type) {
    return parser.cur.type == type;
}

static bool match (Token_t type) {
    if (!check (type)) return false;
    advance ();
    return true;
}

/* Writes BYTE to the current chunk. */
static void emit_byte (uint8_t byte) {
    write_chunk (current_chunk (), byte, parser.prev.line);
}

/* Writes 2 bytes to the current chunk. */
static void emit_bytes (uint8_t byte1, uint8_t byte2) {
    write_chunk (current_chunk (), byte1, parser.prev.line);
    write_chunk (current_chunk (), byte2, parser.prev.line);
}

static void emit_loop (int loop_start) {
    emit_byte (OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) error ("Loop body too large.");

    emit_byte ((offset >> 8) & 0xff);
    emit_byte (offset & 0xff);
}

static int emit_jmp (uint8_t instruction) {
    /* Write placeholder operand for the jmp offset. */
    emit_byte (instruction);
    /* Use 2 bytes for the jump offset, since 16 bits allow
        jumping up to 65535 bytes of code, which should be enough. */
    emit_bytes (0xff, 0xff);
    return current_chunk()->count - 2;
}

/* Writes the return operation to the current chunk. */
static void emit_return () {
    emit_byte (OP_RETURN);
}

/* Checks if we have room for this constant and 
    if so, calls add_constant. */
static uint8_t make_constant (Value val) {
    int constant = add_constant (current_chunk (), val);
    if (constant > UINT8_MAX) {
        error ("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

static void emit_constant (Value val) {
    emit_bytes (OP_CONSTANT, make_constant (val));
}

/* Replaces the operand at the given location with the 
    calculated jmp offset. */
static void patch_jmp (int offset) {
    /* -2 to adjust for the bytecode for the jmp offset itself. */
    int jmp = current_chunk ()->count - offset - 2;

    if (jmp > UINT16_MAX) error ("Too much code to jump over.");

    current_chunk ()->code[offset] = (jmp >> 8) & 0xff;
    current_chunk ()->code[offset + 1] = jmp & 0xff;     
}

static void init_compiler (Compiler *compiler, Function_t type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function ();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copy_string (parser.prev.start,
                                               parser.prev.len);
    }

    Local *local = &current->locals[current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.len = 0;
}

static ObjFunction *end_compiler () {
    emit_return ();
    ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(), function->name != NULL
            ? function->name->chars : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

static void begin_scope () {
    current->scope_depth++;
}

static void end_scope () {
    current->scope_depth--;
    /* We need to pop the local variables off the stack
        when we exit the current scope. */
    while (current->local_count > 0 && 
           current->locals[current->local_count - 1].depth >
           current->scope_depth) {
        emit_byte (OP_POP);
        current->local_count--;
}
}

static uint8_t argument_list ();
static void expression();
static void statement();
static void declaration();
static void declare_variable ();
static uint8_t identifier_constant (Token *name);
static void parse_prec(Precedence prec);
static int resolve_local (Compiler *compiler, Token *name);
static ParseRule* get_rule(Token_t type);

static void binary (bool can_assign) {
    Token_t op_type = parser.prev.type;
    ParseRule *rule = get_rule (op_type);
    parse_prec ((Precedence) (rule->prec + 1));

    switch (op_type) {
        case TOKEN_PLUS: emit_byte (OP_ADD);                    break;
        case TOKEN_MINUS: emit_byte (OP_SUBTRACT);              break;
        case TOKEN_STAR: emit_byte (OP_MULTIPLY);               break;
        case TOKEN_SLASH: emit_byte (OP_DIVIDE);                break;
        case TOKEN_BANG_EQUAL: emit_bytes (OP_EQUAL, OP_NOT);   break;
        case TOKEN_EQUAL_EQUAL: emit_byte (OP_EQUAL);           break;
        case TOKEN_GREATER: emit_byte (OP_GREATER);             break;
        case TOKEN_GREATER_EQUAL: emit_bytes (OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte (OP_LESS);                   break;
        case TOKEN_LESS_EQUAL: emit_bytes (OP_GREATER, OP_NOT); break;
        default: return;  /* Unreachable. */
    }
}

static void call (bool can_assign) {
    uint8_t arg_count = argument_list ();
    emit_bytes (OP_CALL, arg_count);
}

static void literal (bool can_assign) {
    switch (parser.prev.type) {
        case TOKEN_FALSE: emit_byte (OP_FALSE); break;
        case TOKEN_NIL: emit_byte (OP_NIL); break;
        case TOKEN_TRUE: emit_byte (OP_TRUE); break;
        default: return;
    }
}

static void grouping (bool can_assign) {
    expression ();
    consume (TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number (bool can_assign) {
    double val = strtod (parser.prev.start, NULL);
    emit_constant (NUMBER_VAL(val));
}

static void _and (bool can_assign) {
    int end_jmp = emit_jmp (OP_JMP_IF_FALSE);
    emit_byte (OP_POP);
    parse_prec (PREC_AND);
    patch_jmp (end_jmp);
}

static void _or (bool can_assign) {
    int else_jmp = emit_jmp (OP_JMP_IF_FALSE);
    int end_jmp = emit_jmp (OP_JMP);

    patch_jmp (else_jmp);
    emit_byte (OP_POP);

    parse_prec (PREC_OR);
    patch_jmp (end_jmp);
}

static void string (bool can_assign) {
    emit_constant (OBJ_VAL(copy_string (parser.prev.start + 1,
                                        parser.prev.len - 2)));
}

static void named_variable (Token *name, bool can_assign) {
    uint8_t get_op, set_op;

    int arg = resolve_local (current, name);
    if (arg == -1) arg = identifier_constant (name);

    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;

    if (can_assign && match (TOKEN_EQUAL)) {
        expression ();
        emit_bytes (set_op, (uint8_t) arg);
    } else {
        emit_bytes (get_op, (uint8_t) arg);
    }
} 

static void variable (bool can_assign) {
    named_variable (&parser.prev, can_assign);
}

static void unary (bool can_assign) {
    Token_t op_type = parser.prev.type;
    parse_prec (PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS: emit_byte (OP_NEGATE); break;
        case TOKEN_BANG: emit_byte (OP_NOT); break;
        default: break;  /* Unreachable. */
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     _and,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     _or,   PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_prec (Precedence prec) {
    advance ();
    ParseFn prefix_rule = get_rule (parser.prev.type)->prefix;
    if (prefix_rule == NULL) {
        error ("Expect expression.");
        return;
    }
    bool can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule (can_assign);

    while (prec <= get_rule (parser.cur.type)->prec) {
        advance ();
        ParseFn infix_rule = get_rule (parser.prev.type)->infix;
        infix_rule (can_assign);
    }

    if (can_assign && match (TOKEN_EQUAL)) {
        error ("Invalid assignment target.");
    }
}

static uint8_t parse_variable (const char *error_msg) {
    consume (TOKEN_IDENTIFIER, error_msg);
    
    declare_variable ();
    if (current->scope_depth > 0) return 0;

    return identifier_constant (&parser.prev);
}

static void mark_initialized () {
    if (current->scope_depth == 0) return;
    current->locals[current->local_count - 1].depth = 
        current->scope_depth;
}

static void define_variable (uint8_t global) {
    if (current->scope_depth > 0) {
        mark_initialized ();
        return;
    } 
    emit_bytes (OP_DEFINE_GLOBAL, global);
}

static uint8_t argument_list () {
    uint8_t arg_count = 0;
    if (!check (TOKEN_RIGHT_PAREN)) {
        do {
            expression ();
            if (arg_count == 255) {
                error ("Cannot have more than 255 arguments.");
            }
            arg_count++;
        }   while (match (TOKEN_COMMA));
    }
    consume (TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

static uint8_t identifier_constant (Token *name) {
    return make_constant (OBJ_VAL(copy_string (name->start, name->len)));
}

static bool identifiers_equal (Token *a, Token *b) {
    if (a->len != b->len) return false;
    return memcmp (a->start, b->start, a->len) == 0;
}

/* Get the index of the local variable with identifier NAME. 
    Returns -1 if not found. */
static int resolve_local (Compiler *compiler, Token *name) {
    /* Walk backwards so we get the last declared variable 
        with the identifier. */
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiers_equal (name, &local->name)) {
            if (local->depth == -1) {
                error ("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void add_local (Token name) {
    if (current->local_count == UINT8_MAX + 1) {
        error ("Too many local variables in function.");
        return;
    }
    Local *local = &current->locals[current->local_count++];
    local->name = name;
    /* This is to indicate that the local is uninitialized. */
    local->depth = -1;
}

static void declare_variable () {
    if (current->scope_depth == 0) return;
    Token *name = &parser.prev;

    for (int i = current->local_count - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;    
        }

        if (identifiers_equal (name, &local->name)) {
            error ("Already a variable with this name in this scope.");
        }
    }
    add_local (*name);
}

static ParseRule *get_rule (Token_t type) {
    return &rules[type];
}

static void expression () {
    parse_prec (PREC_ASSIGNMENT);
}

static void block () {
    while (!check (TOKEN_RIGHT_BRACE) && !check (TOKEN_EOF)) {
        declaration ();
    } 
    consume (TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function (Function_t type) {
    Compiler compiler;
    init_compiler (&compiler, type);
    begin_scope ();

    consume (TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    /* Check if we have parameters. */
    if (!check (TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_current ("Cannot have more than 255 parameters.");
            }
            uint8_t constant = parse_variable ("Expect parameter name.");
            define_variable (constant);
        }   while (match (TOKEN_COMMA));
    }

    consume (TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume (TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block ();

    ObjFunction *function = end_compiler ();
    emit_bytes (OP_CONSTANT, make_constant (OBJ_VAL(function)));
}

static void fun_declaration () {
    uint8_t global = parse_variable ("Expect function name.");
    mark_initialized ();
    function (TYPE_FUNCTION);
    define_variable (global);
}

static void var_declaration () {
    uint8_t global = parse_variable ("Expect variable name.");

    if (match (TOKEN_EQUAL)) {
        expression ();
    } else {
        /* E.g., 'var a;' sets a to nil. */
        emit_byte (OP_NIL);
    }

    consume (TOKEN_SEMICOLON, 
             "Expect ';' after variable declaration.");

    define_variable (global);
}

static void expression_statement () {
    expression ();
    consume (TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte (OP_POP);
}

static void if_statement () {
    int then_jmp, else_jmp;

    consume (TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression ();
    consume (TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    /* Use backpatching to know how far to jump. */
    then_jmp = emit_jmp (OP_JMP_IF_FALSE);
    emit_byte (OP_POP);
    statement ();
    else_jmp = emit_jmp (OP_JMP);

    patch_jmp (then_jmp);

    if (match (TOKEN_ELSE)) statement ();
    patch_jmp (else_jmp);
}

static void for_statement () {
    int loop_start, exit_jmp;
    /* If a for-statement declares a variable, that variable
        should be scoped to the loop body. */
    begin_scope ();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match (TOKEN_SEMICOLON)) {
        /* No initializer. */
    } else if (match (TOKEN_VAR)) {
        var_declaration ();
    } else {
        expression_statement ();
    }
    
    loop_start = current_chunk ()->count;
    /* Check condition expression. */
    exit_jmp = -1;
    if (!match (TOKEN_SEMICOLON)) {
        expression ();
        consume (TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        /* Jump out of the loop if the condition is false. */
        exit_jmp = emit_jmp (OP_JMP_IF_FALSE);
        /* Condition. */
        emit_byte (OP_POP);
    }

    /* One pass compiler, increment clause comes before the body, but 
        executes after. So we need to jump a little bit back and forth. */
    if (!match (TOKEN_RIGHT_PAREN)) {
        int body_jmp = emit_jmp (OP_JMP);
        int inc_start = current_chunk ()->count;
        expression ();
        emit_byte (OP_POP);
        consume (TOKEN_RIGHT_PAREN, "Expect ')' after for-clauses.");

        emit_loop (loop_start);
        loop_start = inc_start;
        patch_jmp (body_jmp);
    }

    statement ();
    emit_loop (loop_start);
    /* After the loop body, we need to patch that jump. */
    if (exit_jmp != -1) {
        patch_jmp (exit_jmp);
        emit_byte (OP_POP);
    }

    end_scope ();
}

static void print_statement () {
    expression ();
    consume (TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte (OP_PRINT);
}

static void while_statement () {
    /* Capture the location to jump back to. */
    int loop_start = current_chunk ()->count;
    consume (TOKEN_LEFT_PAREN, "Ecpect '(' after 'while'.");
    expression ();
    consume (TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jmp = emit_jmp (OP_JMP_IF_FALSE);
    emit_byte (OP_POP);
    statement ();
    emit_loop (loop_start);

    patch_jmp (exit_jmp);
    emit_jmp (OP_POP);
}

/* Skip tokens until statement boundary is found (like a semicolon). 
    Or, we can look for a beginning of a statement. */
static void synchronize () {
    parser.panic_mode = false;

    while (parser.cur.type != TOKEN_EOF) {
        if (parser.prev.type == TOKEN_SEMICOLON) return;
        switch (parser.cur.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ;  /* Do nothing. */
        }

        advance ();
    }
}

static void declaration () {
    if (match (TOKEN_FUN)) {
        fun_declaration ();
    }
    else if (match (TOKEN_VAR)) {
        var_declaration ();
    } else {
        statement ();
    }

    if (parser.panic_mode) synchronize ();
}

static void statement () {
    if (match (TOKEN_PRINT)) {
        print_statement ();
    } 

    else if (match (TOKEN_IF)) {
        if_statement ();
    }

    else if (match (TOKEN_FOR)) {
        for_statement ();
    }
    
    else if (match (TOKEN_WHILE)) {
        while_statement ();
    }

    else if (match (TOKEN_LEFT_BRACE)) {
        begin_scope ();
        block ();
        end_scope ();
    } 
    
    else {
        expression_statement ();
    }
}

ObjFunction *compile (const char* source) {
    init_scanner (source);
    Compiler compiler;
    init_compiler (&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance ();
    
    while (!match (TOKEN_EOF)) {
        declaration ();
    }
        
    ObjFunction *function = end_compiler ();
    return parser.had_error ? NULL : function;
}