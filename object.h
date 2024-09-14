#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

/* ##################################################################################### */

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_FUNCTION(value)  (is_obj_type (value, OBJ_FUNCTION))
#define IS_NATIVE(value)    (is_obj_type(value, OBJ_NATIVE))
#define IS_STRING(value)    (is_obj_type (value, OBJ_STRING))

#define AS_FUNCTION(value)  ((ObjFunction *) AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative *)AS_OBJ(value))->function)
#define AS_STRING(value)    ((ObjString *) AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString *) AS_OBJ(value))->chars)

/* ##################################################################################### */

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
}   Obj_t;

/* ##################################################################################### */

struct Obj {
  Obj_t type;
  struct Obj *next; 
};

/* ##################################################################################### */

typedef struct {
    Obj obj;
    int arity;  /* Stores the expected number of parameters. */
    Chunk c;
    ObjString *name;
}   ObjFunction;

/* ##################################################################################### */

typedef Value (*NativeFn)(int arg_count, Value *args);

/* ##################################################################################### */

/* Native functions are not really clox functions. 
    They are implemented in C so we need a new struct for them. */
typedef struct {
    Obj obj;
    NativeFn function;
}   ObjNative;

/* ##################################################################################### */

struct ObjString {
    Obj obj;
    int len;
    char *chars;
    uint32_t hash;
};

/* ##################################################################################### */

ObjFunction *new_function ();
ObjNative *new_native (NativeFn function);
ObjString *take_string (char *chars, int len);
ObjString *copy_string (const char *chars, int len);
void print_object (Value val);

/* ##################################################################################### */

static inline bool is_obj_type (Value value, Obj_t type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif