#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
    init_VM ();
    Chunk c;
    init_chunk (&c);

    int c_idx = add_constant (&c, 1.2);
    write_chunk (&c, OP_CONSTANT, 123);
    write_chunk (&c, c_idx, 123);
    write_chunk (&c, OP_NEGATE, 123);

    write_chunk (&c, OP_RETURN, 123);
    disassemble_chunk (&c, "test chunk");
    interpret (&c);
    free_VM ();
    free_chunk (&c);
    
    return 0;
}