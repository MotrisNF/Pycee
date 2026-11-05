#include "front/tokens.h"

#define TOKEN_AS_NAME(name, desc) [name] = #name,
#define TOKEN_AS_DESC(name, desc) [name] = desc,

static const char   *g_token_names[TOK_TYPE_COUNT] = {
    TOKEN_LIST(TOKEN_AS_NAME)
};

static const char   *g_token_descs[TOK_TYPE_COUNT] = {
    TOKEN_LIST(TOKEN_AS_DESC)
};

const char  *token_type_name(t_token_type type)
{
    return (g_token_names[type]);
}

const char  *token_type_desc(t_token_type type)
{
    return (g_token_descs[type]);
}
