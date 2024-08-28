#include "chunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    
    Chunk c;
    init_chunk (&c);

    int c_idx = add_constant (&c, 1.2);
    write_chunk (&c, OP_CONSTANT, 123);
    write_chunk (&c, c_idx, 123);

    write_chunk (&c, OP_RETURN, 123);
    disassemble_chunk (&c, "test chunk");
    free_chunk (&c);
    
    return 0;
}