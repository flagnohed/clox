#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_FUNCTION(value)  (is_obj_type (value, OBJ_FUNCTION))
#define IS_STRING(value)    (is_obj_type (value, OBJ_STRING))

#define AS_FUNCTION(value)  ((ObjFunction *) AS_OBJ(value))
#define AS_STRING(value)    ((ObjString *) AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString *) AS_OBJ(value))->chars)

typedef enum {
    OBJ_FUNCTION,
    OBJ_STRING,
}   Obj_t;


struct Obj {
    Obj_t type;
    struct Obj *next; 
};

typedef struct {
    Obj obj;
    int arity;
    Chunk c;
    ObjString *name;
}   ObjFunction;

struct ObjString {
    Obj obj;
    int len;
    char *chars;
    uint32_t hash;
};

ObjFunction *new_function ();
ObjString *take_string (char *chars, int len);
ObjString *copy_string (const char *chars, int len);
void print_object (Value val);

static inline bool is_obj_type (Value value, Obj_t type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif