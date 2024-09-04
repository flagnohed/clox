#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, obj_type) \
    (type *)allocate_object(sizeof(type), obj_type)

static Obj *allocate_object (size_t size, Obj_t type) {
    Obj *object = (Obj *)reallocate (NULL, 0, size);
    object->type = type;

    /* We also need to add it to the VM's list of objects. */
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString *allocate_string (char *chars, int len) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->len = len;
    string->chars = chars;
    return string;
}

ObjString *take_string (char *chars, int len) {
    return allocate_string (chars, len);
}

ObjString* copy_string(const char* chars, int len) {
  char* heap_chars = ALLOCATE(char, len + 1);
  memcpy(heap_chars, chars, len);
  heap_chars[len] = '\0';
  return allocateString(heap_chars, len);
}

void print_object (Value val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            printf ("%s", AS_CSTRING(val));
            break;
    }
}