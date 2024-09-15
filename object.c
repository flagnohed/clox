#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

/* ##################################################################################### */

#define ALLOCATE_OBJ(type, obj_type) \
    (type *)allocate_object(sizeof(type), obj_type)

/* ##################################################################################### */

static Obj *allocate_object (size_t size, Obj_t type) {
    Obj *object = (Obj *)reallocate (NULL, 0, size);
    object->type = type;

    /* We also need to add it to the VM's list of objects. */
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

/* ##################################################################################### */

ObjFunction *new_function () {
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    init_chunk (&function->c);
    return function;
}

/* ##################################################################################### */

ObjNative *new_native (NativeFn function) {
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

/* ##################################################################################### */

static ObjString *allocate_string (char *chars, int len, 
                                   uint32_t hash) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->len = len;
    string->chars = chars;
    string->hash = hash;
    table_set (&vm.strings, string, NIL_VAL);
    return string;
}

/* ##################################################################################### */

/* Hash a given string with FNV-1a algorithm. */
uint32_t hash_string (const char *key, int len) {
    /* Initial hash value. */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }

    return hash;
}

/* ##################################################################################### */

ObjString *take_string (char *chars, int len) {
    uint32_t hash = hash_string (chars, len);
    ObjString *interned = table_find_string (&vm.strings, chars,
                                             len, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, len + 1);
        return interned;
    }
    return allocate_string (chars, len, hash);
}

/* ##################################################################################### */

ObjString* copy_string(const char* chars, int len) {
    uint32_t hash = hash_string (chars, len);
    ObjString *interned = table_find_string (&vm.strings, chars, 
                                             len, hash);
    if (interned != NULL) return interned;
    char* heap_chars = ALLOCATE(char, len + 1);
    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';
    return allocate_string(heap_chars, len, hash);
}

/* ##################################################################################### */

static void print_function (ObjFunction *function) {
    if (function->name == NULL) {
        printf ("<script>");
        return;
    }
    printf ("<fn %s>", function->name->chars);
}

/* ##################################################################################### */

void print_object (Value val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_FUNCTION:
            print_function (AS_FUNCTION(val));
            break;
        case OBJ_NATIVE:
            printf ("<native fn>");
            break;
        case OBJ_STRING:
            printf ("%s", AS_CSTRING(val));
            break;
    }
}