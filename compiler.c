#include <stdio.h>
#include <stdlib.h>

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

typedef struct {
    void (*prefix)();
    void (*infix)();
    Precedence prec;
}   ParseRule;

Parser parser;
Chunk *compiling_chunk;

static Chunk *current_chunk () {
    return compiling_chunk;
}

static void error_at (Token *token, const char *msg) {
    if (parser.panic_mode) return;

    parser.panic_mode = true;
    fprintf (stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf (stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        //NOOP
    } else {
        fprintf (stderr, " at '%.*s'", token->len, token->start);
    }

    fprintf (stderr, ": %s\n", msg);
    parser.had_error = true;

}

static void error (const char *msg) {
    error_at (&parser.prev, msg);
}

static void error_at_current (const char *msg) {
    error_at (&parser.cur, msg);
}

static void advance () {
    parser.prev = parser.cur;

    for (;;) {
        parser.cur = scan_token ();
        if (parser.cur.type != TOKEN_ERROR) break;

        error_at_current (parser.cur.start);
    }
}

static void consume (Token_t type, const char *msg) {
    if (parser.cur.type == type) {
        advance ();
        return;
    }

    error_at_current (msg);
}

static void emit_byte (uint8_t byte) {
    write_chunk (current_chunk (), byte, parser.prev.line);
}

static void emite_bytes (uint8_t byte1, uint8_t byte2) {
    write_chunk (current_chunk (), byte1, parser.prev.line);
    write_chunk (current_chunk (), byte2, parser.prev.line);
}

static void emit_return () {
    emit_byte (OP_RETURN);
}

static uint8_t make_constant (Value val) {
    int constant = add_constant (current_chunk (), val);
    if (constant > UINT8_MAX) {
        error ("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

static void end_compiler () {
    emit_return ();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk (current_chunk (), "code");
    }
#endif
}

static void expression();
static ParseRule* get_rule(Token_t type);
static void parse_prec(Precedence prec);

static void binary () {
    Token_t op_type = parser.prev.type;
    ParseRule *rule = get_rule (op_type);
    parse_prec ((Precedence) (rule->prec + 1));

    switch (op_type) {
        case TOKEN_PLUS: emit_byte (OP_ADD); break;
        case TOKEN_MINUS: emit_byte (OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte (OP_MULTIPLY); break;
        case TOKEN_SLASH: emot_byte (OP_DIVIDE); break;
        default: return;  /* Unreachable. */
    }
}

static void grouping () {
    expression ();
    consume (TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number () {
    double val = strod (parser.prev.start, NULL);
    emit_constant (val);
}

static void unary () {
    Token_t op_type = parser.prev.type;
    parse_prec (PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS: emit_byte (OP_NEGATE); break;
        default: break;  /* Unreachable. */
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
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
    [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_prec (Precedence prec) {
    advance ();
    void (*prefix_rule)() = get_rule (parser.prev.type)->prefix;
    if (prefix_rule == NULL) {
        error ("Expect expression.");
        return;
    }

    prefix_rule ();

    while (prec <= get_rule (parser.cur.type)->prec) {
        advance ();
        void (*infix_rule)() = get_rule (parser.prev.type)->infix;
        infix_rule ();
    }
}

static ParseRule *get_rule (Token_t type) {
    return &rules[type];
}

static void expression () {
    parse_prec (PREC_ASSIGNMENT);
}

bool compile (const char* source, Chunk *c) {
    init_scanner (source);
    parser.had_error = false;
    parser.panic_mode = false;
    advance ();
    expression ();
    consume (TOKEN_EOF, "Expect end of expression.");
    end_compiler ();
    return !parser.had_error;
}