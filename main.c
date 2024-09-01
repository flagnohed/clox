#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

#define EX_USAGE 64
#define EX_COMPILE 65
#define EX_RUNTIME 70
#define EX_FILE 74  
#define MAX_LINE_LEN 1024

static void repl () {
    char line[MAX_LINE_LEN];
    for (;;) {
        printf ("> ");

        if (!fgets (line, sizeof (line), stdin)) {
            printf ("\n");
            break;
        }

        interpret (line);
    }
}

static char *read_file (const char *path) {
    FILE *f = fopen (path, "rb");
    if (f == NULL) {
        fprintf (stderr, "Could not open file \"%s\".\n", path);
        exit (EX_FILE);
    }

    fseek (f, 0L, SEEK_END);
    size_t fsize = ftell (f);   
    rewind (f);

    char *buf = (char *) malloc (fsize + 1);
    if (buf == NULL) {
        fprintf (stderr, "Not enough memory to read \"%s\".\n", path);
        exit (EX_FILE);
    }
    size_t bytes_read = fread (buf, sizeof (char), fsize, f);
    if (bytes_read < fsize) {
        fprintf (stderr, "Could not read file \"%s\".\n", path);
        exit (EX_FILE);
    }
    buf[bytes_read] = '\0';

    fclose (f);
    return buf;
}

static void run_file (const char *path) {
    char *source = read_file (path);
    InterpretRes res = interpret (source);
    free (source);

    if (res == INTERPRET_COMPILE_ERROR) exit (EX_COMPILE);
    if (res == INTERPRET_RUNTIME_ERROR) exit (EX_RUNTIME);
}


int main(int argc, const char *argv[]) {
    init_VM ();
    
    if (argc == 1) {
        repl ();
    } else if (argc == 2) {
        run_file (argv[1]);
    } else {
        fprintf (stderr, "Usage: clox [path\n]");
        exit(EX_USAGE);
    }
    
    free_VM ();
    return 0;
}