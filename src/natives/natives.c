/*
 * Global native functions, one per line of NATIVE_LIST
 * (resolver/natives.h). The resolver has already checked their arity.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "resolver/natives.h"
#include "runtime/interp_internal.h"

#define ARG(i)  (in->stack[base + (i)])

/* print(a, b...): plain text, separated by a space, then a newline. */
static int  native_print(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_buffer    *b;
    int         i;

    b = rt_buf_take(in);
    for (i = 0; i < nargs; i++)
    {
        if (i > 0)
            buf_add_char(b, ' ');
        if (rt_show(in, call, b, ARG(i), 0, err) == -1)
        {
            rt_buf_give(in);
            return (-1);
        }
    }
    buf_add_char(b, '\n');
    fwrite(b->data, 1, b->len, stdout);
    rt_buf_give(in);
    *out = val_none();
    return (0);
}

static int  native_str(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_string    *s;
    t_buffer    *b;

    (void)nargs;
    if (val_is_string(ARG(0)))
    {
        *out = ARG(0);
        return (0);
    }
    b = rt_buf_take(in);
    if (rt_show(in, call, b, ARG(0), 0, err) == -1)
    {
        rt_buf_give(in);
        return (-1);
    }
    s = heap_string(&in->heap, b->data ? b->data : "", b->len);
    rt_buf_give(in);
    *out = val_obj(&s->obj);
    return (0);
}

static int  is_space(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v');
}

static int  is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

/*
 * The text of a string argument without surrounding whitespace and
 * without its '_' separators, into in->text. Returns 0 if a '_' is not
 * between two digits (as in literals) or the text has a '\0' inside.
 */
static int  clean_number_text(t_interp *in, const t_string *s)
{
    size_t  start;
    size_t  end;
    size_t  i;

    start = 0;
    end = s->len;
    while (start < end && is_space(s->chars[start]))
        start++;
    while (end > start && is_space(s->chars[end - 1]))
        end--;
    buf_clear(&in->text);
    for (i = start; i < end; i++)
    {
        if (s->chars[i] == '\0')
            return (0);
        if (s->chars[i] != '_')
            buf_add_char(&in->text, s->chars[i]);
        else if (i == start || i + 1 >= end || !is_digit(s->chars[i - 1])
            || !is_digit(s->chars[i + 1]))
            return (0);
    }
    return (in->text.len > 0);
}

/*
 * [+-] digits, in in->text: 1 with the value, 0 if it is not an int,
 * -1 if it is one but does not fit in 64 bits.
 */
static int  parse_int_text(const char *s, int64_t *out)
{
    uint64_t    value;
    uint64_t    limit;
    int         negative;

    negative = (*s == '-');
    if (*s == '-' || *s == '+')
        s++;
    if (!is_digit(*s))
        return (0);
    limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    value = 0;
    for (; is_digit(*s); s++)
    {
        if (value > (limit - (uint64_t)(*s - '0')) / 10)
            return (-1);
        value = value * 10 + (uint64_t)(*s - '0');
    }
    if (*s)
        return (0);
    *out = negative ? (int64_t)(0 - value) : (int64_t)value;
    return (1);
}

/*
 * A decimal float, or inf / nan (any case, with a sign), in in->text.
 * strtod alone would also take hex floats and "infinity"-like prefixes,
 * so the shape is checked first.
 */
static int  parse_float_text(const char *s, double *out)
{
    const char  *p;
    int         digits;

    p = s + (*s == '-' || *s == '+');
    if (!strcasecmp(p, "inf") || !strcasecmp(p, "nan"))
    {
        *out = strtod(s, NULL);
        return (1);
    }
    digits = 0;
    while (is_digit(*p) && ++digits)
        p++;
    if (*p == '.')
        p++;
    while (is_digit(*p) && ++digits)
        p++;
    if (!digits)
        return (0);
    if (*p == 'e' || *p == 'E')
    {
        p++;
        p += (*p == '-' || *p == '+');
        if (!is_digit(*p))
            return (0);
        while (is_digit(*p))
            p++;
    }
    if (*p)
        return (0);
    *out = strtod(s, NULL);
    return (1);
}

