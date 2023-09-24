#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main()
{

    int res = compile_file("./test.c", "./test", 0);
    if(res == COMPILER_FILE_COMPILED_OK)
    {
        printf("file compiled with no issues\n");
    }
    else if (res == COMPILER_FAILED_WITH_ERRORS)
    {
        printf("file failed to compile\n");
    }
    else
    {
        printf("Unknown compile error\n");
    }

    return 0;
}