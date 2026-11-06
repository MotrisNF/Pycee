#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/stack.h"
#include "core/utf8.h"
#include "front/parser.h"

/*
 * Binding power of binary operators, lowest to highest (see docs/EBNF.md).
 * 0 means "not a binary operator". Adding one = one row here.
 */
enum e_prec
{
    PREC_NONE,
    PREC_TERNARY,       /* c ? a : b (right-associative) */
    PREC_OR,            /* or xor */
    PREC_AND,           /* and */
    PREC_NOT,           /* not (prefix) */
    PREC_COMPARISON,    /* is, is not, in, not in, < <= > >= (no chains) */
    PREC_BIT_OR,        /* | */
    PREC_BIT_XOR,       /* ^ */
    PREC_BIT_AND,       /* & */
    PREC_SHIFT,         /* << >> */
    PREC_SUM,           /* + - */
    PREC_PRODUCT,       /* * / // % */
    PREC_PREFIX,        /* - ~ ++ -- (prefix) */
};

static const int    g_binary_prec[TOK_TYPE_COUNT] = {
    [TOK_OR] = PREC_OR,
    [TOK_XOR] = PREC_OR,
    [TOK_AND] = PREC_AND,
    [TOK_IS] = PREC_COMPARISON,
    [TOK_IN] = PREC_COMPARISON,
    [TOK_LT] = PREC_COMPARISON,
    [TOK_LE] = PREC_COMPARISON,
    [TOK_GT] = PREC_COMPARISON,
    [TOK_GE] = PREC_COMPARISON,
    [TOK_PIPE] = PREC_BIT_OR,
    [TOK_CARET] = PREC_BIT_XOR,
    [TOK_AMP] = PREC_BIT_AND,
    [TOK_SHL] = PREC_SHIFT,
    [TOK_SHR] = PREC_SHIFT,
    [TOK_PLUS] = PREC_SUM,
    [TOK_MINUS] = PREC_SUM,
    [TOK_STAR] = PREC_PRODUCT,
    [TOK_SLASH] = PREC_PRODUCT,
    [TOK_SLASH_SLASH] = PREC_PRODUCT,
    [TOK_PERCENT] = PREC_PRODUCT,
};

static t_node   *parse_expr(t_parser *p, int min_prec);
static t_node   *parse_block(t_parser *p);
static int      parse_params(t_parser *p, t_node *func);
static t_node   *parse_func(t_parser *p, int is_prog);

/* ---------- helpers ---------- */

static t_node   *error_at(t_parser *p, const t_token *tok, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/*
 * Fills the error at tok and returns NULL, so callers can return it.
 * At end of file the caret goes right after the last token (where the
 * missing thing should be), not on the empty line after the last '\n'.
 */
static t_node   *error_at(t_parser *p, const t_token *tok, const char *fmt, ...)
{
    va_list ap;
    size_t  offset;

    offset = tok->offset;
    if (tok->type == TOK_EOF)
        offset = p->last_end;
    va_start(ap, fmt);
    error_vset(p->err, ERR_SYNTAX, offset, tok->len, fmt, ap);
    va_end(ap);
    p->err->incomplete = (tok->type == TOK_EOF);
    return (NULL);
}

static void error_after_last(t_parser *p, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Fills the error just after the last consumed token. */
static void error_after_last(t_parser *p, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_vset(p->err, ERR_SYNTAX, p->last_end, 1, fmt, ap);
    va_end(ap);
    p->err->incomplete = (p->cur.type == TOK_EOF);
}

static int  advance(t_parser *p)
{
    if (p->cur.type != TOK_EOF)
        p->last_end = p->cur.offset + p->cur.len;
    p->prev = p->cur;
    return (lexer_next(&p->lx, &p->cur, p->err));
}

static int  check(t_parser *p, t_token_type type)
{
    return (p->cur.type == type);
}

/*
 * Whether the token after cur has the given type, without consuming
 * anything (a copy of the lexer reads it). A lexer error there just
 * means "no": it is reported when that token is really read.
 */
static int  peek_is(t_parser *p, int distance, t_token_type type)
{
    t_lexer lx;
    t_token tok;
    t_error err;

    lx = p->lx;
    tok.type = TOK_EOF;
    while (distance-- > 0)
        if (lexer_next(&lx, &tok, &err) == -1)
            return (0);
    return (tok.type == type);
}

/* Consumes cur if it has the given type. -1 only on a lexer error. */
static int  match(t_parser *p, t_token_type type, int *matched)
{
    *matched = check(p, type);
    if (*matched)
        return (advance(p));
    return (0);
}

/* Consumes cur if it has the given type; otherwise "expected X". */
static int  expect(t_parser *p, t_token_type type)
{
    if (check(p, type))
        return (advance(p));
    error_at(p, &p->cur, "expected %s but found %s",
        token_type_desc(type), token_type_desc(p->cur.type));
    return (-1);
}

/*
 * A missing ';' is reported right after the previous token (usually the
 * end of the line), not at the next token, which may be lines below.
 */
/* In the REPL the ';' that would end the input may be left out. */
static int  expect_semicolon(t_parser *p)
{
    if (check(p, TOK_SEMICOLON))
        return (advance(p));
    if (p->repl && check(p, TOK_EOF))
        return (0);
    error_after_last(p, "expected ';' but found %s",
        token_type_desc(p->cur.type));
    return (-1);
}

static t_node   *node_new(t_parser *p, t_node_type type, size_t offset)
{
    t_node  *n;

    n = arena_alloc(p->arena, sizeof(t_node));
    n->type = type;
    n->offset = offset;
    return (n);
}

/* Sets the span of n to end at the last consumed token. */
static t_node   *node_end(t_parser *p, t_node *n)
{
    n->len = p->prev.offset + p->prev.len - n->offset;
    return (n);
}

/* Text of a token: offsets are global, the text is local to the source. */
static const char   *tok_text(t_parser *p, const t_token *tok)
{
    return (p->src->text + (tok->offset - p->src->base));
}

static t_name   token_name(t_parser *p, const t_token *tok)
{
    t_name  name;

    name.ptr = tok_text(p, tok);
    name.len = tok->len;
    return (name);
}

/* ---------- literals ---------- */

/*
 * The text of a number token without its '_' separators and
 * '\0'-terminated, as strtod and parse_int need it. Lives in the arena.
 */
static char *number_text(t_parser *p, const t_token *tok)
{
    const char  *s;
    char        *out;
    size_t      i;
    size_t      n;

    s = tok_text(p, tok);
    out = arena_alloc(p->arena, tok->len + 1);
    n = 0;
    for (i = 0; i < tok->len; i++)
        if (s[i] != '_')
            out[n++] = s[i];
    out[n] = '\0';
    return (out);
}

static int  digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    return (c - 'A' + 10);
}

/*
 * An INT token (already consumed). The lexer checked its digits; here
 * only its size can fail: an int must fit in 64 bits.
 */
static t_node   *parse_int(t_parser *p, const t_token *tok)
{
    const char  *s;
    t_node      *n;
    uint64_t    value;
    int         base;
    int         d;

    s = number_text(p, tok);
    base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        base = 16;
    else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B'))
        base = 2;
    else if (s[0] == '0' && (s[1] == 'o' || s[1] == 'O'))
        base = 8;
    if (base != 10)
        s += 2;
    value = 0;
    for (; *s; s++)
    {
        d = digit_value(*s);
        if (value > ((uint64_t)INT64_MAX - (uint64_t)d) / (uint64_t)base)
            return (error_at(p, tok, "integer literal is too large (the"
                    " largest is %lld)", (long long)INT64_MAX));
        value = value * (uint64_t)base + (uint64_t)d;
    }
    n = node_new(p, NODE_INT, tok->offset);
    n->as.integer = (int64_t)value;
    return (node_end(p, n));
}

static int  hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    return (c - 'A' + 10);
}

/*
 * One escape (the lexer checked it) at s, its backslash: writes what it
 * stands for to out and returns how many source bytes it took.
 */
static size_t   decode_escape(const char *s, char *out, size_t *j)
{
    int32_t cp;
    size_t  i;

    if (s[1] == 'x')
    {
        out[(*j)++] = (char)(hex_digit(s[2]) * 16 + hex_digit(s[3]));
        return (4);
    }
    if (s[1] == 'u')
    {
        cp = 0;
        for (i = 3; s[i] != '}'; i++)
            cp = cp * 16 + hex_digit(s[i]);
        *j += (size_t)utf8_encode(cp, out + *j);
        return (i + 1);
    }
    out[(*j)++] = s[1] == 'n' ? '\n' : s[1] == 't' ? '\t' : s[1] == 'r' ? '\r'
        : s[1] == '0' ? '\0' : s[1];
    return (2);
}

/*
 * A STRING token (already checked by the lexer) as its text: quotes and
 * the r prefix off, escapes decoded (none in a raw string), and every
 * byte of the source that is not valid UTF-8 replaced by U+FFFD, so a
 * string is always valid UTF-8. The result lives in the arena.
 */