static int  invalid_literal(t_interp *in, const t_node *call,
                const char *what, t_value v, t_error *err)
{
    buf_clear(&in->text);
    rt_format(&in->text, v, 1);
    if (in->text.len > 60)
    {
        in->text.len = 57;
        buf_add_str(&in->text, "...");
    }
    return (rt_error(err, ERR_VALUE, call, "invalid literal for %s(): %s",
            what, in->text.data));
}

static int  not_a_number(const t_node *call, const char *what, t_value v,
                t_error *err)
{
    return (rt_error(err, ERR_TYPE, call, "%s() needs a number or a string,"
            " not '%s'", what, val_type_name(v)));
}

/* int(x): truncates a float, parses a string of digits. */
static int  native_int(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    int64_t i;
    int     r;
    double  f;

    (void)nargs;
    if (ARG(0).type == VAL_INT)
        *out = ARG(0);
    else if (ARG(0).type == VAL_FLOAT)
    {
        f = trunc(ARG(0).as.number);
        if (f != f)
            return (rt_error(err, ERR_VALUE, call, "cannot convert nan to an"
                    " int"));
        if (f >= 9223372036854775808.0 || f < -9223372036854775808.0)
            return (rt_error(err, ERR_OVERFLOW, call, "the number is too large"
                    " for an int"));
        *out = val_int((int64_t)f);
    }
    else if (!val_is_string(ARG(0)))
        return (not_a_number(call, "int", ARG(0), err));
    else
    {
        r = clean_number_text(in, val_string(ARG(0)))
            ? parse_int_text(in->text.data, &i) : 0;
        if (r == -1)
            return (rt_error(err, ERR_OVERFLOW, call, "the number is too large"
                    " for an int"));
        if (r == 0)
            return (invalid_literal(in, call, "int", ARG(0), err));
        *out = val_int(i);
    }
    return (0);
}

/* float(x): from an int, a float or the text of a number. */
static int  native_float(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    double  f;

    (void)nargs;
    if (ARG(0).type == VAL_INT)
        *out = val_float((double)ARG(0).as.integer);
    else if (ARG(0).type == VAL_FLOAT)
        *out = ARG(0);
    else if (!val_is_string(ARG(0)))
        return (not_a_number(call, "float", ARG(0), err));
    else if (!clean_number_text(in, val_string(ARG(0)))
        || !parse_float_text(in->text.data, &f))
        return (invalid_literal(in, call, "float", ARG(0), err));
    else
        *out = val_float(f);
    return (0);
}

/* num(x): an int if the text is an int, a float if it is a float. */
static int  native_num(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    int64_t i;
    double  f;
    int     r;

    (void)nargs;
    if (ARG(0).type == VAL_INT || ARG(0).type == VAL_FLOAT)
    {
        *out = ARG(0);
        return (0);
    }
    if (!val_is_string(ARG(0)))
        return (not_a_number(call, "num", ARG(0), err));
    if (!clean_number_text(in, val_string(ARG(0))))
        return (invalid_literal(in, call, "num", ARG(0), err));
    r = parse_int_text(in->text.data, &i);
    if (r == -1)
        return (rt_error(err, ERR_OVERFLOW, call, "the number is too large"
                " for an int"));
    if (r == 1)
        *out = val_int(i);
    else if (parse_float_text(in->text.data, &f))
        *out = val_float(f);
    else
        return (invalid_literal(in, call, "num", ARG(0), err));
    return (0);
}

/*
 * input() / input(prompt): a line without its end. At the end of input,
 * EOFError; a terminal (Ctrl+D) is then read again by the next call, as
 * in Python, while a file or a pipe stays at its end.
 */
