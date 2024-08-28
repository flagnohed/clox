#include "chunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    
    Chunk c;
    init_chunk (&c);
    write_chunk (&c, OP_RETURN);
    disassemble_chunk (&c, "test chunk");
    free_chunk (&c);
    
    return 0;
}