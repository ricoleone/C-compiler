#include "compiler.h"

int compile_file(const char* filename, const char* filename_out, int flags)
{
    struct compile_process *process = compile_process_create(filename, filename_out, flags);
    if(!process)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }
    
    // Lexical Analysis of filename
    
    // Parse
    
    // Generate code

    return COMPILER_FILE_COMPILED_OK;
}