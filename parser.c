#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

static struct compile_process *current_process;
static struct token *parser_last_token;
static struct token *token_peek_next();
void parse_single_token_to_node();
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

struct history
{
    int flags;
};

struct history *history_begin(int flags)
{
    struct history *history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}

struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

void parse_identifier(struct history *history)
{
    assert(token_peek_next()->type == NODE_IDENTIFIER);
    parse_single_token_to_node();
}

int parse_expressionable_single(struct history *history);
void parse_expressionable(struct history *history);

static void parser_ignore_nl_or_comment(struct token *token)
{
    while (token && token_is_nl_or_comment_or_newline_separator(token))
    {
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

static bool token_next_is_operator(const char* op)
{
    struct token *token = token_peek_next();
    return token_is_operator(token, op);
}

void parse_single_token_to_node()
{
    struct token *token = token_next();
    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_NUMBER:
        node = node_create(&(struct node){.type = NODE_NUMBER, .llnum = token->llnum});
        break;
    case TOKEN_IDENTIFIER:
        node = node_create(&(struct node){.type = NODE_IDENTIFIER, .sval = token->sval});
        break;
    case TOKEN_STRING:
        node = node_create(&(struct node){.type = NODE_STRING, .sval = token->sval});
        break;
    default:
        compiler_error(current_process, "Not a single token that can be converted to a node");
        // break;
    }
}

void parse_expressionable_for_op(struct history *history, const char *op)
{
    // printf("parse_expressionalbe_for_op op: %s not used\n", op);
    parse_expressionable(history);
}

static int parser_get_precedence_for_operator(const char *op, struct expressionable_op_precedence_group **group_out)
{
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++)
    {
        for (int b = 0; op_precedence[i].operators[b]; b++)
        {
            const char *_op = op_precedence[i].operators[b];
            if (S_EQ(_op, op))
            {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

static bool parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    struct expressionable_op_precedence_group *group_left = NULL;
    struct expressionable_op_precedence_group *group_right = NULL;
    if (S_EQ(op_left, op_right))
    {
        return false;
    }
    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if (group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        return false;
    }
    return precedence_left <= precedence_right;
}

void parser_node_shift_children_left(struct node *node)
{
    assert(node->type == NODE_EXPRESSION);
    assert(node->exp.right->type == NODE_EXPRESSION);

    const char *right_op = node->exp.right->exp.op;
    struct node *new_exp_left_node = node->exp.left;
    struct node *new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    struct node *new_left_operand = node_pop();
    struct node *new_right_operand = node->exp.right->exp.right;
    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}

void parser_reorder_expression(struct node **node_out)
{
    struct node *node = *node_out;
    if (node->type != NODE_EXPRESSION)
    {
        return;
    }
    // No adjacent nodes are expressions
    if (node->exp.left->type != NODE_EXPRESSION && node->exp.right && node->exp.right->type != NODE_EXPRESSION)
    {
        return;
    }
    // Right adjacent node is an expression, may need to reorder
    if (node->exp.left->type != NODE_EXPRESSION && node->exp.right && node->exp.right->type == NODE_EXPRESSION)
    {
        const char *right_op = node->exp.right->exp.op;
        if (parser_left_op_has_priority(node->exp.op, right_op))
        {
            parser_node_shift_children_left(node);
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

void parse_exp_normal(struct history *history)
{
    struct token *op_token = token_peek_next();
    const char *op = op_token->sval;
    // printf("parse_exp_normal: op = %s\n", op);
    struct node *node_left = node_peek_expressionable_or_null();
    if (!node_left)
    {
        return;
    }
    token_next();
    node_pop();
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node *node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    make_exp_node(node_left, node_right, op);
    struct node *exp_node = node_pop();

    // Re-oder the expression according to precedence
    parser_reorder_expression(&exp_node);
    node_push(exp_node);
}

int parse_exp(struct history *history)
{
    parse_exp_normal(history);
    return 0;
}

static bool is_keyword_variable_modifier(const char *val)
{
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const") ||
           S_EQ(val, "extern") ||
           S_EQ(val, "__ignore_typecheck__");
}

void parse_datatype_modifiers(struct datatype *dtype)
{
    struct token *token = token_peek_next();
    while (token && token->type == TOKEN_KEYWORD)
    {
        if (!is_keyword_variable_modifier(token->sval))
        {
            break;
        }
        if (S_EQ(token->sval, "signed"))
        {
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
        }
        else if (S_EQ(token->sval, "unsigned"))
        {
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
        }
        else if (S_EQ(token->sval, "static"))
        {
            dtype->flags |= DATATYPE_FLAG_IS_STATIC;
        }
        else if (S_EQ(token->sval, "const"))
        {
            dtype->flags |= DATATYPE_FLAG_IS_CONST;
        }
        else if (S_EQ(token->sval, "extern"))
        {
            dtype->flags |= DATATYPE_FLAG_IS_EXTERN;
        }
        else if (S_EQ(token->sval, "__ignore_typecheck__"))
        {
            dtype->flags |= DATATYPE_FLAG_IGNORE_TYPE_CHECKING;
        }
        token_next();
        token = token_peek_next();
    }
}

void parser_get_datatype_tokens(struct token **datatype_token, struct token **datatype_secondary_token)
{
    *datatype_token = token_next();
    struct token *next_token = token_peek_next();
    if(token_is_primitive_keyword(next_token))
    {
        *datatype_secondary_token = next_token;
        token_next();
    }
}

int parser_get_datatype_expected_for_type_string(const char* str)
{
    int type = DATA_TYPE_EXPECT_PRIMITIVE;
    if(S_EQ(str, "union"))
    {
        type = DATA_TYPE_EXPECT_UNION;
    }
    else if(S_EQ(str, "struct"))
    {
        type = DATA_TYPE_EXPECT_STRUCT;
    }
    return type;
}

int parser_get_random_type_index()
{
    static int x = 0;
    x++;
    return x;
}

struct token *parser_build_random_type_name()
{
    char tmp_name[25];
    sprintf(tmp_name, "customtypename_%i", parser_get_random_type_index());
    char *sval = malloc(sizeof(tmp_name));
    strncpy(sval, tmp_name, sizeof(tmp_name));
    struct token *token = calloc(1, sizeof(struct token));
    token->type = TOKEN_IDENTIFIER;
    token->sval = sval;
    return token;
}

int parser_get_pointer_depth()
{
    int depth = 0;
    while(token_next_is_operator("*"))
    {
        depth++;
        token_next();
    }
    return depth;
}

bool parser_datatype_is_secondary_allowed(int expected_type)
{
    return expected_type == DATA_TYPE_EXPECT_PRIMITIVE;
}

bool parser_datatype_is_secondary_allowed_for_type(const char *type)
{
    return S_EQ(type, "long") || S_EQ(type, "short") || S_EQ(type, "double") || S_EQ(type, "float");
}
void parser_datatype_init_type_and_size_for_primitive(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out);
void parser_datatype_adjust_size_for_secondary(struct datatype *datatype, struct token *datatype_secondary_token)
{
    if (!datatype_secondary_token)
    {
        return;
    }

    struct datatype *secondary_data_type = calloc(1, sizeof(struct datatype));
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, NULL, secondary_data_type);
    datatype->size += secondary_data_type->size;
    datatype->secondary = secondary_data_type;
    datatype->flags |= DATATYPE_FLAG_IS_SECONDARY;
}

void parser_datatype_init_type_and_size_for_primitive(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out)
{
    if (!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval) && datatype_secondary_token)
    {
        compiler_error(current_process, "Your not allowed a secondary datatype here for the given datatype %s\n", datatype_token->sval);
    }

    if (S_EQ(datatype_token->sval, "void"))
    {
        datatype_out->type = DATA_TYPE_VOID;
        datatype_out->size = DATA_SIZE_ZERO;
    }
    else if (S_EQ(datatype_token->sval, "char"))
    {
        datatype_out->type = DATA_TYPE_CHAR;
        datatype_out->size = DATA_SIZE_BYTE;
    }
    else if (S_EQ(datatype_token->sval, "short"))
    {
        datatype_out->type = DATA_TYPE_SHORT;
        datatype_out->size = DATA_SIZE_WORD;
    }
    else if (S_EQ(datatype_token->sval, "int"))
    {
        datatype_out->type = DATA_TYPE_INTEGER;
        datatype_out->size = DATA_SIZE_DWORD;
    }
    else if (S_EQ(datatype_token->sval, "long"))
    {
        datatype_out->type = DATA_TYPE_LONG;
        // change to DDWORD for 64 bit compiler
        datatype_out->size = DATA_SIZE_DWORD;
    }
    else if (S_EQ(datatype_token->sval, "float"))
    {
        datatype_out->type = DATA_TYPE_FLOAT;
        datatype_out->size = DATA_SIZE_DWORD;
    }
    else if (S_EQ(datatype_token->sval, "double"))
    {
        datatype_out->size = DATA_TYPE_DOUBLE;
        datatype_out->size = DATA_SIZE_DWORD;
    }
    else
    {
        compiler_error(current_process, "Invalid primitive datatype\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

void parser_datatype_init_type_and_size(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out, int pointer_depth, int expected_type)
{
    if (!parser_datatype_is_secondary_allowed(expected_type) && datatype_secondary_token)
    {
        compiler_error(current_process, "Invalid secondary datatype\n");
    }

    switch (expected_type)
    {
    case DATA_TYPE_EXPECT_PRIMITIVE:
        parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
        break;

    case DATA_TYPE_EXPECT_STRUCT:
    case DATA_TYPE_EXPECT_UNION:
        compiler_error(current_process, "Structure and union types are currently unsupported\n");
        break;

    default:
        compiler_error(current_process, "Unsupported datatype expectation\n");
    }
}

void parser_datatype_init(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out, int pointer_depth, int expected_type)
{
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;

    if (S_EQ(datatype_token->sval, "long") && datatype_secondary_token && S_EQ(datatype_secondary_token->sval, "long"))
    {
        compiler_warning(current_process, "Compiler does not support 64 bit longs: long long is defaulting to 32 bits\n");
        datatype_out->size = DATA_SIZE_DWORD;
    }
}

void parse_datatype_type(struct datatype *dtype)
{
    struct token *datatype_token = NULL;
    struct token *datatype_secondary_token = NULL;
    parser_get_datatype_tokens(&datatype_token, &datatype_secondary_token);
    int expected_type = parser_get_datatype_expected_for_type_string(datatype_token->sval);
    if (datatype_is_struct_or_union_for_name(datatype_token->sval))
    {
        // handle "struct abc", for declaring a struct type
        if(token_peek_next()->type == TOKEN_IDENTIFIER)
        {
            datatype_token = token_next();
        }
        // handle the case: "struct {}", for defining a struct
        else
        {
            datatype_token = parser_build_random_type_name();
            dtype->flags = DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
        }
    }
    int pointer_depth = parser_get_pointer_depth();
    parser_datatype_init(datatype_token, datatype_secondary_token, dtype, pointer_depth, expected_type);
}

void parse_datatype(struct datatype *dtype)
{
    memset(dtype, 0, sizeof(struct datatype));
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED;

    parse_datatype_modifiers(dtype);
    parse_datatype_type(dtype);
    parse_datatype_modifiers(dtype);
}
void parse_variable_function_or_struct_union(struct history *history)
{
    struct datatype dtype;
    parse_datatype(&dtype);
}

void parse_keyword(struct history *history)
{
    struct token *token = token_peek_next();
    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval))
    {
        parse_variable_function_or_struct_union(history);
        return;
    }
}

int parse_expressionable_single(struct history *history)
{
    struct token *token = token_peek_next();
    if (!token)
    {
        return -1;
    }
    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;
    switch (token->type)
    {
    case TOKEN_NUMBER:
        // printf("parse_expresionable_single: NUMBER token parsed\n");
        parse_single_token_to_node();
        res = 0;
        break;
    case TOKEN_IDENTIFIER:
        parse_identifier(history);
        break;
    case TOKEN_OPERATOR:
        // printf("parse_expresionable_single: OPERATOR token parsed\n");
        parse_exp(history);
        res = 0;
        break;
    case TOKEN_KEYWORD:
        parse_keyword(history);
        res = 0;
        break;
    }
    return res;
}

void parse_expressionable(struct history *history)
{
    while (parse_expressionable_single(history) == 0)
    {
    }
}

void parse_keyword_for_global()
{
    parse_keyword(history_begin(0));
    struct node *node = node_pop();
}
int parse_next()
{
    struct token *token = token_peek_next();
    if (!token)
    {
        return -1;
    }
    int res = 0;
    switch (token->type)
    {
    case TOKEN_NUMBER:
    case TOKEN_IDENTIFIER:
    case TOKEN_STRING:
        parse_expressionable(history_begin(0));
        break;
        case TOKEN_KEYWORD:
            parse_keyword_for_global();
            break;
        }
    return 0;
}

int parse(struct compile_process *process)
{
    current_process = process;
    parser_last_token = NULL;
    node_set_vector(process->node_vec, process->node_tree_vec);
    struct node *node = NULL;
    vector_set_peek_pointer(process->token_vec, 0);
    while (parse_next() == 0)
    {
        node = node_peek();
        vector_push(process->node_tree_vec, &node);
    }
    return PARSE_ALL_OK;
}