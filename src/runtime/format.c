/* Values as text: print and str (plain), the REPL echo (repr). */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/stack.h"
#include "runtime/interp_internal.h"

/*
 * The digits of the shortest decimal that reads back as exactly f
 * (finite, not negative) and its exponent: f is d0.d1d2... * 10^exp.
 * Returns the number of digits (no trailing zeros).
 */
static int  shortest_digits(double f, char *digits, int *exp)
{
    char    text[40];
    char    *e;
    int     precision;
    int     n;
    int     i;

    for (precision = 1; precision <= 17; precision++)
    {
        snprintf(text, sizeof(text), "%.*e", precision - 1, f);
        if (strtod(text, NULL) == f)
            break ;
    }
    e = strchr(text, 'e');
    *exp = atoi(e + 1);
    n = 0;
    for (i = 0; text + i < e; i++)
        if (text[i] != '.')
            digits[n++] = text[i];
    while (n > 1 && digits[n - 1] == '0')
        n--;
    digits[n] = '\0';
    return (n);
}

/*
 * The shortest text that reads back as exactly f, as Python's repr:
 * plain notation while the exponent is in [-4, 16), an exponent outside
 * (1e+16, 1e-05); an integral float keeps ".0" so it never looks like an
 * int (7.0, 1000.0).
 */
