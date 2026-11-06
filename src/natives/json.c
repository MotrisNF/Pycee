/*
 * Module json: JSON text (RFC 8259) to values and back. parse gives
 * dicts, lists, strings, numbers, bools and none; dump writes them, with
 * tuples as lists, in one line as Python does ({"a": 1, "b": [1, 2]}) or
 * indented. Text stays UTF-8: nothing is turned into \uXXXX on the way
 * out.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "core/stack.h"
#include "core/utf8.h"
#include "natives/modules.h"

/* ---------- parse ---------- */

typedef struct s_json
{
    const char      *s;
    size_t          len;
    size_t          pos;
    const t_node    *call;
    t_error         *err;
    int             depth;
    int             max_depth;
}   t_json;

/*
 * A ValueError "json.parse(): <what> at line L, column C" where the text
 * stopped making sense; the column counts characters, from 1.
 */
static int  fail(t_json *j, const char *what)
{
    size_t  line;
    size_t  col;
    size_t  i;

    line = 1;
    col = 1;
    for (i = 0; i < j->pos && i < j->len; i++)
    {
        if (j->s[i] == '\n')
        {
            line++;
            col = 1;
        }
        else if (((unsigned char)j->s[i] & 0xC0) != 0x80)
            col++;
    }
    return (rt_error(j->err, ERR_VALUE, j->call, "json.parse(): %s at line"
            " %zu, column %zu", what, line, col));
}

static void skip_space(t_json *j)
{
    while (j->pos < j->len && (j->s[j->pos] == ' ' || j->s[j->pos] == '\t'
            || j->s[j->pos] == '\n' || j->s[j->pos] == '\r'))
        j->pos++;
}

static int  peek(const t_json *j)
{
    return (j->pos < j->len ? (unsigned char)j->s[j->pos] : -1);
}

/* What was found, for "expected X but found Y" messages. */
static int  unexpected(t_json *j, const char *expected)
{
    char    msg[96];
    int     c;

    c = peek(j);
    if (c == -1)
        snprintf(msg, sizeof(msg), "expected %s but the text ended",
            expected);
    else if (c >= 0x20 && c < 0x7F)
        snprintf(msg, sizeof(msg), "expected %s but found '%c'", expected,
            c);
    else
        snprintf(msg, sizeof(msg), "expected %s", expected);
    return (fail(j, msg));
}

static int  parse_value(t_interp *in, t_json *j, t_value *out);

static int  hex4(t_json *j, int32_t *out)
{
    int32_t v;
    int     c;
    int     i;

    v = 0;
    for (i = 0; i < 4; i++)
    {
        c = peek(j);
        if (c >= '0' && c <= '9')
            v = v * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f')
            v = v * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v = v * 16 + (c - 'A' + 10);
        else
            return (fail(j, "a \\u escape needs 4 hexadecimal digits"));
        j->pos++;
    }
    *out = v;
    return (0);
}

/*
 * \uXXXX, and the second half of a surrogate pair when the first one
 * needs it. Half a pair alone has no character: U+FFFD, as for any text
 * that is not valid.
 */
static int  unicode_escape(t_json *j, t_buffer *b)
{
    char    enc[4];
    int32_t cp;
    int32_t low;
    size_t  save;

    if (hex4(j, &cp) == -1)
        return (-1);
    if (cp >= 0xD800 && cp <= 0xDBFF)
    {
        save = j->pos;
        if (j->pos + 1 < j->len && j->s[j->pos] == '\\'
            && j->s[j->pos + 1] == 'u')
        {
            j->pos += 2;
            if (hex4(j, &low) == -1)
                return (-1);
            if (low >= 0xDC00 && low <= 0xDFFF)
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            else
                j->pos = save;
        }
    }
    if (cp >= 0xD800 && cp <= 0xDFFF)
        cp = 0xFFFD;
    buf_add(b, enc, (size_t)utf8_encode(cp, enc));
    return (0);
}

static int  escape(t_json *j, t_buffer *b)
{
    int c;

    c = peek(j);
    j->pos++;
    if (c == '"' || c == '\\' || c == '/')
        buf_add_char(b, (char)c);
    else if (c == 'b')
        buf_add_char(b, '\b');
    else if (c == 'f')
        buf_add_char(b, '\f');
    else if (c == 'n')
        buf_add_char(b, '\n');
    else if (c == 'r')
        buf_add_char(b, '\r');
    else if (c == 't')
        buf_add_char(b, '\t');
    else if (c == 'u')
        return (unicode_escape(j, b));
    else
    {
        j->pos--;
        return (fail(j, "invalid escape in a string"));
    }
    return (0);
}

