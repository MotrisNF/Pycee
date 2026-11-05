#include <string.h>
#include "front/lexer.h"

typedef struct s_op
{
    const char      *text;
    size_t          len;
    t_token_type    type;
}   t_op;

/*
 * Operators and punctuation, longest first: the first match is the
 * longest one (maximal munch). Adding an operator = one row in its
 * length group.
 */
static const t_op   g_ops[] = {
    {"//=", 3, TOK_SLASH_SLASH_EQ},
    {"<<=", 3, TOK_SHL_EQ},
    {">>=", 3, TOK_SHR_EQ},
    {"...", 3, TOK_ELLIPSIS},
    {"//",  2, TOK_SLASH_SLASH},
    {"/=",  2, TOK_SLASH_EQ},
    {"++",  2, TOK_PLUS_PLUS},
    {"+=",  2, TOK_PLUS_EQ},
    {"--",  2, TOK_MINUS_MINUS},
    {"-=",  2, TOK_MINUS_EQ},
    {"*=",  2, TOK_STAR_EQ},
    {"%=",  2, TOK_PERCENT_EQ},
    {"<=",  2, TOK_LE},
    {">=",  2, TOK_GE},
    {"<<",  2, TOK_SHL},
    {">>",  2, TOK_SHR},
    {"&=",  2, TOK_AMP_EQ},
    {"|=",  2, TOK_PIPE_EQ},
    {"^=",  2, TOK_CARET_EQ},
    {"=>",  2, TOK_ARROW},
    {"/",   1, TOK_SLASH},
    {"+",   1, TOK_PLUS},
    {"-",   1, TOK_MINUS},
    {"*",   1, TOK_STAR},
    {"%",   1, TOK_PERCENT},
    {"<",   1, TOK_LT},
    {">",   1, TOK_GT},
    {"=",   1, TOK_ASSIGN},
    {"(",   1, TOK_LPAREN},
    {")",   1, TOK_RPAREN},
    {"{",   1, TOK_LBRACE},
    {"}",   1, TOK_RBRACE},
    {"[",   1, TOK_LBRACKET},
    {"]",   1, TOK_RBRACKET},
    {",",   1, TOK_COMMA},
    {";",   1, TOK_SEMICOLON},
    {".",   1, TOK_DOT},
    {"?",   1, TOK_QUESTION},
    {":",   1, TOK_COLON},
    {"&",   1, TOK_AMP},
    {"|",   1, TOK_PIPE},
    {"^",   1, TOK_CARET},
    {"~",   1, TOK_TILDE},
    {"@",   1, TOK_AT},
};

/* Keywords. Adding one = one row here + one line in TOKEN_LIST. */
static const t_op   g_keywords[] = {
    {"func",     4, TOK_FUNC},
    {"prog",     4, TOK_PROG},
    {"obj",      3, TOK_OBJ},
    {"var",      3, TOK_VAR},
    {"vemp",     4, TOK_VEMP},
    {"import",   6, TOK_IMPORT},
    {"from",     4, TOK_FROM},
    {"as",       2, TOK_AS},
    {"if",       2, TOK_IF},
    {"elif",     4, TOK_ELIF},
    {"else",     4, TOK_ELSE},
    {"while",    5, TOK_WHILE},
    {"for",      3, TOK_FOR},
    {"in",       2, TOK_IN},
    {"switch",   6, TOK_SWITCH},
    {"case",     4, TOK_CASE},
    {"default",  7, TOK_DEFAULT},
    {"break",    5, TOK_BREAK},
    {"continue", 8, TOK_CONTINUE},
    {"return",   6, TOK_RETURN},
    {"and",      3, TOK_AND},
    {"or",       2, TOK_OR},
    {"xor",      3, TOK_XOR},
    {"not",      3, TOK_NOT},
    {"is",       2, TOK_IS},
    {"true",     4, TOK_TRUE},
    {"false",    5, TOK_FALSE},
    {"none",     4, TOK_NONE},
    {"self",     4, TOK_SELF},
    {"try",      3, TOK_TRY},
    {"except",   6, TOK_EXCEPT},
    {"finally",  7, TOK_FINALLY},
    {"raise",    5, TOK_RAISE},
    {"const",    5, TOK_CONST},
    {"extends",  7, TOK_EXTENDS},
    {"super",    5, TOK_SUPER},
    {"enum",     4, TOK_ENUM},
    {"private",  7, TOK_PRIVATE},
    {"static",   6, TOK_STATIC},
    {"assert",   6, TOK_ASSERT},
    {"do",       2, TOK_DO},
    {"match",    5, TOK_MATCH},
    {"async",    5, TOK_ASYNC},
    {"await",    5, TOK_AWAIT},
    {"export",   6, TOK_EXPORT},
    {"with",     4, TOK_WITH},
    {"man",      3, TOK_MAN},
};

