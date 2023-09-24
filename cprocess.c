#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
struct compile_process *compile_process_create(const char *filename, const char *filename_out, int flags)
{
    FILE *file_in = fopen(filename, "r");
    if (!file_in)
    {
        return NULL;
    }
    FILE *file_out = NULL;
    if (filename_out)
    {
        file_out = fopen(filename_out, "w");
        if (!file_out)
        {
            return NULL;
        }
    }

    struct compile_process* process = calloc(1, sizeof(struct compile_process));
    process->flags    = flags;
    process->cfile.fp = file_in;
    process->ofile    = file_out;
    return process;
};