static t_node   *parse_string(t_parser *p)
{
    t_node      *n;
    const char  *s;
    char        *out;
    size_t      i;
    size_t      j;
    size_t      len;
    int32_t     cp;
    int         raw;
    int         quotes;
    int         step;

    n = node_new(p, NODE_STRING, p->cur.offset);
    s = tok_text(p, &p->cur);
    raw = (s[0] == 'r');
    s += raw;
    len = p->cur.len - (size_t)raw;
    quotes = (len >= 6 && s[1] == s[0] && s[2] == s[0]) ? 3 : 1;
    s += quotes;
    len -= 2 * (size_t)quotes;
    out = arena_alloc(p->arena, 3 * len + 1);
    i = 0;
    j = 0;
    while (i < len)
    {
        if (s[i] == '\\' && !raw)
        {
            i += decode_escape(s + i, out, &j);
            continue ;
        }
        step = utf8_decode(s + i, len - i, &cp);
        if (cp == -1)
        {
            memcpy(out + j, UTF8_REPLACEMENT, 3);
            j += 3;
        }
        else
        {
            memcpy(out + j, s + i, (size_t)step);
            j += (size_t)step;
        }
        i += (size_t)step;
    }
    out[j] = '\0';
    n->as.string.chars = out;
    n->as.string.len = j;
    if (advance(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* An error at local position i of the f-string token (len characters). */
static t_node   *fstring_error(t_parser *p, size_t i, size_t len,
                    const char *msg)
{
    error_set(p->err, ERR_SYNTAX, p->cur.offset + i, len, "%s", msg);
    return (NULL);
}

/* Digits at s[*i]: their value, capped (more than max is an error). */
static int  spec_number(const char *s, size_t *i, size_t end, int max)
{
    long    v;

    v = 0;
    while (*i < end && s[*i] >= '0' && s[*i] <= '9')
    {
        if (v <= max)
            v = v * 10 + (s[*i] - '0');
        (*i)++;
    }
    return (v > max ? -2 : (int)v);
}

/*
 * The format specifier s[start, end) of a field, as in Python:
 * [[fill]align][sign][#][0][width][grouping][.precision][type].
 */
static int  parse_spec(t_parser *p, const char *s, size_t start, size_t end,
                t_fmt_spec *f)
{
    size_t  i;
    int32_t cp;
    int     n;

    f->fill = ' ';
    f->precision = -1;
    i = start;
    n = i < end ? utf8_decode(s + i, end - i, &cp) : 0;
    if (i + (size_t)n < end && strchr("<>^=", s[i + n]) && cp > 0)
    {
        f->fill = cp;
        f->align = s[i + n];
        i += (size_t)n + 1;
    }
    else if (i < end && strchr("<>^=", s[i]))
        f->align = s[i++];
    if (i < end && strchr("+- ", s[i]))
        f->sign = s[i++];
    if (i < end && s[i] == '#' && ++i)
        f->alt = 1;
    if (i < end && s[i] == '0' && ++i)
        f->zero = 1;
    f->width = spec_number(s, &i, end, 1000000);
    if (i < end && (s[i] == ',' || s[i] == '_'))
        f->grouping = s[i++];
    if (i < end && s[i] == '.')
    {
        i++;
        if (i >= end || s[i] < '0' || s[i] > '9')
            return (fstring_error(p, i, 1, "format precision needs digits"
                    " after '.'") ? 0 : -1);
        f->precision = spec_number(s, &i, end, 1000);
    }
    if (i < end && strchr("sdboxXeEfFgG%", s[i]))
        f->type = s[i++];
    if (f->width == -2 || f->precision == -2)
        return (fstring_error(p, start, end - start, "format width (up to"
                " 1000000) or precision (up to 1000) is too large") ? 0 : -1);
    if (i < end)
        return (fstring_error(p, start, end - start, "invalid format"
                " specifier") ? 0 : -1);
    if ((f->grouping == ',' && f->type && strchr("boxX", f->type))
        || (f->grouping && f->type == 's')
        || (f->precision >= 0 && f->type && strchr("dboxX", f->type))
        || (f->alt && (!f->type || !strchr("boxX", f->type))))
        return (fstring_error(p, start, end - start, "this format specifier"
                " does not fit its type") ? 0 : -1);
    return (0);
}

/*
 * The expression of a field, s[start, end) of the token: parsed by a
 * parser of its own over that part of the source (so positions in errors
 * are the real ones), which must use all of it.
 */
static t_node   *parse_field_expr(t_parser *p, size_t start, size_t end)
{
    t_parser    sub;
    t_node      *expr;
    size_t      local;

    local = p->cur.offset - p->src->base;
    sub = *p;
    sub.repl = 0;
    lexer_init(&sub.lx, p->src);
    sub.lx.pos = local + start;
    sub.lx.len = local + end;
    sub.cur.type = TOK_EOF;
    sub.last_end = p->cur.offset + start;
    if (advance(&sub) == -1)
        return (NULL);
    if (check(&sub, TOK_EOF))
        return (fstring_error(p, start - 1, end - start + 2, "empty"
                " expression in an f-string field"));
    expr = parse_expr(&sub, PREC_TERNARY);
    if (expr && !check(&sub, TOK_EOF))
        expr = error_at(&sub, &sub.cur, "expected '}' to end the f-string"
                " field but found %s", token_type_desc(sub.cur.type));
    p->err->incomplete = 0;
    return (expr);
}

/* Appends the literal text s[from, to) (escapes, {{ }} and bad UTF-8). */
static size_t   fstring_literal(const char *s, size_t from, size_t to,
                    char *out, size_t j)
{
    int32_t cp;
    int     step;

    while (from < to)
    {
        if (s[from] == '\\')
            from += decode_escape(s + from, out, &j);
        else if ((s[from] == '{' || s[from] == '}') && from + 1 < to
            && s[from + 1] == s[from])
        {
            out[j++] = s[from];
            from += 2;
        }
        else
        {
            step = utf8_decode(s + from, to - from, &cp);
            memcpy(out + j, cp == -1 ? UTF8_REPLACEMENT : s + from,
                cp == -1 ? 3 : (size_t)step);
            j += cp == -1 ? 3 : (size_t)step;
            from += (size_t)step;
        }
    }
    return (j);
}

/* A part of literal text, if there is any. */
static t_node   *literal_part(t_parser *p, const char *s, size_t from,
                    size_t to)
{
    t_node  *n;
    char    *out;
    size_t  len;

    out = arena_alloc(p->arena, 3 * (to - from) + 1);
    len = fstring_literal(s, from, to, out, 0);
    out[len] = '\0';
    n = node_new(p, NODE_STRING, p->cur.offset + from);
    n->len = to - from;
    n->as.string.chars = out;
    n->as.string.len = len;
    return (n);
}

/* The field {expr} or {expr:spec} whose '{' is s[open]; its len spans it. */
static t_node   *parse_field(t_parser *p, const char *s, size_t open)
{
    t_node  *f;
    size_t  close;
    size_t  colon;

    close = lex_fstring_field(s, p->cur.len, open + 1, &colon);
    f = node_new(p, NODE_FORMAT, p->cur.offset + open);
    f->len = close + 1 - open;
    f->as.format.spec.fill = ' ';
    f->as.format.spec.precision = -1;
    f->as.format.expr = parse_field_expr(p, open + 1, colon);
    if (!f->as.format.expr)
        return (NULL);
    if (colon < close && parse_spec(p, s, colon + 1, close,
            &f->as.format.spec) == -1)
        return (NULL);
    return (f);
}

/*
 * f"text {expr} text {expr:spec}" (the lexer found where it ends):
 * literal parts and FORMAT parts, in order.
 */
static t_node   *parse_fstring(t_parser *p)
{
    t_node      *n;
    t_node      **tail;
    const char  *s;
    size_t      i;
    size_t      from;
    size_t      end;
    size_t      quotes;

    n = node_new(p, NODE_FSTRING, p->cur.offset);
    tail = &n->as.list.items;
    s = tok_text(p, &p->cur);
    quotes = (p->cur.len >= 7 && s[2] == s[1] && s[3] == s[1]) ? 3 : 1;
    end = p->cur.len - quotes;
    i = 1 + quotes;
    from = i;
    while (i <= end)
    {
        if (i < end && (s[i] == '{' || s[i] == '}') && s[i + 1] == s[i])
            i += 2;
        else if (i < end && s[i] == '\\')
            i += 2;
        else if (i < end && s[i] != '{')
            i++;
        else
        {
            if (i > from)
            {
                *tail = literal_part(p, s, from, i);
                tail = &(*tail)->next;
            }
            if (i == end)
                break ;
            *tail = parse_field(p, s, i);
            if (!*tail)
                return (NULL);
            i += (*tail)->len;
            tail = &(*tail)->next;
            from = i;
        }
    }
    if (advance(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* Comma-separated expressions up to (and including) the closing token. */
static int  parse_args(t_parser *p, t_token_type close, t_node **out)
{
    t_node  **tail;
    int     comma;

    *out = NULL;
    tail = out;
    if (!check(p, close))
    {
        comma = 1;
        while (comma)
        {
            *tail = parse_expr(p, PREC_TERNARY);
            if (!*tail)
                return (-1);
            tail = &(*tail)->next;
            if (match(p, TOK_COMMA, &comma) == -1)
                return (-1);
        }
    }
    return (expect(p, close));
}

static int      parse_for_names(t_parser *p, t_node *n);
static t_node   *parse_paren_cond(t_parser *p);

/*
 * One clause of a comprehension, at its "for" or "if": a FOR over
 * (names in iterable) or an IF (cond), with an empty body (*body) that
 * the caller fills with the next clause.
 */
static t_node   *parse_comp_clause(t_parser *p, t_node ***body)
{
    t_node  *n;

    n = node_new(p, check(p, TOK_FOR) ? NODE_FOR : NODE_IF, p->cur.offset);
    if (advance(p) == -1)
        return (NULL);
    if (n->type == NODE_IF)
    {
        n->as.if_.cond = parse_paren_cond(p);
        n->as.if_.then = node_new(p, NODE_BLOCK, n->offset);
        *body = &n->as.if_.then->as.list.items;
        return (n->as.if_.cond ? n : NULL);
    }
    if (expect(p, TOK_LPAREN) == -1)
        return (NULL);
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected loop variable but found %s",
                token_type_desc(p->cur.type)));
    n->as.for_.name = token_name(p, &p->cur);
    if (advance(p) == -1 || parse_for_names(p, n) == -1
        || expect(p, TOK_IN) == -1)
        return (NULL);
    n->as.for_.iterable = parse_expr(p, PREC_TERNARY);
    if (!n->as.for_.iterable || expect(p, TOK_RPAREN) == -1)
        return (NULL);
    n->as.for_.body = node_new(p, NODE_BLOCK, n->offset);
    *body = &n->as.for_.body->as.list.items;
    return (n);
}

/*
 * The rest of a comprehension once its value is parsed (value, at
 * start): for (names in iterable), then any number of for and if (cond)
 * clauses, as in Python. Each clause runs inside the one before it (a
 * FOR or an IF whose body is the next one), and the innermost adds the
 * value (COMP_ADD).
 */
static t_node   *parse_comprehension(t_parser *p, t_node *value, size_t start,
                    int kind)
{
    t_node  *comp;
    t_node  **body;
    t_node  **here;

    comp = node_new(p, NODE_COMPREHENSION, start);
    comp->as.comp.kind = kind;
    comp->as.comp.add = node_new(p, NODE_COMP_ADD, value->offset);
    comp->as.comp.add->len = value->len;
    comp->as.comp.add->as.comp.loop = value;
    body = &comp->as.comp.loop;
    while (check(p, TOK_FOR) || (body != &comp->as.comp.loop
            && check(p, TOK_IF)))
    {
        here = body;
        *here = parse_comp_clause(p, &body);
        if (!*here)
            return (NULL);
        node_end(p, *here);
    }
    *body = comp->as.comp.add;
    return (comp);
}

/* [ ], [a, b, c] or [value for (x in l) if (c)] ("[" already consumed). */
static t_node   *parse_list_literal(t_parser *p, size_t start)
{
    t_node  *n;
    t_node  *first;
    int     comma;

    if (check(p, TOK_RBRACKET))
    {
        if (advance(p) == -1)
            return (NULL);
        return (node_end(p, node_new(p, NODE_LIST, start)));
    }
    first = parse_expr(p, PREC_TERNARY);
    if (!first)
        return (NULL);
    if (check(p, TOK_FOR))
    {
        n = parse_comprehension(p, first, start, COMP_LIST);
        if (!n || expect(p, TOK_RBRACKET) == -1)
            return (NULL);
        return (node_end(p, n));
    }
    n = node_new(p, NODE_LIST, start);
    n->as.list.items = first;
    if (match(p, TOK_COMMA, &comma) == -1)
        return (NULL);
    if (comma && parse_args(p, TOK_RBRACKET, &first->next) == -1)
        return (NULL);
    if (!comma && expect(p, TOK_RBRACKET) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* expr, or key: value (a pair; the resolver only lets them into dict). */
static t_node   *parse_pair_or_expr(t_parser *p)
{
    t_node  *key;
    t_node  *n;

    key = parse_expr(p, PREC_TERNARY);
    if (!key || !check(p, TOK_COLON))
        return (key);
    n = node_new(p, NODE_PAIR, key->offset);
    n->as.pair.key = key;
    if (advance(p) == -1)
        return (NULL);
    n->as.pair.value = parse_expr(p, PREC_TERNARY);
    if (!n->as.pair.value)
        return (NULL);
    return (n);
}

/*
 * The arguments of a call, up to and including ')': positional ones
 * (also ...t, the items of a tuple), then named ones (name = value).
 */
static int  parse_call_args(t_parser *p, t_node **out)
{
    t_node  **tail;
    t_node  *n;
    int     comma;
    int     named;

    *out = NULL;
    tail = out;
    named = 0;
    comma = !check(p, TOK_RPAREN);
    while (comma)
    {
        if (check(p, TOK_IDENT) && peek_is(p, 1, TOK_ASSIGN))
        {
            n = node_new(p, NODE_NAMED_ARG, p->cur.offset);
            n->as.named.name = token_name(p, &p->cur);
            n->as.named.index = -1;
            if (advance(p) == -1 || advance(p) == -1)
                return (-1);
            n->as.named.value = parse_expr(p, PREC_TERNARY);
            if (!n->as.named.value)
                return (-1);
            named = 1;
        }
        else if (named)
        {
            error_at(p, &p->cur, "positional arguments go before the named"
                " ones");
            return (-1);
        }
        else if (check(p, TOK_ELLIPSIS))
        {
            n = node_new(p, NODE_SPREAD, p->cur.offset);
            if (advance(p) == -1)
                return (-1);
            n->as.spread.value = parse_expr(p, PREC_TERNARY);
            if (!n->as.spread.value)
                return (-1);
        }
        else
            n = parse_pair_or_expr(p);
        if (n && check(p, TOK_FOR) && tail == out)
            n = parse_comprehension(p, n, n->offset, COMP_GENERAL);
        if (!n)
            return (-1);
        *tail = node_end(p, n);
        tail = &n->next;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (-1);
    }
    return (expect(p, TOK_RPAREN));
}

/*
 * Whether "(" (already consumed) starts the parameters of an arrow
 * function. Never a parenthesised expression: "()", "(...", "(x," and
 * "(x =" cannot be one, and "(x)" is one only if no "=>" follows.
 */
static int  starts_arrow(t_parser *p)
{
    if (check(p, TOK_RPAREN) || check(p, TOK_ELLIPSIS))
        return (1);
    if (!check(p, TOK_IDENT))
        return (0);
    if (peek_is(p, 1, TOK_COMMA) || peek_is(p, 1, TOK_ASSIGN))
        return (1);
    return (peek_is(p, 1, TOK_RPAREN) && peek_is(p, 2, TOK_ARROW));
}

/*
 * (params) => expression, or (params) => { block }, with "(" already
 * consumed; start is its offset. A lambda, like func (params) { }.
 */
static t_node   *parse_arrow(t_parser *p, size_t start)
{
    t_node  *n;

    n = node_new(p, NODE_FUNC, start);
    n->as.func.is_lambda = 1;
    if (parse_params(p, n) == -1 || expect(p, TOK_ARROW) == -1)
        return (NULL);
    if (check(p, TOK_LBRACE))
        n->as.func.body = parse_block(p);
    else
    {
        n->as.func.expr_body = 1;
        n->as.func.body = parse_expr(p, PREC_TERNARY);
    }
    if (!n->as.func.body)
        return (NULL);
    return (node_end(p, n));
}

/* func (params) { block } in an expression ("func" already consumed). */
static t_node   *parse_func_expr(t_parser *p, size_t start)
{
    t_node  *n;

    if (check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "a function used as a value has no"
                " name: func (...) { ... }"));
    n = node_new(p, NODE_FUNC, start);
    n->as.func.is_lambda = 1;
    if (expect(p, TOK_LPAREN) == -1 || parse_params(p, n) == -1)
        return (NULL);
    n->as.func.body = parse_block(p);
    if (!n->as.func.body)
        return (NULL);
    return (node_end(p, n));
}

/* ---------- expressions (Pratt) ---------- */

static t_node   *parse_unary(t_parser *p, t_token_type op, int prec)
{
    t_node  *n;

    n = node_new(p, NODE_UNARY, p->prev.offset);
    n->as.unary.op = op;
    n->as.unary.operand = parse_expr(p, prec);
    if (!n->as.unary.operand)
        return (NULL);
    return (node_end(p, n));
}

static t_node   *parse_prefix_incdec(t_parser *p)
{
    t_node  *n;

    n = node_new(p, NODE_INCDEC, p->prev.offset);
    n->as.incdec.op = p->prev.type;
    n->as.incdec.prefix = 1;
    n->as.incdec.target = parse_expr(p, PREC_PREFIX);
    if (!n->as.incdec.target)
        return (NULL);
    return (node_end(p, n));
}

/* min_prec: 'not' is only valid where a not_expr is (EBNF): not in a < not b. */
static t_node   *parse_primary(t_parser *p, int min_prec)
{
    t_node  *n;
    t_token tok;

    tok = p->cur;
    if (tok.type == TOK_STRING)
        return (parse_string(p));
    if (tok.type == TOK_FSTRING)
        return (parse_fstring(p));
    if (advance(p) == -1)
        return (NULL);
    switch (tok.type)
    {
        case TOK_INT:
            return (parse_int(p, &tok));
        case TOK_FLOAT:
            n = node_new(p, NODE_FLOAT, tok.offset);
            n->as.number = strtod(number_text(p, &tok), NULL);
            return (node_end(p, n));
        case TOK_TRUE:
        case TOK_FALSE:
            n = node_new(p, NODE_BOOL, tok.offset);
            n->as.boolean = (tok.type == TOK_TRUE);
            return (node_end(p, n));
        case TOK_NONE:
            return (node_end(p, node_new(p, NODE_NONE, tok.offset)));
        case TOK_SELF:
            return (node_end(p, node_new(p, NODE_SELF, tok.offset)));
        case TOK_SUPER:
            return (node_end(p, node_new(p, NODE_SUPER, tok.offset)));
        case TOK_IDENT:
            n = node_new(p, NODE_IDENT, tok.offset);
            n->as.ident.name = token_name(p, &tok);
            return (node_end(p, n));
        case TOK_FUNC:
            return (parse_func_expr(p, tok.offset));
        case TOK_LPAREN:
            if (starts_arrow(p))
                return (parse_arrow(p, tok.offset));
            n = parse_expr(p, PREC_TERNARY);
            if (!n || expect(p, TOK_RPAREN) == -1)
                return (NULL);
            return (n);
        case TOK_LBRACKET:
            return (parse_list_literal(p, tok.offset));
        case TOK_MINUS:
        case TOK_TILDE:
            return (parse_unary(p, tok.type, PREC_PREFIX));
        case TOK_NOT:
            if (min_prec > PREC_NOT)
                return (error_at(p, &tok, "'not' needs parentheses here"));
            return (parse_unary(p, TOK_NOT, PREC_NOT));
        case TOK_PLUS_PLUS:
        case TOK_MINUS_MINUS:
            return (parse_prefix_incdec(p));
        default:
            return (error_at(p, &tok, "expected expression but found %s",
                    token_type_desc(tok.type)));
    }
}

/* A part of a slice: absent when the next token ends it. */
static int  parse_slice_part(t_parser *p, t_node **out)
{
    *out = NULL;
    if (check(p, TOK_COLON) || check(p, TOK_RBRACKET))
        return (0);
    *out = parse_expr(p, PREC_TERNARY);
    return (*out ? 0 : -1);
}

/* After "[": index "]", or a slice [start] ":" [end] [ ":" [step] ] "]". */
static int  parse_index(t_parser *p, t_node *n)
{
    int found;

    if (check(p, TOK_RBRACKET))
    {
        error_at(p, &p->cur, "expected an index or a slice but found ']'");
        return (-1);
    }
    if (parse_slice_part(p, &n->as.index.index) == -1
        || match(p, TOK_COLON, &n->as.index.slice) == -1)
        return (-1);
    if (n->as.index.slice)
    {
        if (parse_slice_part(p, &n->as.index.end) == -1
            || match(p, TOK_COLON, &found) == -1)
            return (-1);
        if (found && parse_slice_part(p, &n->as.index.step) == -1)
            return (-1);
    }
    return (expect(p, TOK_RBRACKET));
}

/* Call, index, field access and postfix ++/--, applied to left. */
static t_node   *parse_postfix(t_parser *p, t_node *left)
{
    t_node  *n;

    while (1)
    {
        if (check(p, TOK_LPAREN))
        {
            n = node_new(p, NODE_CALL, left->offset);
            n->as.call.callee = left;
            if (advance(p) == -1
                || parse_call_args(p, &n->as.call.args) == -1)
                return (NULL);
        }
        else if (check(p, TOK_LBRACKET))
        {
            n = node_new(p, NODE_INDEX, left->offset);
            n->as.index.object = left;
            if (advance(p) == -1 || parse_index(p, n) == -1)
                return (NULL);
        }
        else if (check(p, TOK_DOT))
        {
            n = node_new(p, NODE_FIELD, left->offset);
            n->as.field.object = left;
            n->as.field.slot = -1;
            if (advance(p) == -1)
                return (NULL);
            if (!check(p, TOK_IDENT))
                return (error_at(p, &p->cur, "expected field name after '.'"
                        " but found %s", token_type_desc(p->cur.type)));
            n->as.field.name = token_name(p, &p->cur);
            if (advance(p) == -1)
                return (NULL);
        }
        else if (check(p, TOK_PLUS_PLUS) || check(p, TOK_MINUS_MINUS))
        {
            n = node_new(p, NODE_INCDEC, left->offset);
            n->as.incdec.op = p->cur.type;
            n->as.incdec.target = left;
            if (advance(p) == -1)
                return (NULL);
        }
        else
            return (left);
        left = node_end(p, n);
    }
}

static t_node   *make_binary(t_parser *p, t_token_type op, t_node *left,
        t_node *right)
{
    t_node  *n;

    n = node_new(p, NODE_BINARY, left->offset);
    n->as.binary.op = op;
    n->as.binary.left = left;
    n->as.binary.right = right;
    return (node_end(p, n));
}


/*
 * c ? a : b, after c (already parsed as left). Both branches are full
 * expressions; the else branch binds to the right, so a ? b : c ? d : e
 * is a ? b : (c ? d : e).
 */
static t_node   *parse_ternary(t_parser *p, t_node *cond)
{
    t_node  *n;

    if (advance(p) == -1)
        return (NULL);
    n = node_new(p, NODE_TERNARY, cond->offset);
    n->as.ternary.cond = cond;
    n->as.ternary.then = parse_expr(p, PREC_TERNARY);
    if (!n->as.ternary.then || expect(p, TOK_COLON) == -1)
        return (NULL);
    n->as.ternary.else_ = parse_expr(p, PREC_TERNARY);
    if (!n->as.ternary.else_)
        return (NULL);
    return (node_end(p, n));
}

/*
 * Parses an expression whose binary operators all bind at least as
 * tightly as min_prec. Binary operators are left-associative; comparisons
 * do not chain. "a is not b" becomes UNARY not (BINARY is), and
 * "a not in b" UNARY not (BINARY in).
 */
static t_node   *parse_expr(t_parser *p, int min_prec)
{
    t_node      *left;
    t_node      *right;
    t_token     op;
    int         prec;
    int         negate;

    if (stack_exhausted())
        return (error_at(p, &p->cur, "too deeply nested"));
    left = parse_primary(p, min_prec);
    if (left)
        left = parse_postfix(p, left);
    while (left)
    {
        op = p->cur;
        if (op.type == TOK_QUESTION && min_prec <= PREC_TERNARY)
            return (parse_ternary(p, left));
        negate = 0;
        if (op.type == TOK_NOT && min_prec <= PREC_COMPARISON
            && peek_is(p, 1, TOK_IN))
        {
            if (advance(p) == -1)
                return (NULL);
            negate = 1;
            op = p->cur;
        }
        prec = g_binary_prec[op.type];
        if (prec == PREC_NONE || prec < min_prec)
            break ;
        if (advance(p) == -1)
            return (NULL);
        if (op.type == TOK_IS && match(p, TOK_NOT, &negate) == -1)
            return (NULL);
        right = parse_expr(p, prec + 1);
        if (!right)
            return (NULL);
        left = make_binary(p, op.type, left, right);
        if (negate)
        {
            right = node_new(p, NODE_UNARY, left->offset);
            right->as.unary.op = TOK_NOT;
            right->as.unary.operand = left;
            left = node_end(p, right);
        }
        if (prec == PREC_COMPARISON
            && g_binary_prec[p->cur.type] == PREC_COMPARISON)
            return (error_at(p, &p->cur,
                    "comparison operators cannot be chained"));
    }
    return (left);
}

/* ---------- statements ---------- */

static t_node   *parse_statement(t_parser *p);

/* "{" { statement } "}" */
static t_node   *parse_block(t_parser *p)
{
    t_node  *n;
    t_node  **tail;

    n = node_new(p, NODE_BLOCK, p->cur.offset);
    if (expect(p, TOK_LBRACE) == -1)
        return (NULL);
    tail = &n->as.list.items;
    while (!check(p, TOK_RBRACE))
    {
        if (check(p, TOK_EOF))
            return (error_at(p, &p->cur, "expected '}' but found end of file"));
        *tail = parse_statement(p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    if (advance(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* "(" expression ")" after if / elif / while / switch. */
static t_node   *parse_paren_cond(t_parser *p)
{
    t_node  *cond;

    if (expect(p, TOK_LPAREN) == -1)
        return (NULL);
    cond = parse_expr(p, PREC_TERNARY);
    if (!cond || expect(p, TOK_RPAREN) == -1)
        return (NULL);
    return (cond);
}

/*
 * var / vemp IDENT [ "[" expr "]" | "=" expr ] ";"
 * const IDENT "=" expr ";"
 * (keyword already read: kw is TOK_VAR, TOK_VEMP or TOK_CONST)
 */
static t_node   *parse_multi_var(t_parser *p, t_node *first, t_token_type kw);

static t_node   *parse_var(t_parser *p, t_token_type kw)
{
    t_node  *n;
    int     found;

    n = node_new(p, NODE_VAR, p->prev.offset);
    n->as.var.is_vemp = (kw == TOK_VEMP);
    n->as.var.is_const = (kw == TOK_CONST);
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected %s name but found %s",
                kw == TOK_CONST ? "constant" : "variable",
                token_type_desc(p->cur.type)));
    n->as.var.name = token_name(p, &p->cur);
    if (advance(p) == -1)
        return (NULL);
    if (check(p, TOK_COMMA))
        return (parse_multi_var(p, node_end(p, n), kw));
    if (kw == TOK_CONST && !check(p, TOK_ASSIGN))
        return (error_at(p, &p->cur, "a constant needs a value: const %.*s"
                " = ...;", (int)n->as.var.name.len, n->as.var.name.ptr));
    if (match(p, TOK_LBRACKET, &found) == -1)
        return (NULL);
    if (found)
    {
        n->as.var.size = parse_expr(p, PREC_TERNARY);
        if (!n->as.var.size || expect(p, TOK_RBRACKET) == -1)
            return (NULL);
        if (check(p, TOK_ASSIGN))
            return (error_at(p, &p->cur,
                    "a sized list cannot have an initial value"));
    }
    else
    {
        if (match(p, TOK_ASSIGN, &found) == -1)
            return (NULL);
        if (found)
        {
            n->as.var.init = parse_expr(p, PREC_TERNARY);
            if (!n->as.var.init)
                return (NULL);
        }
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* Fails when nvalues values cannot go into ntargets targets. */
static int  check_value_count(t_parser *p, const t_node *values, int nvalues,
                int ntargets)
{
    const t_node    *last;

    if (nvalues == 1 || nvalues == ntargets)
        return (0);
    for (last = values; last->next; last = last->next)
        ;
    error_set(p->err, ERR_SYNTAX, values->offset,
        last->offset + last->len - values->offset,
        "cannot assign %d values to %d targets", nvalues, ntargets);
    return (-1);
}

static int  parse_more_exprs(t_parser *p, t_node **tail);

/*
 * var a, b = x, y; (also vemp and const), after the first name. Several
 * names always need a value: one per name, or a single tuple.
 */
static t_node   *parse_multi_var(t_parser *p, t_node *first, t_token_type kw)
{
    t_node  *n;
    t_node  **tail;
    int     ntargets;
    int     nvalues;
    int     comma;

    n = node_new(p, NODE_MULTI_VAR, first->offset);
    n->as.multi.targets = first;
    tail = &first->next;
    ntargets = 1;
    if (match(p, TOK_COMMA, &comma) == -1)
        return (NULL);
    while (comma)
    {
        if (!check(p, TOK_IDENT))
            return (error_at(p, &p->cur, "expected %s name but found %s",
                    kw == TOK_CONST ? "constant" : "variable",
                    token_type_desc(p->cur.type)));
        *tail = node_new(p, NODE_VAR, p->cur.offset);
        (*tail)->as.var.name = token_name(p, &p->cur);
        (*tail)->as.var.is_vemp = (kw == TOK_VEMP);
        (*tail)->as.var.is_const = (kw == TOK_CONST);
        if (advance(p) == -1)
            return (NULL);
        node_end(p, *tail);
        tail = &(*tail)->next;
        ntargets++;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (NULL);
    }
    if (!check(p, TOK_ASSIGN))
        return (error_at(p, &p->cur, "several names need a value: %s a, b"
                " = ...;", kw == TOK_CONST ? "const" : kw == TOK_VEMP ? "vemp"
                : "var"));
    if (advance(p) == -1)
        return (NULL);
    n->as.multi.values = parse_expr(p, PREC_TERNARY);
    if (!n->as.multi.values)
        return (NULL);
    nvalues = parse_more_exprs(p, &n->as.multi.values->next);
    if (nvalues == -1
        || check_value_count(p, n->as.multi.values, nvalues, ntargets) == -1
        || expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/*
 * if (c) {} elif (c) {} else {}. Each elif becomes an IF in the else
 * branch of the previous one, so the chain is built from the front.
 */
static t_node   *parse_if(t_parser *p)
{
    t_node  *first;
    t_node  *n;
    int     found;

    first = node_new(p, NODE_IF, p->prev.offset);
    n = first;
    while (1)
    {
        n->as.if_.cond = parse_paren_cond(p);
        if (!n->as.if_.cond)
            return (NULL);
        n->as.if_.then = parse_block(p);
        if (!n->as.if_.then || match(p, TOK_ELIF, &found) == -1)
            return (NULL);
        if (!found)
            break ;
        n->as.if_.else_ = node_new(p, NODE_IF, p->prev.offset);
        n = n->as.if_.else_;
    }
    if (match(p, TOK_ELSE, &found) == -1)
        return (NULL);
    if (found)
    {
        n->as.if_.else_ = parse_block(p);
        if (!n->as.if_.else_)
            return (NULL);
    }
    /* Every IF in the chain ends where the whole statement ends. */
    for (n = first; n && n->type == NODE_IF; n = n->as.if_.else_)
        node_end(p, n);
    return (first);
}

/* do block while "(" expr ")" ";" (do already read) */
static t_node   *parse_do(t_parser *p)
{
    t_node  *n;

    n = node_new(p, NODE_DO_WHILE, p->prev.offset);
    n->as.while_.body = parse_block(p);
    if (!n->as.while_.body || expect(p, TOK_WHILE) == -1)
        return (NULL);
    n->as.while_.cond = parse_paren_cond(p);
    if (!n->as.while_.cond || expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

static t_node   *parse_while(t_parser *p)
{
    t_node  *n;

    n = node_new(p, NODE_WHILE, p->prev.offset);
    n->as.while_.cond = parse_paren_cond(p);
    if (!n->as.while_.cond)
        return (NULL);
    n->as.while_.body = parse_block(p);
    if (!n->as.while_.body)
        return (NULL);
    return (node_end(p, n));
}

/* for (k, v in ...): the names after the first one, as vemp VAR nodes. */
static int  parse_for_names(t_parser *p, t_node *n)
{
    t_node  **tail;
    int     comma;

    tail = &n->as.for_.more;
    if (match(p, TOK_COMMA, &comma) == -1)
        return (-1);
    while (comma)
    {
        if (!check(p, TOK_IDENT))
        {
            error_at(p, &p->cur, "expected loop variable but found %s",
                token_type_desc(p->cur.type));
            return (-1);
        }
        *tail = node_new(p, NODE_VAR, p->cur.offset);
        (*tail)->as.var.name = token_name(p, &p->cur);
        (*tail)->as.var.is_vemp = 1;
        if (advance(p) == -1)
            return (-1);
        node_end(p, *tail);
        tail = &(*tail)->next;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (-1);
    }
    return (0);
}

/* for "(" IDENT { "," IDENT } "in" expr ")" block [ "else" block ] */
static t_node   *parse_for(t_parser *p)
{
    t_node  *n;
    int     found;

    n = node_new(p, NODE_FOR, p->prev.offset);
    if (expect(p, TOK_LPAREN) == -1)
        return (NULL);
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected loop variable but found %s",
                token_type_desc(p->cur.type)));
    n->as.for_.name = token_name(p, &p->cur);
    if (advance(p) == -1 || parse_for_names(p, n) == -1
        || expect(p, TOK_IN) == -1)
        return (NULL);
    n->as.for_.iterable = parse_expr(p, PREC_TERNARY);
    if (!n->as.for_.iterable || expect(p, TOK_RPAREN) == -1)
        return (NULL);
    n->as.for_.body = parse_block(p);
    if (!n->as.for_.body || match(p, TOK_ELSE, &found) == -1)
        return (NULL);
    if (found)
    {
        n->as.for_.else_ = parse_block(p);
        if (!n->as.for_.else_)
            return (NULL);
    }
    return (node_end(p, n));
}

/* case_value = [ "-" ] NUMBER | STRING. A '-' is folded into the number. */
static t_node   *parse_case_value(t_parser *p)
{
    t_node  *n;
    size_t  start;
    int     negative;

    start = p->cur.offset;
    if (match(p, TOK_MINUS, &negative) == -1)
        return (NULL);
    if (check(p, TOK_STRING) && !negative)
        return (parse_string(p));
    if (negative && !check(p, TOK_INT) && !check(p, TOK_FLOAT))
        return (error_at(p, &p->cur, "'-' in a case value can only go"
                " before a number"));
    if (!check(p, TOK_INT) && !check(p, TOK_FLOAT))
        return (error_at(p, &p->cur, "case value must be a number or a"
                " string, found %s", token_type_desc(p->cur.type)));
    n = parse_primary(p, PREC_OR);
    if (!n)
        return (NULL);
    if (negative && n->type == NODE_INT)
        n->as.integer = -n->as.integer;
    else if (negative)
        n->as.number = -n->as.number;
    n->offset = start;
    return (node_end(p, n));
}

/* case value { "," value } block */
static t_node   *parse_case(t_parser *p)
{
    t_node  *n;
    t_node  **tail;
    int     comma;

    n = node_new(p, NODE_CASE, p->prev.offset);
    tail = &n->as.case_.values;
    comma = 1;
    while (comma)
    {
        *tail = parse_case_value(p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (NULL);
    }
    n->as.case_.body = parse_block(p);
    if (!n->as.case_.body)
        return (NULL);
    return (node_end(p, n));
}

/* switch "(" expr ")" "{" { case } [ default block ] "}" */
static t_node   *parse_switch(t_parser *p)
{
    t_node  *n;
    t_node  **tail;
    int     found;

    n = node_new(p, NODE_SWITCH, p->prev.offset);
    n->as.switch_.subject = parse_paren_cond(p);
    if (!n->as.switch_.subject || expect(p, TOK_LBRACE) == -1)
        return (NULL);
    tail = &n->as.switch_.cases;
    while (1)
    {
        if (match(p, TOK_CASE, &found) == -1)
            return (NULL);
        if (!found)
            break ;
        *tail = parse_case(p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    if (match(p, TOK_DEFAULT, &found) == -1)
        return (NULL);
    if (found)
    {
        n->as.switch_.default_ = parse_block(p);
        if (!n->as.switch_.default_)
            return (NULL);
    }
    if (check(p, TOK_CASE))
        return (error_at(p, &p->cur, "'case' after 'default'"));
    if (expect(p, TOK_RBRACE) == -1)
        return (NULL);
    return (node_end(p, n));
}

static t_node   *parse_return(t_parser *p)
{
    t_node  *n;

    n = node_new(p, NODE_RETURN, p->prev.offset);
    if (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE)
        && !check(p, TOK_EOF))
    {
        n->as.ret.value = parse_expr(p, PREC_TERNARY);
        if (!n->as.ret.value)
            return (NULL);
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/*
 * break [ outer ] ";" / continue [ outer ] ";" (keyword already read).
 * outer is not a keyword: only here, right after break or continue, does
 * the name mean "the loop around this one".
 */
static t_node   *parse_jump(t_parser *p, t_node_type type)
{
    t_node  *n;

    n = node_new(p, type, p->prev.offset);
    n->as.jump.levels = 1;
    if (check(p, TOK_IDENT))
    {
        if (p->cur.len != 5 || memcmp(tok_text(p, &p->cur), "outer", 5) != 0)
            return (error_at(p, &p->cur, "expected ';' or 'outer' after"
                    " '%s'", type == NODE_BREAK ? "break" : "continue"));
        n->as.jump.levels = 2;
        if (advance(p) == -1)
            return (NULL);
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/*
 * except [ Type { "," Type } ] [ "(" name ")" ] block (except already
 * consumed). No type: any error.
 */
static t_node   *parse_except(t_parser *p)
{
    t_node  *n;
    t_node  **tail;
    int     comma;

    n = node_new(p, NODE_EXCEPT, p->prev.offset);
    tail = &n->as.except.types;
    comma = check(p, TOK_IDENT);
    if (check(p, TOK_AS))
        return (error_at(p, &p->cur, "an except has no 'as': write except"
                " Type (name) { ... }"));
    while (comma)
    {
        if (!check(p, TOK_IDENT))
            return (error_at(p, &p->cur, "expected an error type but found"
                    " %s", token_type_desc(p->cur.type)));
        *tail = node_new(p, NODE_IDENT, p->cur.offset);
        (*tail)->as.ident.name = token_name(p, &p->cur);
        if (advance(p) == -1)
            return (NULL);
        node_end(p, *tail);
        tail = &(*tail)->next;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (NULL);
    }
    if (check(p, TOK_AS))
        return (error_at(p, &p->cur, "an except has no 'as': write except"
                " Type (name) { ... }"));
    if (check(p, TOK_LPAREN))
    {
        if (advance(p) == -1)
            return (NULL);
        if (!check(p, TOK_IDENT))
            return (error_at(p, &p->cur, "expected a name for the error but"
                    " found %s", token_type_desc(p->cur.type)));
        n->as.except.name = token_name(p, &p->cur);
        if (advance(p) == -1 || expect(p, TOK_RPAREN) == -1)
            return (NULL);
    }
    n->as.except.body = parse_block(p);
    if (!n->as.except.body)
        return (NULL);
    return (node_end(p, n));
}

/* try block { except } [ finally block ]: at least one of them. */
static t_node   *parse_try(t_parser *p)
{
    t_node  *n;
    t_node  **tail;
    int     found;

    n = node_new(p, NODE_TRY, p->prev.offset);
    n->as.try_.body = parse_block(p);
    if (!n->as.try_.body)
        return (NULL);
    tail = &n->as.try_.handlers;
    while (1)
    {
        if (match(p, TOK_EXCEPT, &found) == -1)
            return (NULL);
        if (!found)
            break ;
        *tail = parse_except(p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    if (match(p, TOK_FINALLY, &found) == -1)
        return (NULL);
    if (found)
    {
        n->as.try_.finally_ = parse_block(p);
        if (!n->as.try_.finally_)
            return (NULL);
    }
    if (!n->as.try_.handlers && !n->as.try_.finally_)
        return (error_at(p, &p->cur, "expected 'except' or 'finally' after"
                " the try block but found %s", token_type_desc(p->cur.type)));
    if (check(p, TOK_EXCEPT))
        return (error_at(p, &p->cur, "'except' after 'finally'"));
    return (node_end(p, n));
}

/* raise expr ";" / assert "(" cond [ "," message ] ")" ";" */
static t_node   *parse_raise(t_parser *p, t_token_type kw)
{
    t_node  *n;
    int     comma;

    n = node_new(p, kw == TOK_RAISE ? NODE_RAISE : NODE_ASSERT,
            p->prev.offset);
    if (kw == TOK_ASSERT && expect(p, TOK_LPAREN) == -1)
        return (NULL);
    n->as.raise.value = parse_expr(p, PREC_TERNARY);
    if (!n->as.raise.value)
        return (NULL);
    if (kw == TOK_ASSERT)
    {
        if (match(p, TOK_COMMA, &comma) == -1)
            return (NULL);
        if (comma && !(n->as.raise.message = parse_expr(p, PREC_TERNARY)))
            return (NULL);
        if (expect(p, TOK_RPAREN) == -1)
            return (NULL);
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

static int  is_assign_op(t_token_type type)
{
    return (type == TOK_ASSIGN || type == TOK_PLUS_EQ
        || type == TOK_MINUS_EQ || type == TOK_STAR_EQ
        || type == TOK_SLASH_EQ || type == TOK_SLASH_SLASH_EQ
        || type == TOK_PERCENT_EQ || type == TOK_AMP_EQ
        || type == TOK_PIPE_EQ || type == TOK_CARET_EQ
        || type == TOK_SHL_EQ || type == TOK_SHR_EQ);
}

/*
 * Comma-separated expressions after the first one (already parsed and
 * linked from *tail's predecessor). Returns how many there are in all.
 */
static int  parse_more_exprs(t_parser *p, t_node **tail)
{
    int count;
    int comma;

    count = 1;
    if (match(p, TOK_COMMA, &comma) == -1)
        return (-1);
    while (comma)
    {
        *tail = parse_expr(p, PREC_TERNARY);
        if (!*tail)
            return (-1);
        tail = &(*tail)->next;
        count++;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (-1);
    }
    return (count);
}

/*
 * a, b = x, y (the first target already parsed). Every value is
 * evaluated before any target is assigned, so a, b = b, a swaps. With
 * several values their number must be the number of targets; a single
 * value (a, b = t) is unpacked when it runs.
 */
static t_node   *parse_multi_assign(t_parser *p, t_node *first)
{
    t_node  *n;
    int     ntargets;
    int     nvalues;

    n = node_new(p, NODE_MULTI_ASSIGN, first->offset);
    n->as.multi.targets = first;
    ntargets = parse_more_exprs(p, &first->next);
    if (ntargets == -1)
        return (NULL);
    if (is_assign_op(p->cur.type) && !check(p, TOK_ASSIGN))
        return (error_at(p, &p->cur, "%s cannot assign to several targets:"
                " use '='", token_type_desc(p->cur.type)));
    if (expect(p, TOK_ASSIGN) == -1)
        return (NULL);
    n->as.multi.values = parse_expr(p, PREC_TERNARY);
    if (!n->as.multi.values)
        return (NULL);
    nvalues = parse_more_exprs(p, &n->as.multi.values->next);
    if (nvalues == -1
        || check_value_count(p, n->as.multi.values, nvalues, ntargets) == -1
        || expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/*
 * simple_stmt = expression [ assign_op expression ] ";"
 *             | expression { "," expression } "=" expression { "," expression } ";"
 * Whether the target is assignable is checked by the resolver.
 */
static t_node   *parse_simple(t_parser *p)
{
    t_node  *expr;
    t_node  *n;

    expr = parse_expr(p, PREC_TERNARY);
    if (!expr)
        return (NULL);
    if (check(p, TOK_COMMA))
        return (parse_multi_assign(p, expr));
    if (is_assign_op(p->cur.type))
    {
        n = node_new(p, NODE_ASSIGN, expr->offset);
        n->as.assign.op = p->cur.type;
        n->as.assign.target = expr;
        if (advance(p) == -1)
            return (NULL);
        n->as.assign.value = parse_expr(p, PREC_TERNARY);
        if (!n->as.assign.value)
            return (NULL);
        if (is_assign_op(p->cur.type))
            return (error_at(p, &p->cur, "assignments cannot be chained:"
                    " an assignment is a statement, not an expression"));
    }
    else
    {
        n = node_new(p, NODE_EXPR_STMT, expr->offset);
        n->as.expr_stmt.expr = expr;
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

static t_node   *parse_statement(t_parser *p)
{
    t_token_type    type;
    t_node          *n;

    if (stack_exhausted())
        return (error_at(p, &p->cur, "too deeply nested"));

    type = p->cur.type;
    if (type == TOK_ELSE || type == TOK_ELIF)
        return (error_at(p, &p->cur, "%s without a matching 'if'",
                token_type_desc(type)));
    if (type == TOK_EXCEPT || type == TOK_FINALLY)
        return (error_at(p, &p->cur, "%s without a matching 'try'",
                token_type_desc(type)));
    if (type == TOK_LBRACE)
        return (error_at(p, &p->cur, "unexpected '{': blocks only go after"
                " if, while, for, switch, func..."));
    if (type == TOK_MAN)
        return (error_at(p, &p->cur, p->repl ? "man() goes alone at the"
                " REPL's prompt, not inside a statement" : "man() is only"
                " available in the REPL (run ./pycee and type man(topic))"));
    if (type == TOK_FUNC && peek_is(p, 1, TOK_IDENT))
    {
        if (advance(p) == -1 || !(n = parse_func(p, 0)))
            return (NULL);
        n->as.func.nested = 1;
        return (n);
    }
    if (type == TOK_PROG)
        return (error_at(p, &p->cur, "'prog' only goes at the top level"));
    if (type != TOK_VAR && type != TOK_VEMP && type != TOK_CONST
        && type != TOK_IF && type != TOK_WHILE && type != TOK_DO
        && type != TOK_TRY && type != TOK_RAISE && type != TOK_ASSERT
        && type != TOK_FOR && type != TOK_SWITCH && type != TOK_RETURN
        && type != TOK_BREAK && type != TOK_CONTINUE)
        return (parse_simple(p));
    if (advance(p) == -1)
        return (NULL);
    switch (type)
    {
        case TOK_VAR:
        case TOK_VEMP:
        case TOK_CONST:
            return (parse_var(p, type));
        case TOK_DO:
            return (parse_do(p));
        case TOK_TRY:
            return (parse_try(p));
        case TOK_RAISE:
        case TOK_ASSERT:
            return (parse_raise(p, type));
        case TOK_IF:
            return (parse_if(p));
        case TOK_WHILE:
            return (parse_while(p));
        case TOK_FOR:
            return (parse_for(p));
        case TOK_SWITCH:
            return (parse_switch(p));
        case TOK_RETURN:
            return (parse_return(p));
        case TOK_BREAK:
            return (parse_jump(p, NODE_BREAK));
        default:
            return (parse_jump(p, NODE_CONTINUE));
    }
}

/* ---------- declarations ---------- */

/*
 * One parameter: [ "..." ] IDENT [ "=" expression ]. Once one has a
 * default every later one needs one too (otherwise a call could not
 * tell which argument goes where), and ...rest is always the last.
 */
static t_node   *parse_param(t_parser *p, t_node *func)
{
    t_node  *n;
    int     found;

    n = node_new(p, NODE_PARAM, p->cur.offset);
    if (match(p, TOK_ELLIPSIS, &n->as.param.is_rest) == -1)
        return (NULL);
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected parameter name but found %s",
                token_type_desc(p->cur.type)));
    n->as.param.name = token_name(p, &p->cur);
    if (advance(p) == -1 || match(p, TOK_ASSIGN, &found) == -1)
        return (NULL);
    if (found && n->as.param.is_rest)
        return (error_at(p, &p->prev, "'...%.*s' cannot have a default: it"
                " is an empty tuple when nothing is left",
                (int)n->as.param.name.len, n->as.param.name.ptr));
    if (found)
    {
        n->as.param.default_ = parse_expr(p, PREC_TERNARY);
        if (!n->as.param.default_)
            return (NULL);
    }
    else if (!n->as.param.is_rest
        && func->as.func.nrequired < func->as.func.nparams)
        return (error_at(p, &p->prev, "parameter '%.*s' needs a default:"
                " it comes after one that has a default",
                (int)n->as.param.name.len, n->as.param.name.ptr));
    else if (!n->as.param.is_rest)
        func->as.func.nrequired++;
    func->as.func.nparams++;
    func->as.func.has_rest = n->as.param.is_rest;
    return (node_end(p, n));
}

/* [ param { "," param } ] ")" ("(" already consumed) */
static int  parse_params(t_parser *p, t_node *func)
{
    t_node  **tail;
    t_node  *last;
    int     comma;

    tail = &func->as.func.params;
    last = NULL;
    comma = !check(p, TOK_RPAREN);
    while (comma)
    {
        if (last && last->as.param.is_rest)
        {
            error_at(p, &p->cur, "'...%.*s' must be the last parameter",
                (int)last->as.param.name.len, last->as.param.name.ptr);
            return (-1);
        }
        last = parse_param(p, func);
        if (!last)
            return (-1);
        *tail = last;
        tail = &last->next;
        if (match(p, TOK_COMMA, &comma) == -1)
            return (-1);
    }
    return (expect(p, TOK_RPAREN));
}

/* func / prog IDENT params block (keyword already read) */
static t_node   *parse_func(t_parser *p, int is_prog)
{
    t_node  *n;

    n = node_new(p, NODE_FUNC, p->prev.offset);
    n->as.func.is_prog = is_prog;
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected %s name but found %s",
                is_prog ? "prog" : "function", token_type_desc(p->cur.type)));
    n->as.func.name = token_name(p, &p->cur);
    if (advance(p) == -1 || expect(p, TOK_LPAREN) == -1
        || parse_params(p, n) == -1)
        return (NULL);
    n->as.func.body = parse_block(p);
    if (!n->as.func.body)
        return (NULL);
    return (node_end(p, n));
}

/* [ "private" ] [ "static" ] ( var_decl | func_decl ), in an obj. */
static t_node   *parse_member(t_parser *p, const t_node *obj)
{
    t_node  *m;
    int     priv;
    int     stat;

    if (match(p, TOK_PRIVATE, &priv) == -1 || match(p, TOK_STATIC, &stat)
        == -1)
        return (NULL);
    if (!check(p, TOK_VAR) && !check(p, TOK_FUNC))
        return (error_at(p, &p->cur, "expected 'var', 'func'%s or '}' in"
                " object body but found %s", priv || stat ? ""
                : ", 'private', 'static'", token_type_desc(p->cur.type)));
    if (advance(p) == -1)
        return (NULL);
    if (p->prev.type == TOK_VAR)
        m = parse_var(p, TOK_VAR);
    else
        m = parse_func(p, 0);
    if (!m)
        return (NULL);
    if (m->type == NODE_MULTI_VAR)
        return (error_at(p, &p->prev, "declare one field per 'var' in an"
                " object"));
    if (m->type == NODE_VAR)
    {
        m->as.var.is_private = priv;
        m->as.var.is_static = stat;
        m->as.var.cls = obj;
    }
    else
    {
        m->as.func.is_private = priv;
        m->as.func.is_static = stat;
        m->as.func.cls = obj;
    }
    return (m);
}

/*
 * obj IDENT [ "extends" IDENT | "(" IDENT ")" ] "{" { member } "}"
 * (keyword already read). Both forms of inheritance mean the same.
 */
static t_node   *parse_obj(t_parser *p)
{
    t_node  *n;
    t_node  **tail;
    char    *cname;
    int     found;
    int     paren;

    paren = 0;
    n = node_new(p, NODE_OBJ, p->prev.offset);
    if (!check(p, TOK_IDENT))
        return (error_at(p, &p->cur, "expected object name but found %s",
                token_type_desc(p->cur.type)));
    n->as.obj.name = token_name(p, &p->cur);
    cname = arena_alloc(p->arena, n->as.obj.name.len + 1);
    memcpy(cname, n->as.obj.name.ptr, n->as.obj.name.len);
    n->as.obj.cname = cname;
    if (advance(p) == -1 || match(p, TOK_EXTENDS, &found) == -1
        || (!found && match(p, TOK_LPAREN, &paren) == -1))
        return (NULL);
    if (found || paren)
    {
        if (!check(p, TOK_IDENT))
            return (error_at(p, &p->cur, "expected the object it extends but"
                    " found %s", token_type_desc(p->cur.type)));
        n->as.obj.parent = node_new(p, NODE_IDENT, p->cur.offset);
        n->as.obj.parent->as.ident.name = token_name(p, &p->cur);
        if (advance(p) == -1)
            return (NULL);
        node_end(p, n->as.obj.parent);
        if (!found && expect(p, TOK_RPAREN) == -1)
            return (NULL);
    }
    if (expect(p, TOK_LBRACE) == -1)
        return (NULL);
    tail = &n->as.obj.members;
    while (!check(p, TOK_RBRACE))
    {
        if (check(p, TOK_EOF))
            return (error_at(p, &p->cur, "expected '}' but found end of"
                    " file"));
        *tail = parse_member(p, n);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    if (advance(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* Reads an IDENT into *name, or fails with "expected <what>". */
static int  expect_name(t_parser *p, const char *what, t_name *name)
{
    if (!check(p, TOK_IDENT))
    {
        error_at(p, &p->cur, "expected %s but found %s", what,
            token_type_desc(p->cur.type));
        return (-1);
    }
    *name = token_name(p, &p->cur);
    return (advance(p));
}

/*
 * import_decl = "import" IDENT [ "as" IDENT ] ";"
 *             | "from" IDENT "import" IDENT [ "as" IDENT ] { "," ... } ";"
 * (the first keyword is already read)
 */
static t_node   *parse_import(t_parser *p, int is_from)
{
    t_node  *n;
    t_node  **tail;
    int     found;

    n = node_new(p, NODE_IMPORT, p->prev.offset);
    n->as.import.is_from = is_from;
    if (expect_name(p, "module name", &n->as.import.module) == -1)
        return (NULL);
    if (!is_from)
    {
        if (match(p, TOK_AS, &found) == -1)
            return (NULL);
        if (found && expect_name(p, "name after 'as'",
                &n->as.import.alias) == -1)
            return (NULL);
    }
    else
    {
        if (expect(p, TOK_IMPORT) == -1)
            return (NULL);
        if (check(p, TOK_STAR))
            return (error_at(p, &p->cur, "'from ... import *' is not"
                    " supported: list the names to import"));
        tail = &n->as.import.names;
        found = 1;
        while (found)
        {
            *tail = node_new(p, NODE_IDENT, p->cur.offset);
            if (expect_name(p, "name to import",
                    &(*tail)->as.ident.name) == -1
                || match(p, TOK_AS, &found) == -1)
                return (NULL);
            if (found && expect_name(p, "name after 'as'",
                    &(*tail)->as.ident.alias) == -1)
                return (NULL);
            node_end(p, *tail);
            tail = &(*tail)->next;
            if (match(p, TOK_COMMA, &found) == -1)
                return (NULL);
        }
    }
    if (expect_semicolon(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/* top_decl = func_decl | prog_decl | obj_decl | var_decl | const_decl */
static t_node   *parse_top_decl(t_parser *p)
{
    t_token_type    type;

    type = p->cur.type;
    if (type == TOK_IMPORT || type == TOK_FROM)
        return (error_at(p, &p->cur, "imports must go at the beginning of"
                " the file, before any other declaration"));
    if (type == TOK_VEMP)
        return (error_at(p, &p->cur, "'vemp' is only allowed inside"
                " functions; use 'var' for globals"));
    if (type == TOK_MAN)
        return (error_at(p, &p->cur, "man() is only available in the REPL"
                " (run ./pycee and type man(topic))"));
    if (type != TOK_FUNC && type != TOK_PROG && type != TOK_OBJ
        && type != TOK_VAR && type != TOK_CONST)
        return (error_at(p, &p->cur, "expected a declaration (func, prog,"
                " obj, var or const) but found %s", token_type_desc(type)));
    if (advance(p) == -1)
        return (NULL);
    if (type == TOK_FUNC)
        return (parse_func(p, 0));
    if (type == TOK_PROG)
        return (parse_func(p, 1));
    if (type == TOK_OBJ)
        return (parse_obj(p));
    return (parse_var(p, type));
}

/* ---------- entry point ---------- */

static int  parser_init(t_parser *p, t_arena *arena, const t_source *src,
                t_error *err)
{
    p->src = src;
    p->cur.type = TOK_EOF;
    p->cur.offset = src->base;
    p->cur.len = 0;
    p->last_end = src->base;
    p->arena = arena;
    p->err = err;
    p->repl = 0;
    lexer_init(&p->lx, src);
    return (advance(p));
}

t_node  *parse_program(t_arena *arena, const t_source *src, t_error *err)
{
    t_parser    p;
    t_node      *root;
    t_node      **tail;

    if (parser_init(&p, arena, src, err) == -1)
        return (NULL);
    root = node_new(&p, NODE_PROGRAM, src->base);
    tail = &root->as.list.items;
    while (check(&p, TOK_IMPORT) || check(&p, TOK_FROM))
    {
        if (advance(&p) == -1)
            return (NULL);
        *tail = parse_import(&p, p.prev.type == TOK_FROM);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    while (!check(&p, TOK_EOF))
    {
        *tail = parse_top_decl(&p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    root->len = src->len;
    return (root);
}

/*
 * man(topic), alone at the REPL's prompt: the manual. The topic is the
 * text between the parentheses (spaces around it dropped), whatever it
 * is: a name, type.method, a keyword, an operator... It is never
 * evaluated: looking it up is the manual's job.
 */
static t_node   *parse_man(t_parser *p)
{
    t_node      *n;
    const char  *start;
    const char  *end;
    int         depth;

    n = node_new(p, NODE_MAN, p->cur.offset);
    if (advance(p) == -1 || expect(p, TOK_LPAREN) == -1)
        return (NULL);
    start = tok_text(p, &p->cur);
    end = start;
    depth = 0;
    while (!(depth == 0 && check(p, TOK_RPAREN)))
    {
        if (check(p, TOK_EOF))
            return (error_at(p, &p->cur, "expected ')' to end man(...)"
                    " but found end of file"));
        depth += check(p, TOK_LPAREN) - check(p, TOK_RPAREN);
        end = tok_text(p, &p->cur) + p->cur.len;
        if (advance(p) == -1)
            return (NULL);
    }
    n->as.man.topic.ptr = start;
    n->as.man.topic.len = (size_t)(end - start);
    if (advance(p) == -1)
        return (NULL);
    if (check(p, TOK_SEMICOLON) && advance(p) == -1)
        return (NULL);
    return (node_end(p, n));
}

/*
 * One thing typed in the REPL: an import, a declaration or any
 * statement (control statements too, unlike the top level of a file).
 */
static t_node   *parse_repl_item(t_parser *p)
{
    t_token_type    type;

    type = p->cur.type;
    if (type == TOK_MAN)
        return (parse_man(p));
    if (type == TOK_IMPORT || type == TOK_FROM)
    {
        if (advance(p) == -1)
            return (NULL);
        return (parse_import(p, type == TOK_FROM));
    }
    if ((type == TOK_FUNC && !peek_is(p, 1, TOK_LPAREN)) || type == TOK_PROG
        || type == TOK_OBJ || type == TOK_VAR || type == TOK_VEMP
        || type == TOK_CONST)
        return (parse_top_decl(p));
    return (parse_statement(p));
}

t_node  *parse_repl(t_arena *arena, const t_source *src, t_error *err)
{
    t_parser    p;
    t_node      *root;
    t_node      **tail;

    if (parser_init(&p, arena, src, err) == -1)
        return (NULL);
    p.repl = 1;
    root = node_new(&p, NODE_PROGRAM, src->base);
    tail = &root->as.list.items;
    while (!check(&p, TOK_EOF))
    {
        *tail = parse_repl_item(&p);
        if (!*tail)
            return (NULL);
        tail = &(*tail)->next;
    }
    root->len = src->len;
    return (root);
}
