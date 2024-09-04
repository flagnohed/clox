#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_STRING(value)    (is_obj_type (value, OBJ_STRING))

#define AS_STRING(value)    ((ObjString *) AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString *) AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
}   Obj_t;


struct Obj {
  Obj_t type;
  struct Obj *next; 
};

struct ObjString {
    Obj obj;
    int len;
    char *chars;
};

ObjString *take_string (char *chars, int len);
ObjString *copy_string (const char *chars, int len);

static inline bool is_obj_type (Value value, Obj_t type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif