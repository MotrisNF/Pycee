/*
 * c[i] and c[start:end:step] on tuples and strings. Strings count
 * characters: an ASCII one indexes its bytes directly, any other walks
 * them (and a slice with a step keeps the offset of every character in
 * the interpreter's scratch buffer meanwhile).
 */

#include <string.h>
#include "core/utf8.h"
#include "runtime/interp_internal.h"

/* A part of a slice: none (absent) or an integer. */
static int  slice_part(const t_node *n, t_value v, int64_t *out, int *given,
                t_error *err)
{
    *given = (v.type != VAL_NONE);
    if (!*given)
        return (0);
    if (v.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, n, "slice indices must be integers"
                " or none, not %s", v.type == VAL_FLOAT ? "a float"
                : val_type_name(v)));
    *out = v.as.integer;
    return (0);
}

/* A bound as Python takes it: negative from the end, then clipped. */
static int64_t  clip(int64_t x, int64_t len, int64_t lower, int64_t upper)
{
    if (x < 0)
    {
        x += len;
        if (x < lower)
            x = lower;
    }
    else if (x > upper)
        x = upper;
    return (x);
}

/*
 * The items a slice of a sequence of len takes: the first one, the
 * step between them and how many. Out-of-range bounds are clipped,
 * never an error; a step of 0 is a ValueError.
 */
static int  slice_bounds(const t_interp *in, const t_node *n, size_t base,
                int64_t len, int64_t *start, int64_t *step, int64_t *count,
                t_error *err)
{
    int64_t stop;
    int     has_start;
    int     has_stop;
    int     has_step;

    *step = 1;
    stop = 0;
    if (slice_part(n, in->stack[base + 1], start, &has_start, err) == -1
        || slice_part(n, in->stack[base + 2], &stop, &has_stop, err) == -1
        || slice_part(n, in->stack[base + 3], step, &has_step, err) == -1)
        return (-1);
    if (*step == 0)
        return (rt_error(err, ERR_VALUE, n, "slice step cannot be zero"));
    if (!has_start)
        *start = *step < 0 ? len - 1 : 0;
    else
        *start = clip(*start, len, *step < 0 ? -1 : 0, *step < 0 ? len - 1
                : len);
    if (!has_stop)
        stop = *step < 0 ? -1 : len;
    else
        stop = clip(stop, len, *step < 0 ? -1 : 0, *step < 0 ? len - 1 : len);
    *count = 0;
    if (*step > 0 && *start < stop)
        *count = (int64_t)(((uint64_t)stop - (uint64_t)*start - 1)
                / (uint64_t)*step + 1);
    else if (*step < 0 && stop < *start)
        *count = (int64_t)(((uint64_t)*start - (uint64_t)stop - 1)
                / (0 - (uint64_t)*step) + 1);
    return (0);
}

/*
 * The items and length of a tuple or a list. A list's items move when
 * it grows: get them again after anything that may run code.
 */
t_value *rt_seq_items(t_value c, size_t *len)
{
    if (val_is_list(c))
    {
        *len = val_list(c)->len;
        return (val_list(c)->items);
    }
    *len = val_tuple(c)->len;
    return (val_tuple(c)->items);
}

/* A slice of a tuple is a tuple, of a list a new list. */
static int  items_slice(t_interp *in, const t_node *n, size_t base,
                t_value *out, t_error *err)
{
    t_value *items;
    t_tuple *t;
    t_list  *l;
    size_t  len;
    int64_t start;
    int64_t step;
    int64_t count;
    int64_t k;

    rt_seq_items(in->stack[base], &len);
    if (slice_bounds(in, n, base, (int64_t)len, &start, &step, &count, err)
        == -1)
        return (-1);
    if (val_is_tuple(in->stack[base]))
    {
        t = heap_tuple(&in->heap, (size_t)count);
        items = val_tuple(in->stack[base])->items;
        for (k = 0; k < count; k++)
            t->items[k] = items[start + k * step];
        *out = val_obj(&t->obj);
        return (0);
    }
    l = heap_list(&in->heap, (size_t)count);
    items = val_list(in->stack[base])->items;
    for (k = 0; k < count; k++)
        l->items[k] = items[start + k * step];
    l->len = (size_t)count;
    *out = val_obj(&l->obj);
    return (0);
}

/*
 * The characters of a slice, into in->text. Contiguous ones (step 1, or
 * any ASCII string) are copied from their offsets; otherwise every
 * character's offset goes to scratch first, so each one is found at once.
 */
static void string_slice_text(t_interp *in, t_string *s, int64_t start,
                int64_t step, int64_t count)
{
    size_t  *offs;
    size_t  pos;
    size_t  i;
    size_t  n;
    int32_t cp;
    int64_t k;
    int64_t c;

    buf_clear(&in->text);
    if (count == 0)
        return ;
    if (step == 1)
    {
        pos = rt_string_offset(s, (size_t)start);
        buf_add(&in->text, s->chars + pos,
            rt_string_offset(s, (size_t)(start + count)) - pos);
        return ;
    }
    n = rt_string_chars(s);
    buf_clear(&in->scratch);
    pos = 0;
    for (i = 0; i <= n; i++)
    {
        buf_add(&in->scratch, (const char *)&pos, sizeof(pos));
        if (i < n)
            pos += (size_t)utf8_decode(s->chars + pos, s->len - pos, &cp);
    }
    offs = (size_t *)in->scratch.data;
    for (k = 0; k < count; k++)
    {
        c = start + k * step;
        buf_add(&in->text, s->chars + offs[c], offs[c + 1] - offs[c]);
    }
}

