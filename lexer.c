#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

#define LEX_GETC_IF(buffer, c, exp)      \
    for (c = peekc(); exp; c = peekc()) \
    {                                   \
        buffer_write(buffer, c);        \
        nextc();                        \
    }

static struct lex_process *lex_process;
static struct token tmp_token;
struct token *read_next_token();


static char peekc()
{
    return lex_process->function->peek_char(lex_process);
}

static char nextc()
{
    char c = lex_process->function->next_char(lex_process);
    lex_process->pos.col += 1;
    if(c == '\n')
    {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

static void pushc(char c)
{
    lex_process->function->push_char(lex_process, c);
}

static struct pos lex_file_position()
{
    return lex_process->pos;
}

struct token* token_create(struct token* _token)
{
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    return &tmp_token;
}

static struct token* lexer_last_token()
{
    return vector_back_or_null(lex_process->token_vec);
}

static struct token *handle_whitespace()
{
    printf("handling last token\n");
    struct token *last_token = lexer_last_token();
    if (last_token)
    {
        last_token->whitespace = true;
        printf("last_token is whitespace\n");
    }
    nextc();
    return read_next_token();
}

const char* read_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

unsigned long long read_number()
{
    const char *s = read_number_str();
    printf("read number string : %s\n", s);
    return atoll(s);
}



struct token* token_make_number_for_value(unsigned long number)
{
    return token_create(&(struct token){.type = TOKEN_NUMBER, .llnum = number});
}

struct token* token_make_number()
{
    return token_make_number_for_value(read_number());
}

struct  token* read_next_token()
{
    struct token *token = NULL;
    char c = peekc();
    printf("Char read : %c, %x\n", c, c);
    switch (c)
    {
        NUMERIC_CASE:
            token = token_make_number();
            break;
        case ' ':
        case '\t':
            token = handle_whitespace();
            break;
        case EOF:
            printf("EOF encountered, all done!\n");
            /* Lexical analysis of file completed */
            break;
    
        default:
            compiler_error(lex_process->compiler, "Unexpected Token\n");
            //break;
    }
    return token;
}

int lex(struct lex_process* process)
{
    process->current_expresion_count = 0;
    process->parentheses_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token *token = read_next_token();
    while (token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }
    
    return LEXICAL_ANALYSIS_ALL_OK;
};