void    rt_format_float(t_buffer *b, double f)
{
    char    digits[24];
    int     n;
    int     exp;
    int     i;

    if (f != f || isinf(f))
    {
        buf_add_str(b, f != f ? "nan" : f < 0 ? "-inf" : "inf");
        return ;
    }
    if (signbit(f))
        buf_add_char(b, '-');
    n = shortest_digits(fabs(f), digits, &exp);
    if (exp < -4 || exp >= 16)
    {
        buf_add_char(b, digits[0]);
        if (n > 1)
        {
            buf_add_char(b, '.');
            buf_add(b, digits + 1, (size_t)(n - 1));
        }
        buf_printf(b, "e%c%02d", exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
    }
    else if (exp >= 0)
    {
        for (i = 0; i <= exp; i++)
            buf_add_char(b, i < n ? digits[i] : '0');
        buf_add_char(b, '.');
        if (n > exp + 1)
            buf_add(b, digits + exp + 1, (size_t)(n - exp - 1));
        else
            buf_add_char(b, '0');
    }
    else
    {
        buf_add_str(b, "0.");
        for (i = 0; i < -exp - 1; i++)
            buf_add_char(b, '0');
        buf_add(b, digits, (size_t)n);
    }
}

/* "..." with escapes, so the text reads back as the same string. */
static void format_string_repr(t_buffer *b, const char *chars, size_t len)
{
    size_t          i;
    unsigned char   c;

    buf_add_char(b, '"');
    for (i = 0; i < len; i++)
    {
        c = (unsigned char)chars[i];
        if (c == '"' || c == '\\')
        {
            buf_add_char(b, '\\');
            buf_add_char(b, (char)c);
        }
        else if (c == '\n')
            buf_add_str(b, "\\n");
        else if (c == '\t')
            buf_add_str(b, "\\t");
        else if (c == '\r')
            buf_add_str(b, "\\r");
        else if (c == '\0')
            buf_add_str(b, "\\0");
        else if (c < 0x20 || c == 0x7f)
            buf_printf(b, "\\x%02X", c);
        else
            buf_add_char(b, (char)c);
    }
    buf_add_char(b, '"');
}

/* range(0, 5), and the step only when it is not 1: range(0, 10, 2). */
static void format_range(t_buffer *b, const t_range *r)
{
    buf_printf(b, "range(%lld, %lld", (long long)r->start,
        (long long)r->stop);
    if (r->step != 1)
        buf_printf(b, ", %lld", (long long)r->step);
    buf_add_char(b, ')');
}

/*
 * The containers being shown, innermost first (each one on the C stack
 * of its own call): one that contains itself shows "[...]" there
 * instead of going round for ever.
 */
typedef struct s_shown
{
    const t_obj             *obj;
    const struct s_shown    *outer;
}   t_shown;

/*
 * Showing values for the program (print, str, f-strings, the REPL): an
 * object's to_str is called, so this can run code and fail. in is NULL
 * for text that must not run anything (error messages).
 */
typedef struct s_show
{
    t_interp        *in;
    const t_node    *at;
    t_error         *err;
    int             failed;
}   t_show;

static void format_value(t_buffer *b, t_value v, int repr,
                const t_shown *shown, t_show *show);

static int  being_shown(const t_shown *shown, const t_obj *o)
{
    for (; shown; shown = shown->outer)
        if (shown->obj == o)
            return (1);
    return (0);
}

/*
 * tuple(1, "a") or [1, "a"]: items always in repr form. Nesting deeper
 * than the stack allows, or a container inside itself, is cut short
 * with "...".
 */
/*
 * With show, c stays on the stack while its items are shown (a to_str
 * may drop it from where it was), and they are fetched again each time
 * (a to_str may change the list).
 */
static void format_items(t_buffer *b, t_value c, const t_shown *shown,
                t_show *show)
{
    t_shown         here;
    size_t          len;
    size_t          i;
    int             list;

    list = val_is_list(c);
    if (stack_exhausted() || being_shown(shown, c.as.obj))
    {
        buf_add_str(b, list ? "[...]" : "tuple(...)");
        return ;
    }
    here.obj = c.as.obj;
    here.outer = shown;
    if (show)
        rt_push(show->in, c);
    buf_add_str(b, list ? "[" : "tuple(");
    for (i = 0; (!show || !show->failed) && (rt_seq_items(c, &len), i < len);
        i++)
    {
        if (i > 0)
            buf_add_str(b, ", ");
        format_value(b, rt_seq_items(c, &len)[i], 1, &here, show);
    }
    buf_add_char(b, list ? ']' : ')');
    if (show)
        rt_pop(show->in);
}

/*
 * dict("a": 1) / set(1, 2), in insertion order, like format_items (a
 * table inside itself shows dict(...) / set(...) there).
 */
static void format_table(t_buffer *b, t_value v, const t_shown *shown,
                t_show *show)
{
    t_shown         here;
    const t_dict    *d;
    size_t          i;
    int             first;
    int             is_dict;

    is_dict = val_is_dict(v);
    if (stack_exhausted() || being_shown(shown, v.as.obj))
    {
        buf_add_str(b, is_dict ? "dict(...)" : "set(...)");
        return ;
    }
    here.obj = v.as.obj;
    here.outer = shown;
    if (show)
        rt_push(show->in, v);
    buf_add_str(b, is_dict ? "dict(" : "set(");
    first = 1;
    for (i = 0; i < val_dict(v)->nentries && (!show || !show->failed); i++)
    {
        d = val_dict(v);
        if (!d->entries[i].used)
            continue ;
        if (!first)
            buf_add_str(b, ", ");
        first = 0;
        format_value(b, d->entries[i].key, 1, &here, show);
        if (is_dict && i < val_dict(v)->nentries && !(show && show->failed))
        {
            buf_add_str(b, ": ");
            format_value(b, val_dict(v)->entries[i].value, 1, &here, show);
        }
    }
    buf_add_char(b, ')');
    if (show)
        rt_pop(show->in);
}

/*
 * <func double>, <func lambda>, <built-in print>, <method number.round>:
 * a function shows what it is, never an address.
 */
static void format_function(t_buffer *b, t_value v)
{
    const t_node    *f;
    const t_bound   *m;

    if (val_is_obj(v, OBJ_FUNCTION))
    {
        f = ((const t_function *)v.as.obj)->decl;
        if (f->as.func.is_lambda)
            buf_add_str(b, "<func lambda>");
        else
            buf_printf(b, "<func %.*s>", (int)f->as.func.name.len,
                f->as.func.name.ptr);
    }
    else if (val_is_obj(v, OBJ_NATIVE))
        buf_printf(b, "<built-in %s>",
            g_native_names[((const t_native *)v.as.obj)->slot]);
    else
    {
        m = (const t_bound *)v.as.obj;
        buf_printf(b, "<method %s.%s>", val_type_name(m->self),
            m->method->name);
    }
}

/*
 * print(e): "ValueError: bad input" (just the type with no message).
 * Its repr is how it is built: ValueError("bad input").
 */
static void format_error(t_buffer *b, const t_error *e, int repr)
{
    buf_add_str(b, error_type_name(e->type));
    if (repr)
    {
        buf_add_char(b, '(');
        format_string_repr(b, e->msg, strlen(e->msg));
        buf_add_char(b, ')');
    }
    else if (e->msg[0])
        buf_printf(b, ": %s", e->msg);
}

/*
 * Appends v to b. repr shows strings quoted and escaped (the REPL echo,
 * and strings inside containers); otherwise a string is its own text.
 */
void    rt_format(t_buffer *b, t_value v, int repr)
{
    format_value(b, v, repr, NULL, NULL);
}

/*
 * v as the program shows it, appended to b (a buffer of the pool: the
 * code run here may use the others). at is where an error goes.
 */
int rt_show(t_interp *in, const t_node *at, t_buffer *b, t_value v, int repr,
        t_error *err)
{
    t_show  show;

    show.in = in;
    show.at = at;
    show.err = err;
    show.failed = 0;
    format_value(b, v, repr, NULL, &show);
    return (show.failed ? -1 : 0);
}

/*
 * An object: what its to_str gives (a string, or a TypeError), else
 * <Point>; one that extends an error type shows as an error does.
 */
static void format_instance(t_buffer *b, t_value v, int repr, t_show *show)
{
    const t_instance    *o;
    const t_node        *f;
    t_value             r;
    t_name              name;
    size_t              top;

    o = (const t_instance *)v.as.obj;
    name.ptr = "to_str";
    name.len = 6;
    f = rt_obj_find(o->cls, name);
    if (show && f && f->type == NODE_FUNC && !f->as.func.is_static)
    {
        top = show->in->top;
        rt_push(show->in, v);
        if (rt_call_method_func(show->in, show->at, f, &v, top + 1, 0, NULL,
                &r, show->err) == -1)
            show->failed = 1;
        else if (!val_is_string(r))
            show->failed = rt_error(show->err, ERR_TYPE, show->at, "to_str()"
                    " must give back a string, not '%s'", val_type_name(r))
                == -1;
        else
            buf_add(b, val_string(r)->chars, val_string(r)->len);
        show->in->top = top;
    }
    else if (o->error && repr)
        buf_printf(b, "%s(", o->cls->as.obj.cname);
    if (o->error && !(show && f))
    {
        if (repr)
        {
            format_string_repr(b, o->error->err.msg, strlen(o->error->err.msg));
            buf_add_char(b, ')');
        }
        else
            buf_printf(b, "%s%s%s", o->cls->as.obj.cname,
                o->error->err.msg[0] ? ": " : "", o->error->err.msg);
    }
    else if (!(show && f))
        buf_printf(b, "<%s>", o->cls->as.obj.cname);
}

static void format_value(t_buffer *b, t_value v, int repr,
                const t_shown *shown, t_show *show)
{
    t_string    *s;

    if (v.type == VAL_NONE)
        buf_add_str(b, "none");
    else if (v.type == VAL_BOOL)
        buf_add_str(b, v.as.boolean ? "true" : "false");
    else if (v.type == VAL_INT)
        buf_printf(b, "%lld", (long long)v.as.integer);
    else if (v.type == VAL_FLOAT)
        rt_format_float(b, v.as.number);
    else if (val_is_range(v))
        format_range(b, val_range(v));
    else if (val_is_tuple(v) || val_is_list(v))
        format_items(b, v, shown, show);
    else if (val_is_dict(v) || val_is_set(v))
        format_table(b, v, shown, show);
    else if (val_is_obj(v, OBJ_FUNCTION) || val_is_obj(v, OBJ_NATIVE)
        || val_is_obj(v, OBJ_BOUND))
        format_function(b, v);
    else if (val_is_obj(v, OBJ_ERROR))
        format_error(b, &((const t_error_value *)v.as.obj)->err, repr);
    else if (val_is_obj(v, OBJ_INSTANCE))
        format_instance(b, v, repr, show);
    else if (val_is_obj(v, OBJ_CLASS))
        buf_printf(b, "<obj %s>", ((const t_class *)v.as.obj)->decl->as.obj.cname);
    else if (val_is_obj(v, OBJ_METHOD))
        buf_printf(b, "<method %s.%.*s>", ((const t_method_value *)v.as.obj)
            ->func->as.func.cls->as.obj.cname,
            (int)((const t_method_value *)v.as.obj)->func->as.func.name.len,
            ((const t_method_value *)v.as.obj)->func->as.func.name.ptr);
    else if (val_is_string(v))
    {
        s = val_string(v);
        if (repr)
            format_string_repr(b, s->chars, s->len);
        else
            buf_add(b, s->chars, s->len);
    }
}
