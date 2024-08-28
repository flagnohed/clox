#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassemble_chunk (Chunk *c, const char *name);
int disassemble_instruction (Chunk *c, int offset);

#endif