static int  is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static int  is_ident_start(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static int  is_ident_char(char c)
{
    return (is_ident_start(c) || is_digit(c));
}

static void make_token(t_lexer *lx, t_token *tok, t_token_type type,
        size_t start)
{
    tok->type = type;
    tok->offset = lx->base + start;
    tok->len = lx->pos - start;
}

void    lexer_init(t_lexer *lx, const t_source *src)
{
    lx->src = src->text;
    lx->len = src->len;
    lx->base = src->base;
    lx->pos = 0;
}

// Skips a block comment (pos is at its opening "/*"). Comments do not nest.
static int  skip_block_comment(t_lexer *lx, t_error *err)
{
    size_t  start;

    start = lx->pos;
    lx->pos += 2;
    while (lx->pos + 1 < lx->len)
    {
        if (lx->src[lx->pos] == '*' && lx->src[lx->pos + 1] == '/')
        {
            lx->pos += 2;
            return (0);
        }
        lx->pos++;
    }
    lx->pos = lx->len;
    error_set(err, ERR_SYNTAX, lx->base + start, 2,
        "unterminated block comment");
    err->incomplete = 1;
    return (-1);
}

/* Skips whitespace, '#' line comments and block comments. */
static int  skip_trivia(t_lexer *lx, t_error *err)
{
    char    c;

    while (lx->pos < lx->len)
    {
        c = lx->src[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            lx->pos++;
        else if (c == '#')
        {
            while (lx->pos < lx->len && lx->src[lx->pos] != '\n')
                lx->pos++;
        }
        else if (c == '/' && lx->pos + 1 < lx->len
            && lx->src[lx->pos + 1] == '*')
        {
            if (skip_block_comment(lx, err) == -1)
                return (-1);
        }
        else
            return (0);
    }
    return (0);
}

/*
 * Returns 1 and fills tok if an operator starts at pos, 0 otherwise.
 * strncmp is safe near the end: src is '\0'-terminated, so a too-short
 * tail mismatches before reading past the buffer.
 */
static int  lex_operator(t_lexer *lx, t_token *tok)
{
    const char  *s;
    size_t      i;

    s = lx->src + lx->pos;
    for (i = 0; i < sizeof(g_ops) / sizeof(g_ops[0]); i++)
    {
        if (s[0] == g_ops[i].text[0]
            && strncmp(s, g_ops[i].text, g_ops[i].len) == 0)
        {
            tok->type = g_ops[i].type;
            tok->offset = lx->base + lx->pos;
            tok->len = g_ops[i].len;
            lx->pos += g_ops[i].len;
            return (1);
        }
    }
    return (0);
}

static int  is_base_digit(char c, int base)
{
    if (base == 16)
        return (is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
    return (c >= '0' && c < '0' + base);
}

/*
 * Skips the digits of a base; '_' is allowed only between two digits
 * ("1_000"). Returns how many digits there were.
 */
static int  scan_digits(t_lexer *lx, int base)
{
    int n;

    n = 0;
    while (1)
    {
        if (is_base_digit(lx->src[lx->pos], base))
            n++;
        else if (!(lx->src[lx->pos] == '_' && n > 0
                && is_base_digit(lx->src[lx->pos + 1], base)))
            return (n);
        lx->pos++;
    }
}

/* 0x, 0b or 0o: returns the base, or 0 if src[pos] starts no prefix. */
static int  base_prefix(const t_lexer *lx)
{
    char    c;

    if (lx->src[lx->pos] != '0')
        return (0);
    c = lx->src[lx->pos + 1];
    if (c == 'x' || c == 'X')
        return (16);
    if (c == 'b' || c == 'B')
        return (2);
    if (c == 'o' || c == 'O')
        return (8);
    return (0);
}

/* "e5", "E-3"... after a decimal number; nothing is consumed if it is not one. */
static int  scan_exponent(t_lexer *lx)
{
    size_t  digit;

    if (lx->src[lx->pos] != 'e' && lx->src[lx->pos] != 'E')
        return (0);
    digit = lx->pos + 1;
    if (lx->src[digit] == '+' || lx->src[digit] == '-')
        digit++;
    if (!is_digit(lx->src[digit]))
        return (0);
    lx->pos = digit;
    scan_digits(lx, 10);
    return (1);
}

/*
 * INT   = digits | "0x" hex | "0b" bin | "0o" oct, '_' between digits
 *         (and right after a prefix, as in Python: 0x_FF).
 * FLOAT = digits "." digits [ exponent ] | digits exponent.
 * The '.' only belongs to the number if a digit follows ("1.x" is 1 . x).
 * A letter or '_' right after the number ("3x", "1_") is an error, as in
 * Python. So are leading zeros ("007", "00.5"); "0", "0.5" are fine.
 * The value is computed by the parser (an int may not fit in 64 bits).
 */
static int  lex_number(t_lexer *lx, t_token *tok, t_error *err)
{
    size_t          start;
    t_token_type    type;
    int             base;

    start = lx->pos;
    type = TOK_INT;
    base = base_prefix(lx);
    if (base)
    {
        lx->pos += 2;
        if (lx->src[lx->pos] == '_' && is_base_digit(lx->src[lx->pos + 1], base))
            lx->pos++;
        if (scan_digits(lx, base) == 0)
            lx->pos = start + 1;
    }
    else
    {
        scan_digits(lx, 10);
        if (lx->src[lx->pos] == '.' && is_digit(lx->src[lx->pos + 1]))
        {
            lx->pos++;
            scan_digits(lx, 10);
            type = TOK_FLOAT;
        }
        if (scan_exponent(lx))
            type = TOK_FLOAT;
    }
    if (is_ident_char(lx->src[lx->pos]))
    {
        while (is_ident_char(lx->src[lx->pos]))
            lx->pos++;
        return (error_set(err, ERR_SYNTAX, lx->base + start, lx->pos - start,
                "invalid number literal"));
    }
    if (!base && lx->src[start] == '0'
        && (is_digit(lx->src[start + 1]) || lx->src[start + 1] == '_'))
        return (error_set(err, ERR_SYNTAX, lx->base + start, lx->pos - start,
                "leading zeros are not permitted"));
    make_token(lx, tok, type, start);
    return (0);
}

/* Identifier, or keyword if it is in g_keywords. */
static void lex_ident(t_lexer *lx, t_token *tok)
{
    size_t  start;
    size_t  len;
    size_t  i;

    start = lx->pos;
    while (is_ident_char(lx->src[lx->pos]))
        lx->pos++;
    len = lx->pos - start;
    make_token(lx, tok, TOK_IDENT, start);
    for (i = 0; i < sizeof(g_keywords) / sizeof(g_keywords[0]); i++)
    {
        if (g_keywords[i].len == len
            && memcmp(lx->src + start, g_keywords[i].text, len) == 0)
        {
            tok->type = g_keywords[i].type;
            return ;
        }
    }
}

static int  is_hex(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F'));
}

static int  hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    return (c - 'A' + 10);
}

/*
 * The escape at pos (its backslash): \n \t \r \0 \" \' \\, \xHH (a
 * byte below 0x80: a character) and \u{X...} (1 to 6 hex digits, a code
 * point that is not a surrogate). Returns its length, or -1.
 */
static int  check_escape(t_lexer *lx, size_t pos, t_error *err)
{
    const char  *s;
    long        cp;
    size_t      i;

    s = lx->src + pos;
    if (strchr("nrt0\"'\\", s[1]) && s[1])
        return (2);
    if (s[1] == 'x' && pos + 3 < lx->len && is_hex(s[2]) && is_hex(s[3]))
    {
        if (hex_value(s[2]) * 16 + hex_value(s[3]) < 0x80)
            return (4);
        return (error_set(err, ERR_SYNTAX, lx->base + pos, 4, "'\\x%c%c' is"
                " not a character: \\x goes up to \\x7F, use \\u{...} for"
                " the rest", s[2], s[3]));
    }
    if (s[1] == 'u' && s[2] == '{')
    {
        cp = 0;
        for (i = 3; i < 9 && pos + i < lx->len && is_hex(s[i]); i++)
            cp = cp * 16 + hex_value(s[i]);
        if (i > 3 && pos + i < lx->len && s[i] == '}' && cp <= 0x10FFFF
            && (cp < 0xD800 || cp > 0xDFFF))
            return ((int)i + 1);
        return (error_set(err, ERR_SYNTAX, lx->base + pos, 2, "invalid"
                " '\\u' escape: write \\u{XXXX} with 1 to 6 hex digits, up"
                " to 10FFFF and not D800-DFFF"));
    }
    if (s[1] == 'x')
        return (error_set(err, ERR_SYNTAX, lx->base + pos, 2, "invalid '\\x'"
                " escape: write \\xHH with two hex digits"));
    return (error_set(err, ERR_SYNTAX, lx->base + pos, 2, "invalid escape"
            " sequence '\\%c'", s[1]));
}

/*
 * A string from its opening quote: "..." or '...' (one line), or
 * """...""" / '''...''' (several lines). raw (after an r): no escapes,
 * a backslash is itself. An unterminated one-line string ends at the
 * end of its line; an unterminated multi-line one at the end of the
 * input (incomplete: the REPL asks for more lines).
 */
static int  lex_string(t_lexer *lx, t_token *tok, int raw, t_error *err)
{
    size_t  start;
    char    quote;
    int     triple;
    int     len;

    start = lx->pos - (size_t)raw;
    quote = lx->src[lx->pos];
    triple = (lx->pos + 2 < lx->len && lx->src[lx->pos + 1] == quote
            && lx->src[lx->pos + 2] == quote);
    lx->pos += triple ? 3 : 1;
    while (lx->pos < lx->len)
    {
        if (triple && lx->pos + 2 < lx->len && lx->src[lx->pos] == quote
            && lx->src[lx->pos + 1] == quote && lx->src[lx->pos + 2] == quote)
            break ;
        if (!triple && (lx->src[lx->pos] == quote || lx->src[lx->pos] == '\n'))
            break ;
        if (lx->src[lx->pos] == '\\' && !raw)
        {
            if (lx->pos + 1 >= lx->len || lx->src[lx->pos + 1] == '\n')
                break ;
            len = check_escape(lx, lx->pos, err);
            if (len == -1)
                return (-1);
            lx->pos += (size_t)len;
            continue ;
        }
        lx->pos++;
    }
    if (lx->pos >= lx->len || (!triple && lx->src[lx->pos] != quote))
    {
        error_set(err, ERR_SYNTAX, lx->base + start, triple ? 3 : 1,
            "unterminated string literal");
        err->incomplete = triple && lx->pos >= lx->len;
        return (-1);
    }
    lx->pos += triple ? 3 : 1;
    make_token(lx, tok, TOK_STRING, start);
    return (0);
}

static size_t   skip_fstring_body(const char *s, size_t len, size_t i,
                    char quote, int triple);

/*
 * Skips a string inside a field, from its opening quote (prefix already
 * passed): its end (after the closing quote), or (size_t)-1.
 */
static size_t   skip_inner_string(const char *s, size_t len, size_t i,
                    int is_f)
{
    char    quote;
    int     triple;

    quote = s[i];
    triple = (i + 2 < len && s[i + 1] == quote && s[i + 2] == quote);
    i += triple ? 3 : 1;
    if (is_f)
        return (skip_fstring_body(s, len, i, quote, triple));
    while (i < len)
    {
        if (s[i] == '\\' && i + 1 < len)
            i += 2;
        else if (triple && i + 2 < len && s[i] == quote && s[i + 1] == quote
            && s[i + 2] == quote)
            return (i + 3);
        else if (!triple && s[i] == quote)
            return (i + 1);
        else if (!triple && s[i] == '\n')
            return ((size_t)-1);
        else
            i++;
    }
    return ((size_t)-1);
}

size_t  lex_fstring_field(const char *s, size_t len, size_t start,
            size_t *colon)
{
    size_t  i;
    int     depth;
    int     pending;

    depth = 0;
    pending = 0;
    *colon = (size_t)-1;
    i = start;
    while (i < len)
    {
        if ((s[i] == '"' || s[i] == '\'') || ((s[i] == 'f' || s[i] == 'r')
                && i + 1 < len && (s[i + 1] == '"' || s[i + 1] == '\'')
                && (i == start || !is_ident_char(s[i - 1]))))
        {
            i = skip_inner_string(s, len, i + (s[i] == 'f' || s[i] == 'r'),
                    s[i] == 'f');
            if (i == (size_t)-1)
                return (i);
            continue ;
        }
        if (s[i] == '(' || s[i] == '[' || s[i] == '{')
            depth++;
        else if ((s[i] == ')' || s[i] == ']' || s[i] == '}') && depth > 0)
            depth--;
        else if (s[i] == '}' && *colon == (size_t)-1)
        {
            *colon = i;
            return (i);
        }
        else if (s[i] == '}')
            return (i);
        else if (s[i] == '?' && depth == 0 && *colon == (size_t)-1)
            pending++;
        else if (s[i] == ':' && depth == 0 && *colon == (size_t)-1)
        {
            if (pending > 0)
                pending--;
            else
                *colon = i;
        }
        i++;
    }
    return ((size_t)-1);
}

/*
 * The text of an f-string after its opening quote(s): literal text
 * (escapes as in any string, {{ and }} for braces) and {fields}. Its
 * end, after the closing quote(s), or (size_t)-1 (unterminated, or a
 * '}' alone: the caller says which).
 */
static size_t   skip_fstring_body(const char *s, size_t len, size_t i,
                    char quote, int triple)
{
    size_t  colon;

    while (i < len)
    {
        if (triple && i + 2 < len && s[i] == quote && s[i + 1] == quote
            && s[i + 2] == quote)
            return (i + 3);
        if (!triple && s[i] == quote)
            return (i + 1);
        if ((!triple && s[i] == '\n') || (s[i] == '\\' && i + 1 >= len))
            return ((size_t)-1);
        if (s[i] == '\\')
            i += 2;
        else if ((s[i] == '{' || s[i] == '}') && i + 1 < len
            && s[i + 1] == s[i])
            i += 2;
        else if (s[i] == '{')
        {
            i = lex_fstring_field(s, len, i + 1, &colon);
            if (i == (size_t)-1)
                return (i);
            i++;
        }
        else if (s[i] == '}')
            return ((size_t)-1);
        else
            i++;
    }
    return ((size_t)-1);
}

/*
 * f"..." (f already passed, at the quote). The fields are only found
 * here; the parser reads what is inside them. The escapes of the
 * literal text are checked as in any string.
 */
static int  lex_fstring(t_lexer *lx, t_token *tok, t_error *err)
{
    size_t  start;
    size_t  i;
    size_t  end;
    char    quote;
    int     triple;
    int     len;

    start = lx->pos - 1;
    quote = lx->src[lx->pos];
    triple = (lx->pos + 2 < lx->len && lx->src[lx->pos + 1] == quote
            && lx->src[lx->pos + 2] == quote);
    end = skip_fstring_body(lx->src, lx->len, lx->pos + (triple ? 3 : 1),
            quote, triple);
    if (end == (size_t)-1)
    {
        error_set(err, ERR_SYNTAX, lx->base + start, triple ? 4 : 2,
            "unterminated f-string (or a '}' alone: write '}}' for a"
            " brace)");
        err->incomplete = triple;
        return (-1);
    }
    for (i = lx->pos + 1; i < end; i++)
    {
        if (lx->src[i] == '\\')
        {
            len = check_escape(lx, i, err);
            if (len == -1)
                return (-1);
            i += (size_t)len - 1;
        }
    }
    lx->pos = end;
    make_token(lx, tok, TOK_FSTRING, start);
    return (0);
}

static int  invalid_char(t_lexer *lx, t_error *err)
{
    unsigned char   c;

    c = (unsigned char)lx->src[lx->pos];
    if (c >= 0x20 && c < 0x7f)
        return (error_set(err, ERR_SYNTAX, lx->base + lx->pos, 1,
                "invalid character '%c'", c));
    return (error_set(err, ERR_SYNTAX, lx->base + lx->pos, 1,
            "invalid character '\\x%02x'", c));
}

int     lexer_next(t_lexer *lx, t_token *tok, t_error *err)
{
    char    c;

    if (skip_trivia(lx, err) == -1)
        return (-1);
    if (lx->pos >= lx->len)
    {
        tok->type = TOK_EOF;
        tok->offset = lx->base + lx->len;
        tok->len = 0;
        return (0);
    }
    c = lx->src[lx->pos];
    if (is_digit(c))
        return (lex_number(lx, tok, err));
    if (c == 'f' && lx->pos + 1 < lx->len
        && (lx->src[lx->pos + 1] == '"' || lx->src[lx->pos + 1] == '\''))
    {
        lx->pos++;
        return (lex_fstring(lx, tok, err));
    }
    if (c == 'r' && lx->pos + 1 < lx->len
        && (lx->src[lx->pos + 1] == '"' || lx->src[lx->pos + 1] == '\''))
    {
        lx->pos++;
        return (lex_string(lx, tok, 1, err));
    }
    if (is_ident_start(c))
    {
        lex_ident(lx, tok);
        return (0);
    }
    if (c == '"' || c == '\'')
        return (lex_string(lx, tok, 0, err));
    if (lex_operator(lx, tok))
        return (0);
    return (invalid_char(lx, err));
}