/* "...": its text, from a buffer of the session, into a new string. */
static int  parse_string(t_interp *in, t_json *j, t_value *out)
{
    t_buffer    *b;
    int         c;
    int         status;

    j->pos++;
    b = rt_buf_take(in);
    status = 0;
    while (status == 0)
    {
        c = peek(j);
        if (c == -1)
            status = fail(j, "a string is not closed");
        else if (c == '"')
            break ;
        else if (c < 0x20)
            status = fail(j, "a control character inside a string");
        else if (c == '\\')
        {
            j->pos++;
            status = escape(j, b);
        }
        else
        {
            buf_add_char(b, (char)c);
            j->pos++;
        }
    }
    if (status == 0)
    {
        j->pos++;
        *out = val_obj(&rt_new_string(in, b->data ? b->data : "",
                    b->len)->obj);
    }
    rt_buf_give(in);
    return (status);
}

/*
 * -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?: an int without a
 * fraction or an exponent (OverflowError past 64 bits), a float
 * otherwise (OverflowError past what a float holds: no inf from JSON).
 */
static int  parse_number(t_json *j, t_value *out)
{
    char    text[400];
    size_t  start;
    int     is_float;
    double  f;

    start = j->pos;
    is_float = 0;
    if (peek(j) == '-')
        j->pos++;
    if (peek(j) == '0')
        j->pos++;
    else if (peek(j) >= '1' && peek(j) <= '9')
        while (peek(j) >= '0' && peek(j) <= '9')
            j->pos++;
    else
        return (unexpected(j, "a digit"));
    if (peek(j) == '.')
    {
        is_float = 1;
        j->pos++;
        if (!(peek(j) >= '0' && peek(j) <= '9'))
            return (unexpected(j, "a digit after '.'"));
        while (peek(j) >= '0' && peek(j) <= '9')
            j->pos++;
    }
    if (peek(j) == 'e' || peek(j) == 'E')
    {
        is_float = 1;
        j->pos++;
        if (peek(j) == '+' || peek(j) == '-')
            j->pos++;
        if (!(peek(j) >= '0' && peek(j) <= '9'))
            return (unexpected(j, "a digit in the exponent"));
        while (peek(j) >= '0' && peek(j) <= '9')
            j->pos++;
    }
    if (j->pos - start >= sizeof(text))
        return (fail(j, "a number too long"));
    memcpy(text, j->s + start, j->pos - start);
    text[j->pos - start] = '\0';
    errno = 0;
    if (!is_float)
    {
        *out = val_int(strtoll(text, NULL, 10));
        if (errno == ERANGE)
            return (rt_error(j->err, ERR_OVERFLOW, j->call, "json.parse(): %s"
                    " does not fit in an int", text));
        return (0);
    }
    f = strtod(text, NULL);
    if (errno == ERANGE && (f > 1 || f < -1))
        return (rt_error(j->err, ERR_OVERFLOW, j->call, "json.parse(): %s is"
                " too large for a float", text));
    *out = val_float(f);
    return (0);
}

static int  parse_word(t_json *j, const char *word, t_value v, t_value *out)
{
    size_t  n;

    n = strlen(word);
    if (j->len - j->pos < n || memcmp(j->s + j->pos, word, n) != 0)
        return (unexpected(j, "a value"));
    j->pos += n;
    *out = v;
    return (0);
}