static int  string_slice(t_interp *in, const t_node *n, size_t base,
                t_value *out, t_error *err)
{
    t_string    *s;
    t_string    *r;
    int64_t     start;
    int64_t     step;
    int64_t     count;

    s = val_string(in->stack[base]);
    if (slice_bounds(in, n, base, (int64_t)rt_string_chars(s), &start, &step,
            &count, err) == -1)
        return (-1);
    string_slice_text(in, s, start, step, count);
    r = heap_string(&in->heap, in->text.data ? in->text.data : "",
            in->text.len);
    r->nchars = (size_t)count;
    *out = val_obj(&r->obj);
    return (0);
}

/* s[i]: a string of one character. */
static int  string_index(t_interp *in, const t_node *n, size_t base,
                t_value *out, t_error *err)
{
    t_string    *s;
    t_string    *r;
    size_t      i;
    size_t      pos;

    s = val_string(in->stack[base]);
    if (rt_seq_index(n, "string", rt_string_chars(s),
            in->stack[base + 1].as.integer, &i, err) == -1)
        return (-1);
    pos = rt_string_offset(s, i);
    r = heap_string(&in->heap, s->chars + pos, rt_string_offset(s, i + 1)
            - pos);
    r->nchars = 1;
    *out = val_obj(&r->obj);
    return (0);
}

/*
 * The container is at in->stack[base]; after it, the index (c[i]) or
 * the start, end and step of a slice (none where absent). All of them
 * stay rooted while the result is made.
 */
int rt_index_value(t_interp *in, const t_node *n, size_t base, t_value *out,
        t_error *err)
{
    t_value c;
    t_value i;
    t_value *items;
    size_t  pos;
    size_t  len;

    c = in->stack[base];
    if (val_is_obj(c, OBJ_INSTANCE) && !n->as.index.slice)
        return (rt_call_named(in, n, base, "get", 1, out, err));
    if (val_is_obj(c, OBJ_INSTANCE))
        return (rt_error(err, ERR_TYPE, n, "an object cannot be sliced"));
    if (val_is_dict(c) && !n->as.index.slice)
        return (rt_dict_get(in, n, c, in->stack[base + 1], out, err));
    if (val_is_dict(c))
        return (rt_error(err, ERR_TYPE, n, "a dict cannot be sliced"));
    if (!val_is_tuple(c) && !val_is_string(c) && !val_is_list(c))
        return (rt_error(err, ERR_TYPE, n, "'%s' object cannot be indexed",
                val_type_name(c)));
    if (n->as.index.slice)
        return (val_is_string(c) ? string_slice(in, n, base, out, err)
            : items_slice(in, n, base, out, err));
    i = in->stack[base + 1];
    if (i.type == VAL_FLOAT)
        return (rt_error(err, ERR_TYPE, n->as.index.index, "an index must be"
                " an integer, not a float"));
    if (i.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, n->as.index.index, "an index must be"
                " an integer, not '%s'", val_type_name(i)));
    if (val_is_string(c))
        return (string_index(in, n, base, out, err));
    items = rt_seq_items(c, &len);
    if (rt_seq_index(n, val_type_name(c), len, i.as.integer, &pos, err)
        == -1)
        return (-1);
    *out = items[pos];
    return (0);
}

/* obj[i] = v: its set(i, v), with obj, i and v on the stack. */
static int  call_set(t_interp *in, const t_node *n, size_t base, t_value v,
                t_error *err)
{
    t_value ignored;
    size_t  top;
    int     status;

    top = in->top;
    rt_push(in, in->stack[base]);
    rt_push(in, in->stack[base + 1]);
    rt_push(in, v);
    status = rt_call_named(in, n, top, "set", 2, &ignored, err);
    in->top = top;
    return (status);
}

/*
 * l[i] = v: the list is in->stack[base], the index after it. Only lists
 * change: a tuple or a string cannot.
 */
int rt_index_store(t_interp *in, const t_node *n, size_t base, t_value v,
        t_error *err)
{
    t_value c;
    t_value i;
    size_t  pos;

    c = in->stack[base];
    i = in->stack[base + 1];
    if (val_is_obj(c, OBJ_INSTANCE))
        return (call_set(in, n, base, v, err));
    if (val_is_dict(c))
        return (rt_table_put(in, n, base, i, v, err));
    if (!val_is_list(c))
        return (rt_error(err, ERR_TYPE, n, "'%s' object does not support"
                " item assignment%s", val_type_name(c), val_is_tuple(c)
                || val_is_string(c) ? ": it cannot change" : ""));
    if (i.type == VAL_FLOAT)
        return (rt_error(err, ERR_TYPE, n->as.index.index, "an index must be"
                " an integer, not a float"));
    if (i.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, n->as.index.index, "an index must be"
                " an integer, not '%s'", val_type_name(i)));
    if (rt_seq_index(n, "list", val_list(c)->len, i.as.integer, &pos, err)
        == -1)
        return (-1);
    val_list(c)->items[pos] = v;
    return (0);
}
