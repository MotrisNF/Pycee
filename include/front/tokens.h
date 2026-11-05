#ifndef TOKENS_H
# define TOKENS_H

#include <stddef.h>

/*
 * Single source of truth for token types (see "Token list" in docs/EBNF.md).
 * X(enum name, human-readable description used in error messages).
 * Adding a token = adding one line here.
 */
# define TOKEN_LIST(X) \
    /* Literals and special */ \
    X(TOK_IDENT,            "identifier") \
    X(TOK_INT,              "integer") \
    X(TOK_FLOAT,            "float") \
    X(TOK_STRING,           "string") \
    X(TOK_FSTRING,          "f-string") \
    X(TOK_EOF,              "end of file") \
    /* Keywords: declarations */ \
    X(TOK_FUNC,             "'func'") \
    X(TOK_PROG,             "'prog'") \
    X(TOK_OBJ,              "'obj'") \
    X(TOK_VAR,              "'var'") \
    X(TOK_VEMP,             "'vemp'") \
    X(TOK_CONST,            "'const'") \
    /* Keywords: modules */ \
    X(TOK_IMPORT,           "'import'") \
    X(TOK_FROM,             "'from'") \
    X(TOK_AS,               "'as'") \
    /* Keywords: control flow */ \
    X(TOK_IF,               "'if'") \
    X(TOK_ELIF,             "'elif'") \
    X(TOK_ELSE,             "'else'") \
    X(TOK_WHILE,            "'while'") \
    X(TOK_FOR,              "'for'") \
    X(TOK_IN,               "'in'") \
    X(TOK_SWITCH,           "'switch'") \
    X(TOK_CASE,             "'case'") \
    X(TOK_DEFAULT,          "'default'") \
    X(TOK_BREAK,            "'break'") \
    X(TOK_CONTINUE,         "'continue'") \
    X(TOK_RETURN,           "'return'") \
    X(TOK_DO,               "'do'") \
    X(TOK_TRY,              "'try'") \
    X(TOK_EXCEPT,           "'except'") \
    X(TOK_FINALLY,          "'finally'") \
    X(TOK_RAISE,            "'raise'") \
    X(TOK_ASSERT,           "'assert'") \
    /* Keywords: logic and comparison */ \
    X(TOK_AND,              "'and'") \
    X(TOK_OR,               "'or'") \
    X(TOK_XOR,              "'xor'") \
    X(TOK_NOT,              "'not'") \
    X(TOK_IS,               "'is'") \
    /* Keywords: literal values */ \
    X(TOK_TRUE,             "'true'") \
    X(TOK_FALSE,            "'false'") \
    X(TOK_NONE,             "'none'") \
    X(TOK_SELF,             "'self'") \
    /*
     * Reserved words: rejected as names, no meaning yet (see "Planned
     * syntax" in docs/EBNF.md). When one gets implemented its description
     * loses the "reserved word" prefix.
     */ \
    X(TOK_EXTENDS,          "'extends'") \
    X(TOK_SUPER,            "'super'") \
    X(TOK_ENUM,             "reserved word 'enum'") \
    X(TOK_PRIVATE,          "'private'") \
    X(TOK_STATIC,           "'static'") \
    X(TOK_MATCH,            "reserved word 'match'") \
    X(TOK_ASYNC,            "reserved word 'async'") \
    X(TOK_AWAIT,            "reserved word 'await'") \
    X(TOK_EXPORT,           "reserved word 'export'") \
    X(TOK_WITH,             "reserved word 'with'") \
    X(TOK_MAN,              "reserved word 'man'") \
    /* Punctuation */ \
    X(TOK_LPAREN,           "'('") \
    X(TOK_RPAREN,           "')'") \
    X(TOK_LBRACE,           "'{'") \
    X(TOK_RBRACE,           "'}'") \
    X(TOK_LBRACKET,         "'['") \
    X(TOK_RBRACKET,         "']'") \
    X(TOK_COMMA,            "','") \
    X(TOK_SEMICOLON,        "';'") \
    X(TOK_DOT,              "'.'") \
    X(TOK_QUESTION,         "'?'") \
    X(TOK_COLON,            "':'") \
    X(TOK_ARROW,            "'=>'") \
    X(TOK_ELLIPSIS,         "'...'") \
    X(TOK_AT,               "reserved symbol '@'") \
    /* Operators: arithmetic */ \
    X(TOK_PLUS,             "'+'") \
    X(TOK_MINUS,            "'-'") \
    X(TOK_STAR,             "'*'") \
    X(TOK_SLASH,            "'/'") \
    X(TOK_SLASH_SLASH,      "'//'") \
    X(TOK_PERCENT,          "'%'") \
    X(TOK_PLUS_PLUS,        "'++'") \
    X(TOK_MINUS_MINUS,      "'--'") \
    /* Operators: bits (integers only) */ \
    X(TOK_AMP,              "'&'") \
    X(TOK_PIPE,             "'|'") \
    X(TOK_CARET,            "'^'") \
    X(TOK_TILDE,            "'~'") \
    X(TOK_SHL,              "'<<'") \
    X(TOK_SHR,              "'>>'") \
    /* Operators: comparison */ \
    X(TOK_LT,               "'<'") \
    X(TOK_LE,               "'<='") \
    X(TOK_GT,               "'>'") \
    X(TOK_GE,               "'>='") \
    /* Operators: assignment */ \
    X(TOK_ASSIGN,           "'='") \
    X(TOK_PLUS_EQ,          "'+='") \
    X(TOK_MINUS_EQ,         "'-='") \
    X(TOK_STAR_EQ,          "'*='") \
    X(TOK_SLASH_EQ,         "'/='") \
    X(TOK_SLASH_SLASH_EQ,   "'//='") \
    X(TOK_PERCENT_EQ,       "'%='") \
    X(TOK_AMP_EQ,           "'&='") \
    X(TOK_PIPE_EQ,          "'|='") \
    X(TOK_CARET_EQ,         "'^='") \
    X(TOK_SHL_EQ,           "'<<='") \
    X(TOK_SHR_EQ,           "'>>='")

# define TOKEN_AS_ENUM(name, desc) name,

typedef enum e_token_type
{
    TOKEN_LIST(TOKEN_AS_ENUM)
    TOK_TYPE_COUNT
}   t_token_type;

/* The lexeme is not copied: it is src[offset .. offset + len). */
typedef struct s_token
{
    t_token_type    type;
    size_t          offset;
    size_t          len;
}   t_token;

/* "TOK_SEMICOLON", for --tokens. */
const char  *token_type_name(t_token_type type);
/* "';'", for messages such as "expected ';'". */
const char  *token_type_desc(t_token_type type);

#endif