/* [a, b...]: the list grows at stack[slot] (rooted) as items come. */
static int  parse_array(t_interp *in, t_json *j, t_value *out)
{
    t_list  *l;
    t_value v;
    size_t  slot;

    j->pos++;
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    skip_space(j);
    if (peek(j) == ']')
        j->pos++;
    else
    {
        while (1)
        {
            if (parse_value(in, j, &v) == -1)
                return (-1);
            rt_push(in, v);
            l = val_list(in->stack[slot]);
            heap_list_reserve(&in->heap, l, l->len + 1);
            l->items[l->len++] = rt_pop(in);
            skip_space(j);
            if (peek(j) == ']')
                break ;
            if (peek(j) != ',')
                return (unexpected(j, "',' or ']'"));
            j->pos++;
        }
        j->pos++;
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/* {"k": v...}: a repeated key keeps its last value, as in Python. */
static int  parse_object(t_interp *in, t_json *j, t_value *out)
{
    t_value v;
    size_t  slot;

    j->pos++;
    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_DICT)->obj));
    skip_space(j);
    if (peek(j) == '}')
        j->pos++;
    else
    {
        while (1)
        {
            skip_space(j);
            if (peek(j) != '"')
                return (unexpected(j, "a key (a string)"));
            if (parse_string(in, j, &v) == -1)
                return (-1);
            rt_push(in, v);
            skip_space(j);
            if (peek(j) != ':')
                return (unexpected(j, "':'"));
            j->pos++;
            if (parse_value(in, j, &v) == -1)
                return (-1);
            rt_push(in, v);
            if (rt_table_put(in, j->call, slot, in->stack[slot + 1],
                    in->stack[slot + 2], j->err) == -1)
                return (-1);
            in->top = slot + 1;
            skip_space(j);
            if (peek(j) == '}')
                break ;
            if (peek(j) != ',')
                return (unexpected(j, "',' or '}'"));
            j->pos++;
        }
        j->pos++;
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/*
 * An array or an object: no deeper than the recursion limit, as in
 * Python (the real stack guard still protects a limit set very high).
 */
static int  nested(t_interp *in, t_json *j, int object, t_value *out)
{
    int status;

    if (j->depth >= j->max_depth || stack_exhausted())
        return (rt_error(j->err, ERR_RECURSION, j->call, "json.parse(): the"
                " text is nested deeper than the recursion limit (%d)",
                j->max_depth));
    j->depth++;
    status = object ? parse_object(in, j, out) : parse_array(in, j, out);
    j->depth--;
    return (status);
}

static int  parse_value(t_interp *in, t_json *j, t_value *out)
{
    int c;

    skip_space(j);
    c = peek(j);
    if (c == '{')
        return (nested(in, j, 1, out));
    if (c == '[')
        return (nested(in, j, 0, out));
    if (c == '"')
        return (parse_string(in, j, out));
    if (c == '-' || (c >= '0' && c <= '9'))
        return (parse_number(j, out));
    if (c == 't')
        return (parse_word(j, "true", val_bool(1), out));
    if (c == 'f')
        return (parse_word(j, "false", val_bool(0), out));
    if (c == 'n')
        return (parse_word(j, "null", val_none(), out));
    return (unexpected(j, "a value"));
}

/* parse(text): the value the JSON text holds; ValueError where it is wrong. */
int nat_json_parse(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    t_json  j;

    (void)nargs;
    if (!val_is_string(ARG(0)))
        return (rt_error(err, ERR_TYPE, call, "parse() needs a string, not"
                " '%s'", val_type_name(ARG(0))));
    j.s = val_string(ARG(0))->chars;
    j.len = val_string(ARG(0))->len;
    j.pos = 0;
    j.call = call;
    j.err = err;
    j.depth = 0;
    j.max_depth = in->max_depth;
    if (parse_value(in, &j, out) == -1)
        return (-1);
    skip_space(&j);
    if (j.pos < j.len)
        return (fail(&j, "unexpected text after the value"));
    return (0);
}

/* ---------- dump ---------- */

/* The containers being written, innermost first: one met again is a cycle. */
typedef struct s_seen
{
    const t_obj         *o;
    const struct s_seen *up;
}   t_seen;

typedef struct s_dump
{
    t_buffer        *b;
    int64_t         indent;     /* -1: one line */
    const t_node    *call;
    t_error         *err;
    int             max_depth;
}   t_dump;

static void dump_string(t_buffer *b, const t_string *s)
{
    unsigned char   c;
    size_t          i;

    buf_add_char(b, '"');
    for (i = 0; i < s->len; i++)
    {
        c = (unsigned char)s->chars[i];
        if (c == '"' || c == '\\')
        {
            buf_add_char(b, '\\');
            buf_add_char(b, (char)c);
        }
        else if (c == '\n')
            buf_add_str(b, "\\n");
        else if (c == '\r')
            buf_add_str(b, "\\r");
        else if (c == '\t')
            buf_add_str(b, "\\t");
        else if (c == '\b')
            buf_add_str(b, "\\b");
        else if (c == '\f')
            buf_add_str(b, "\\f");
        else if (c < 0x20)
            buf_printf(b, "\\u%04x", c);
        else
            buf_add_char(b, (char)c);
    }
    buf_add_char(b, '"');
}

/* Before an item: a new line and the indentation of level, when indenting. */
static void new_line(t_dump *d, int level)
{
    int64_t i;

    if (d->indent < 0)
        return ;
    buf_add_char(d->b, '\n');
    for (i = 0; i < d->indent * level; i++)
        buf_add_char(d->b, ' ');
}

static int  dump_value(t_dump *d, t_value v, int level, const t_seen *seen);

static int  enter(t_dump *d, const t_obj *o, int level, const t_seen *seen)
{
    for (; seen; seen = seen->up)
        if (seen->o == o)
            return (rt_error(d->err, ERR_VALUE, d->call, "dump(): circular"
                    " reference (a value that contains itself)"));
    if (level >= d->max_depth || stack_exhausted())
        return (rt_error(d->err, ERR_RECURSION, d->call, "dump(): the value"
                " is nested deeper than the recursion limit (%d)",
                d->max_depth));
    return (0);
}

static int  dump_items(t_dump *d, t_value v, int level, const t_seen *seen)
{
    const t_value   *items;
    t_seen          here;
    size_t          len;
    size_t          i;

    if (enter(d, v.as.obj, level, seen) == -1)
        return (-1);
    here.o = v.as.obj;
    here.up = seen;
    items = rt_seq_items(v, &len);
    buf_add_char(d->b, '[');
    for (i = 0; i < len; i++)
    {
        if (i > 0)
            buf_add_str(d->b, d->indent < 0 ? ", " : ",");
        new_line(d, level + 1);
        if (dump_value(d, items[i], level + 1, &here) == -1)
            return (-1);
    }
    if (len > 0)
        new_line(d, level);
    buf_add_char(d->b, ']');
    return (0);
}

static int  dump_dict(t_dump *d, t_value v, int level, const t_seen *seen)
{
    const t_dict    *t;
    t_seen          here;
    size_t          i;
    size_t          n;

    if (enter(d, v.as.obj, level, seen) == -1)
        return (-1);
    here.o = v.as.obj;
    here.up = seen;
    t = val_dict(v);
    buf_add_char(d->b, '{');
    n = 0;
    for (i = 0; i < t->nentries; i++)
    {
        if (!t->entries[i].used)
            continue ;
        if (!val_is_string(t->entries[i].key))
            return (rt_error(d->err, ERR_TYPE, d->call, "dump(): the keys"
                    " of a dict must be strings in JSON, not '%s'",
                    val_type_name(t->entries[i].key)));
        if (n++ > 0)
            buf_add_str(d->b, d->indent < 0 ? ", " : ",");
        new_line(d, level + 1);
        dump_string(d->b, val_string(t->entries[i].key));
        buf_add_str(d->b, ": ");
        if (dump_value(d, t->entries[i].value, level + 1, &here) == -1)
            return (-1);
    }
    if (n > 0)
        new_line(d, level);
    buf_add_char(d->b, '}');
    return (0);
}

/* Nothing here allocates on the heap: no collection can run meanwhile. */
static int  dump_value(t_dump *d, t_value v, int level, const t_seen *seen)
{
    if (v.type == VAL_NONE)
        buf_add_str(d->b, "null");
    else if (v.type == VAL_BOOL)
        buf_add_str(d->b, v.as.boolean ? "true" : "false");
    else if (v.type == VAL_INT)
        buf_printf(d->b, "%lld", (long long)v.as.integer);
    else if (v.type == VAL_FLOAT)
    {
        if (v.as.number != v.as.number || v.as.number - v.as.number != 0)
            return (rt_error(d->err, ERR_VALUE, d->call, "dump(): %s cannot"
                    " be written in JSON", v.as.number != v.as.number ? "nan"
                    : v.as.number < 0 ? "-inf" : "inf"));
        rt_format_float(d->b, v.as.number);
    }
    else if (val_is_string(v))
        dump_string(d->b, val_string(v));
    else if (val_is_list(v) || val_is_tuple(v))
        return (dump_items(d, v, level, seen));
    else if (val_is_dict(v))
        return (dump_dict(d, v, level, seen));
    else
        return (rt_error(d->err, ERR_TYPE, d->call, "dump(): a '%s' cannot be"
                " written in JSON (only dicts, lists, tuples, strings,"
                " numbers, bools and none)", val_type_name(v)));
    return (0);
}

/*
 * dump(value) / dump(value, indent = n): the JSON text of value, in one
 * line, or with each item on its own line, n spaces deeper per level.
 */
int nat_json_dump(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    t_dump  d;
    int     status;

    d.indent = -1;
    if (KW(0).type != VAL_NONE)
    {
        if (rt_int_arg(call, KW(0), "dump", "indent", &d.indent, err) == -1)
            return (-1);
        if (d.indent < 0 || d.indent > 64)
            return (rt_error(err, ERR_VALUE, call, "dump() needs an indent"
                    " from 0 to 64, not %lld", (long long)d.indent));
    }
    d.b = rt_buf_take(in);
    d.call = call;
    d.err = err;
    d.max_depth = in->max_depth;
    status = dump_value(&d, ARG(0), 0, NULL);
    if (status == 0)
        *out = val_obj(&rt_new_string(in, d.b->data ? d.b->data : "",
                    d.b->len)->obj);
    rt_buf_give(in);
    return (status);
}