static int  native_input(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_string    *s;
    t_buffer    *b;
    ssize_t     len;
    int         status;

    if (nargs == 1)
    {
        b = rt_buf_take(in);
        status = rt_show(in, call, b, ARG(0), 0, err);
        fwrite(b->data, 1, b->len, stdout);
        rt_buf_give(in);
        if (status == -1)
            return (-1);
    }
    fflush(stdout);
    errno = 0;
    len = getline(&in->line, &in->line_cap, stdin);
    if (len == -1 && errno == ENOMEM)
        fatal_oom();
    if (len == -1 && isatty(STDIN_FILENO))
        clearerr(stdin);
    if (len == -1)
        return (rt_error(err, ERR_EOF, call, "end of input"));
    if (len > 0 && in->line[len - 1] == '\n')
        len--;
    if (len > 0 && in->line[len - 1] == '\r')
        len--;
    s = rt_new_string(in, in->line, (size_t)len);
    *out = val_obj(&s->obj);
    return (0);
}

/*
 * range(stop), range(start, stop), range(start, stop, step), as in
 * Python: integers only, and a step of 0 would never end.
 */
static int  native_range(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_range *r;
    int64_t bounds[3];
    int     i;

    for (i = 0; i < nargs; i++)
    {
        if (ARG(i).type == VAL_FLOAT)
            return (rt_error(err, ERR_TYPE, call, "range() needs integers,"
                    " not a float"));
        if (ARG(i).type != VAL_INT)
            return (rt_error(err, ERR_TYPE, call, "range() needs integers,"
                    " not '%s'", val_type_name(ARG(i))));
        bounds[i] = ARG(i).as.integer;
    }
    if (nargs == 3 && bounds[2] == 0)
        return (rt_error(err, ERR_VALUE, call, "range() step must not be"
                " zero"));
    r = (t_range *)heap_alloc(&in->heap, OBJ_RANGE, sizeof(t_range));
    r->start = nargs == 1 ? 0 : bounds[0];
    r->stop = nargs == 1 ? bounds[0] : bounds[1];
    r->step = nargs == 3 ? bounds[2] : 1;
    *out = val_obj(&r->obj);
    return (0);
}

/* tuple(a, b...): its arguments, in order (they stay rooted until it exists). */
static int  native_tuple(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_tuple *t;
    int     i;

    (void)call;
    (void)err;
    t = heap_tuple(&in->heap, (size_t)nargs);
    for (i = 0; i < nargs; i++)
        t->items[i] = ARG(i);
    *out = val_obj(&t->obj);
    return (0);
}

/* set(a, b...): its arguments once each (a repeated one is kept once). */
static int  native_set(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    size_t  slot;
    int     i;

    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_SET)->obj));
    for (i = 0; i < nargs; i++)
        if (rt_table_put(in, call, slot, ARG(i), val_none(), err) == -1)
            return (-1);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/*
 * dict("a": 1, "b": 2): each key: value pair comes as two arguments in a
 * row (the resolver only lets pairs into dict). A repeated key keeps its
 * last value, in the place of its first.
 */
static int  native_dict(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    size_t  slot;
    int     i;

    if (nargs % 2)
        return (rt_error(err, ERR_TYPE, call, "dict() takes key: value"
                " pairs"));
    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_DICT)->obj));
    for (i = 0; i < nargs; i += 2)
        if (rt_table_put(in, call, slot, ARG(i), ARG(i + 1), err) == -1)
            return (-1);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/*
 * any(x), all(x), sum(x), min(x), max(x): the method of that name on
 * the items a for over x gives (a list or a tuple as it is). With a
 * comprehension inside, any(x > 0 for (x in l)), the call never gets
 * here: the resolver makes it a comprehension of its own (eval.c).
 */
static int  on_items(t_interp *in, size_t base, const char *method,
                const t_node *call, t_value *out, t_error *err)
{
    t_name  name;
    size_t  top;
    int     status;

    top = in->top;
    if (val_is_list(ARG(0)) || val_is_tuple(ARG(0)))
        rt_push(in, ARG(0));
    else if (rt_collect(in, call, ARG(0), out, err) == -1)
        return (-1);
    else
        rt_push(in, *out);
    name.ptr = method;
    name.len = strlen(method);
    status = rt_call_method(in, call, name, top, 0, out, err);
    in->top = top;
    return (status);
}

