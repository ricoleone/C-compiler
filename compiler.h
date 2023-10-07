#ifndef LE1_COPILER_H
#define LE1_COPILER_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define S_EQ(str1, str2) (str1 && str2 && (strcmp(str1, str2) == 0))

#define NUMERIC_CASE \
    case '0':        \
    case '1':        \
    case '2':        \
    case '3':        \
    case '4':        \
    case '5':        \
    case '6':        \
    case '7':        \
    case '8':        \
    case '9'

#define OPERATOR_CASE_EXCLUDING_DIVISION \
    case '+':                            \
    case '-':                            \
    case '*':                            \
    case '>':                            \
    case '<':                            \
    case '^':                            \
    case '%':                            \
    case '!':                            \
    case '=':                            \
    case '~':                            \
    case '|':                            \
    case '&':                            \
    case '(':                            \
    case '[':                            \
    case ',':                            \
    case '.':                            \
    case '?'

#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case ':':       \
    case ';':       \
    case '#':       \
    case '\\':      \
    case ')':       \
    case ']'

struct pos
{
    int line;
    int col;
    const char *filename;
};

enum
{
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANALYSIS_INPUT_ERROR
};

enum
{
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD,
    TOKEN_OPERATOR,
    TOKEN_SYMBOL,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_NEWLINE
};

enum
{
    NUMBER_NORMAL,
    NUMBER_LONG,
    NUMBER_FLOAT,
    NUMBER_DOUBLE
};

struct token
{
    int type;
    int flags;
    struct pos pos;
    union
    {
        char cval;
        const char *sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void *any;
    };

    struct token_number
    {
        int type;
    } num;

    // True is there is whitespace between tokens
    bool whitespace;
    const char *between_brakets;
};

struct lex_process;
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lex_process *process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lex_process *process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lex_process *process, char c);

struct lex_process_functions
{
    LEX_PROCESS_NEXT_CHAR next_char;
    LEX_PROCESS_PEEK_CHAR peek_char;
    LEX_PROCESS_PUSH_CHAR push_char;
};

struct lex_process
{
    struct pos pos;
    struct vector *token_vec;
    struct compile_process *compiler;

    // how many nested brakets
    int current_expresion_count;
    struct buffer *parentheses_buffer;
    struct lex_process_functions *function;
    // holds private data
    void *private;
};
enum
{
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS
};

struct compile_process
{
    int flags; // Compilation flags
    struct pos pos;
    struct compile_process_input_file
    {
        FILE *fp;
        const char *abs_path;
    } cfile;

    struct vector *token_vec;
    struct vector *node_vec;
    struct vector *node_tree_vec;
    FILE *ofile;
};

enum
{
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR
};

enum
{
    NODE_EXPRESSION,
    NODE_EXPRESSION_PARENTHESES,
    NODE_NUMBER,
    NODE_IDENTIFIER,
    NODE_STRING,
    NODE_VARIABLE,
    NODE_VARIABLE_LIST,
    NODE_FUNCTION,
    NODE_BODY,
    NODE_STATEMENT_RETURN,
    NODE_STATEMENT_IF,
    NODE_STATEMENT_ELSE,
    NODE_STATEMENT_WHILE,
    NODE_STATEMENT_DO_WHILE,
    NODE_STATEMENT_FOR,
    NODE_STATEMENT_BREAK,
    NODE_STATEMENT_CONTINUE,
    NODE_STATEMENT_SWITCH,
    NODE_STATEMENT_CASE,
    NODE_STATEMENT_DEFAULT,
    NODE_STATEMENT_GOTO,
    NODE_UNARY,
    NODE_TENARY,
    NODE_LABEL,
    NODE_STRUCT,
    NODE_UNION,
    NODE_BRACKET,
    NODE_CAST,
    NODE_BLANK
};

struct node
{
    int type;
    int flags;

    struct pos pos;

    struct node_binded
    {
        // Pointer to the body node
        struct node *owner;

        // Pointer to the function containing this node (scope)
        struct node *function;
    } binded;

    union
    {
        char cval;
        const char *sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
    };
};

int compile_file(const char *filename, const char *out_file, int flags);
struct compile_process *compile_process_create(const char *filename, const char *filename_out, int flags);

char compile_process_next_char(struct lex_process *lex_process);
char compile_process_peek_char(struct lex_process *lex_process);
void compile_process_push_char(struct lex_process *lex_process, char c);

void compiler_error(struct compile_process *compiler, const char *msg, ...);
void compiler_warning(struct compile_process *compiler, const char *msg, ...);

struct lex_process *lex_process_create(struct compile_process *compiler, struct lex_process_functions *functions, void *private);
void lex_process_free(struct lex_process *process);
void *lex_process_private(struct lex_process *process);
struct vector *lex_process_tokens(struct lex_process *process);

int lex(struct lex_process *process);

/*
 * Builds tokens for input strings
 */
struct lex_process *tokens_build_for_string(struct compile_process *compiler, const char *str);
int parse(struct compile_process *process);
bool token_is_keyword(struct token *token, const char *value);
bool token_is_nl_or_newline_or_comment_or_separator(struct token *token);
bool token_is_symbol(struct token *token, char c);

struct node *node_create(struct node *_node);
void node_set_vector(struct vector *vec, struct vector *root_vec);
void node_push(struct node *node);
struct node *node_peek_or_null();
struct node *node_peek();
struct node *node_pop();

#endif