static int  native_any(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (on_items(in, base, "any", call, out, err));
}

static int  native_all(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (on_items(in, base, "all", call, out, err));
}

static int  native_sum(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (on_items(in, base, "sum", call, out, err));
}

static int  native_min(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (on_items(in, base, "min", call, out, err));
}

static int  native_max(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (on_items(in, base, "max", call, out, err));
}

/*
 * exit() / exit(code), only in the REPL: ends the session with that
 * status. It travels up as a SystemExit, so every frame on the way is
 * unwound and the session frees everything before leaving.
 */
static int  native_exit(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    int64_t code;

    (void)out;
    code = 0;
    if (nargs == 1)
    {
        if (ARG(0).type == VAL_FLOAT)
            return (rt_error(err, ERR_TYPE, call, "exit() needs an integer"
                    " code, not a float"));
        if (ARG(0).type != VAL_INT)
            return (rt_error(err, ERR_TYPE, call, "exit() needs an integer"
                    " code, not '%s'", val_type_name(ARG(0))));
        code = ARG(0).as.integer;
        if (code < 0 || code > 255)
            return (rt_error(err, ERR_VALUE, call, "exit code must be"
                    " between 0 and 255, not %lld", (long long)code));
    }
    rt_error(err, ERR_EXIT, call, "exit");
    err->code = (int)code;
    return (-1);
}

/*
 * Error("msg"), ValueError("msg")...: an error not raised yet, made
 * where the call is. The message is the argument as text (none: empty).
 */
static int  make_error(t_interp *in, size_t base, int nargs,
                const t_node *call, const char *type, t_value *out,
                t_error *err)
{
    t_error_value   *e;
    t_buffer        *b;

    b = rt_buf_take(in);
    if (nargs == 1 && rt_show(in, call, b, ARG(0), 0, err) == -1)
    {
        rt_buf_give(in);
        return (-1);
    }
    e = (t_error_value *)heap_alloc(&in->heap, OBJ_ERROR,
            sizeof(t_error_value));
    error_set(&e->err, (t_error_type)error_type_named(type, strlen(type)),
        call->offset, call->len, "%.*s", (int)b->len, b->data ? b->data : "");
    rt_buf_give(in);
    e->raised = 0;
    *out = val_obj(&e->obj);
    return (0);
}

#define ERROR_NATIVE(type) \
    static int  native_##type(t_interp *in, size_t base, int nargs, \
                    const t_node *call, t_value *out, t_error *err) \
    { \
        return (make_error(in, base, nargs, call, #type, out, err)); \
    }

ERROR_NATIVE(Error)
ERROR_NATIVE(TypeError)
ERROR_NATIVE(ValueError)
ERROR_NATIVE(ZeroDivisionError)
ERROR_NATIVE(OverflowError)
ERROR_NATIVE(IndexError)
ERROR_NATIVE(KeyError)
ERROR_NATIVE(AttributeError)
ERROR_NATIVE(AssignmentError)
ERROR_NATIVE(RuntimeError)
ERROR_NATIVE(RecursionError)
ERROR_NATIVE(NotImplementedError)
ERROR_NATIVE(EOFError)
ERROR_NATIVE(AssertionError)

#define NATIVE_FN(name, min, max, repl) native_##name,
#define NATIVE_ARITY(name, min, max, repl) {min, max},
#define NATIVE_NAME(name, min, max, repl) #name,

const t_native_fn   g_native_fns[] = {
    NATIVE_LIST(NATIVE_FN)
};

/* For calls through a value, which the resolver could not check. */
const int           g_native_arity[][2] = {
    NATIVE_LIST(NATIVE_ARITY)
};

const char          *g_native_names[] = {
    NATIVE_LIST(NATIVE_NAME)
};
