/*
 * Methods of list and tuple. Most work on both (a tuple is a list that
 * cannot change); those that change it are the list's only.
 *
 * Some call a function of the program (map, filter, sort's key...): it
 * may do anything, even change the list, so items are fetched again
 * after every call (a list's items move when it grows), and a list that
 * changed size meanwhile is a RuntimeError, as in a for. Every value in
 * flight is on the stack.
 */

#include <string.h>
#include "runtime/interp_internal.h"

#define SELF    (in->stack[base])
#define ARG(i)  (in->stack[base + 1 + (i)])

static size_t   seq_len(t_value c)
{
    size_t  len;

    rt_seq_items(c, &len);
    return (len);
}

/* A RuntimeError if the list at stack[base] no longer has len items. */
static int  changed(t_interp *in, size_t base, size_t len, const char *what,
                const t_node *call, t_error *err)
{
    if (!val_is_list(SELF) || val_list(SELF)->len == len)
        return (0);
    return (rt_error(err, ERR_RUNTIME, call, "the list changed size during"
            " %s() (from %zu to %zu items)", what, len, val_list(SELF)->len));
}

/* f(a), from C: f and a go on the stack while it runs. */
static int  call1(t_interp *in, const t_node *call, t_value f, t_value a,
                t_value *out, t_error *err)
{
    size_t  top;
    int     status;

    top = in->top;
    rt_push(in, f);
    rt_push(in, a);
    status = rt_call(in, call, top, 1, out, err);
    in->top = top;
    return (status);
}

/* A list made at stack[slot] gets v at its end. */
static void list_add(t_interp *in, size_t slot, t_value v)
{
    t_list  *l;

    l = val_list(in->stack[slot]);
    heap_list_reserve(&in->heap, l, l->len + 1);
    l->items[l->len++] = v;
}

/* The list at stack[slot] as a tuple, when the method was called on one. */
static void same_type(t_interp *in, size_t base, size_t slot, t_value *out)
{
    t_tuple *t;
    size_t  i;

    if (val_is_list(SELF))
    {
        *out = in->stack[slot];
        return ;
    }
    t = heap_tuple(&in->heap, val_list(in->stack[slot])->len);
    for (i = 0; i < t->len; i++)
        t->items[i] = val_list(in->stack[slot])->items[i];
    *out = val_obj(&t->obj);
}

static int  m_len(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_int((int64_t)seq_len(SELF));
    return (0);
}

/* c.join(sep): the same as sep.join(c). */
static int  m_join(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    if (!val_is_string(ARG(0)))
        return (rt_error(err, ERR_TYPE, call, "join() needs a string"
                " separator, not '%s'", val_type_name(ARG(0))));
    return (rt_join(in, call, val_string(ARG(0)), SELF, out, err));
}

/* The position of the first item that is x, or -1 (-2: RecursionError). */
static int64_t  find_item(t_interp *in, size_t base, t_value x)
{
    const t_value   *items;
    size_t          len;
    size_t          i;
    int             r;

    items = rt_seq_items(SELF, &len);
    for (i = 0; i < len; i++)
    {
        r = rt_equal(items[i], x);
        if (r == -1)
            return (-2);
        if (r == 1)
            return ((int64_t)i);
    }
    return (-1);
}

static int  m_index(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    int64_t i;

    (void)nargs;
    i = find_item(in, base, ARG(0));
    if (i == -2)
        return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    if (i == -1)
        return (rt_error(err, ERR_VALUE, call, "index(): the value is not in"
                " the %s", val_type_name(SELF)));
    *out = val_int(i);
    return (0);
}

static int  m_count(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_value   *items;
    size_t          len;
    size_t          i;
    int64_t         n;
    int             r;

    (void)nargs;
    items = rt_seq_items(SELF, &len);
    n = 0;
    for (i = 0; i < len; i++)
    {
        r = rt_equal(items[i], ARG(0));
        if (r == -1)
            return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
        n += r;
    }
    *out = val_int(n);
    return (0);
}

/* map(f) / filter(f): a new list (or tuple, on a tuple). */
static int  map_filter(t_interp *in, size_t base, int filter,
                const t_node *call, t_value *out, t_error *err)
{
    const char  *what;
    size_t      len;
    size_t      slot;
    size_t      i;
    t_value     v;
    int         keep;

    what = filter ? "filter" : "map";
    len = seq_len(SELF);
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, filter ? 0 : len)->obj));
    for (i = 0; i < len; i++)
    {
        if (call1(in, call, ARG(0), rt_seq_items(SELF, &len)[i], &v, err)
            == -1 || changed(in, base, len, what, call, err) == -1)
            return (-1);
        if (filter && rt_truthy(call, v, "the function of filter()", &keep,
                err) == -1)
            return (-1);
        if (!filter || keep)
            list_add(in, slot, filter ? rt_seq_items(SELF, &len)[i] : v);
    }
    same_type(in, base, slot, out);
    in->top = slot;
    return (0);
}

static int  m_map(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (map_filter(in, base, 0, call, out, err));
}

static int  m_filter(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (map_filter(in, base, 1, call, out, err));
}

/*
 * reduce(f, start): f(acc, x) for each item, from start; without start,
 * from the first item (an empty one is then a ValueError).
 */
static int  m_reduce(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    size_t  len;
    size_t  acc;
    size_t  i;
    size_t  top;
    t_value v;
    int     status;

    len = seq_len(SELF);
    if (nargs < 2 && len == 0)
        return (rt_error(err, ERR_VALUE, call, "reduce() of an empty %s"
                " with no start value", val_type_name(SELF)));
    acc = in->top;
    rt_push(in, nargs == 2 ? ARG(1) : rt_seq_items(SELF, &len)[0]);
    for (i = nargs == 2 ? 0 : 1; i < len; i++)
    {
        top = in->top;
        rt_push(in, ARG(0));
        rt_push(in, in->stack[acc]);
        rt_push(in, rt_seq_items(SELF, &len)[i]);
        status = rt_call(in, call, top, 2, &v, err);
        in->top = top;
        in->stack[acc] = v;
        if (status == -1 || changed(in, base, len, "reduce", call, err) == -1)
            return (-1);
    }
    *out = in->stack[acc];
    in->top = acc;
    return (0);
}

/*
 * mode: 0 any, 1 all, 2 each. f must give a bool (any / all); without
 * f (any() / all()), each item must be one.
 */
static int  quantify(t_interp *in, size_t base, int nargs, int mode,
                const t_node *call, t_value *out, t_error *err)
{
    const char  *what;
    size_t      len;
    size_t      i;
    t_value     v;
    int         b;

    what = mode == 0 ? "any" : mode == 1 ? "all" : "each";
    len = seq_len(SELF);
    for (i = 0; i < len; i++)
    {
        if (nargs == 0)
            v = rt_seq_items(SELF, &len)[i];
        else if (call1(in, call, ARG(0), rt_seq_items(SELF, &len)[i], &v,
                err) == -1 || changed(in, base, len, what, call, err) == -1)
            return (-1);
        if (mode == 2)
            continue ;
        if (rt_truthy(call, v, nargs == 0 ? (mode ? "each item of all()"
                    : "each item of any()") : mode ? "the function of all()"
                : "the function of any()", &b, err) == -1)
            return (-1);
        if (b == (mode == 0))
        {
            *out = val_bool(mode == 0);
            return (0);
        }
    }
    *out = mode == 2 ? val_none() : val_bool(mode == 1);
    return (0);
}

static int  m_any(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (quantify(in, base, nargs, 0, call, out, err));
}

static int  m_all(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (quantify(in, base, nargs, 1, call, out, err));
}

static int  m_each(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (quantify(in, base, nargs, 2, call, out, err));
}

/* sum(): of numbers only, from 0 (with the usual overflow checks). */
static int  m_sum(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_value   *items;
    size_t          len;
    size_t          i;
    size_t          acc;
    t_value         v;

    (void)nargs;
    acc = in->top;
    rt_push(in, val_int(0));
    items = rt_seq_items(SELF, &len);
    for (i = 0; i < len; i++)
    {
        if (items[i].type != VAL_INT && items[i].type != VAL_FLOAT)
            return (rt_error(err, ERR_TYPE, call, "sum() needs numbers, but"
                    " item %zu is '%s'", i, val_type_name(items[i])));
        rt_push(in, items[i]);
        if (rt_binary(in, call, TOK_PLUS, &v, err) == -1)
            return (-1);
        in->top = acc;
        rt_push(in, v);
        items = rt_seq_items(SELF, &len);
    }
    *out = in->stack[acc];
    in->top = acc;
    return (0);
}

/* min() / max(): the first smallest / largest; empty is a ValueError. */
static int  extreme(t_interp *in, size_t base, int largest,
                const t_node *call, t_value *out, t_error *err)
{
    const t_value   *items;
    size_t          len;
    size_t          i;
    size_t          best;
    int             c;

    rt_seq_items(SELF, &len);
    if (len == 0)
        return (rt_error(err, ERR_VALUE, call, "%s() of an empty %s",
                largest ? "max" : "min", val_type_name(SELF)));
    best = 0;
    for (i = 1; i < len; i++)
    {
        items = rt_seq_items(SELF, &len);
        if (rt_compare(in, call, items[i], items[best], &c, err) == -1
            || changed(in, base, len, largest ? "max" : "min", call, err)
            == -1)
            return (-1);
        if ((largest && c > 0) || (!largest && c < 0))
            best = i;
    }
    *out = rt_seq_items(SELF, &len)[best];
    return (0);
}

static int  m_min(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (extreme(in, base, 0, call, out, err));
}

static int  m_max(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (extreme(in, base, 1, call, out, err));
}

/*
 * c.enumerate(): a list of tuple(index, item). Each tuple is made with
 * the list rooted; the item is fetched again after making it.
 */
static int  m_enumerate(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_tuple *t;
    size_t  len;
    size_t  slot;
    size_t  i;

    (void)nargs;
    (void)call;
    (void)err;
    len = seq_len(SELF);
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, len)->obj));
    for (i = 0; i < len; i++)
    {
        t = heap_tuple(&in->heap, 2);
        t->items[0] = val_int((int64_t)i);
        t->items[1] = rt_seq_items(SELF, &len)[i];
        list_add(in, slot, val_obj(&t->obj));
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/* to_list() / to_tuple() / copy(): the same items in a new value. */
static int  convert(t_interp *in, size_t base, int to_list, t_value *out)
{
    t_tuple *t;
    t_list  *l;
    size_t  len;
    size_t  i;

    len = seq_len(SELF);
    if (to_list)
    {
        l = heap_list(&in->heap, len);
        for (i = 0; i < len; i++)
            l->items[i] = rt_seq_items(SELF, &len)[i];
        l->len = len;
        *out = val_obj(&l->obj);
        return (0);
    }
    t = heap_tuple(&in->heap, len);
    for (i = 0; i < len; i++)
        t->items[i] = rt_seq_items(SELF, &len)[i];
    *out = val_obj(&t->obj);
    return (0);
}

static int  m_to_list(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (convert(in, base, 1, out));
}

static int  m_to_tuple(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (convert(in, base, 0, out));
}

/* to_set(): its items once each (only immutable ones can be in a set). */
static int  m_to_set(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    size_t  slot;
    size_t  len;
    size_t  i;

    (void)nargs;
    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_SET)->obj));
    len = seq_len(SELF);
    for (i = 0; i < len; i++)
        if (rt_table_put(in, call, slot, rt_seq_items(SELF, &len)[i],
                val_none(), err) == -1)
            return (-1);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/*
 * ---------- the list's own: they change it ----------
 * Each gives back the list itself (pop, the item), so they chain:
 * l.append(3).sort(). A sorted copy is l.copy().sort().
 */

static int  m_append(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_list  *l;

    (void)nargs;
    (void)call;
    (void)err;
    l = val_list(SELF);
    heap_list_reserve(&in->heap, l, l->len + 1);
    l->items[l->len++] = ARG(0);
    *out = SELF;
    return (0);
}

/* extend(x): the items a for over x gives, at the end (x may be l). */
static int  m_extend(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_list  *l;
    t_list  *more;
    size_t  top;
    size_t  i;

    (void)nargs;
    top = in->top;
    if (rt_collect(in, call, ARG(0), out, err) == -1)
        return (-1);
    rt_push(in, *out);
    more = val_list(*out);
    l = val_list(SELF);
    heap_list_reserve(&in->heap, l, l->len + more->len);
    for (i = 0; i < more->len; i++)
        l->items[l->len++] = more->items[i];
    in->top = top;
    *out = SELF;
    return (0);
}

/* insert(i, x): before position i (negative from the end, clipped). */
static int  m_insert(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_list  *l;
    int64_t i;

    (void)nargs;
    if (ARG(0).type != VAL_INT)
        return (rt_error(err, ERR_TYPE, call, "insert() needs an integer"
                " position, not '%s'", val_type_name(ARG(0))));
    l = val_list(SELF);
    i = ARG(0).as.integer;
    if (i < 0)
        i = i + (int64_t)l->len < 0 ? 0 : i + (int64_t)l->len;
    if (i > (int64_t)l->len)
        i = (int64_t)l->len;
    heap_list_reserve(&in->heap, l, l->len + 1);
    memmove(l->items + i + 1, l->items + i,
        (l->len - (size_t)i) * sizeof(t_value));
    l->items[i] = ARG(1);
    l->len++;
    *out = SELF;
    return (0);
}

/* pop() / pop(i): removes and gives back the last item (or the i-th). */
static int  m_pop(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    t_list  *l;
    size_t  i;

    l = val_list(SELF);
    if (l->len == 0)
        return (rt_error(err, ERR_INDEX, call, "pop from an empty list"));
    i = l->len - 1;
    if (nargs == 1 && ARG(0).type != VAL_INT)
        return (rt_error(err, ERR_TYPE, call, "pop() needs an integer"
                " position, not '%s'", val_type_name(ARG(0))));
    if (nargs == 1 && rt_seq_index(call, "list", l->len, ARG(0).as.integer,
            &i, err) == -1)
        return (-1);
    *out = l->items[i];
    memmove(l->items + i, l->items + i + 1,
        (l->len - i - 1) * sizeof(t_value));
    l->len--;
    return (0);
}

static int  m_remove(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_list  *l;
    int64_t i;

    (void)nargs;
    i = find_item(in, base, ARG(0));
    if (i == -2)
        return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    if (i == -1)
        return (rt_error(err, ERR_VALUE, call, "remove(): the value is not in"
                " the list"));
    l = val_list(SELF);
    memmove(l->items + i, l->items + i + 1,
        (l->len - (size_t)i - 1) * sizeof(t_value));
    l->len--;
    *out = SELF;
    return (0);
}

static int  m_clear(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    val_list(SELF)->len = 0;
    *out = SELF;
    return (0);
}

static int  m_reverse(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_list  *l;
    t_value tmp;
    size_t  i;

    (void)nargs;
    (void)call;
    (void)err;
    l = val_list(SELF);
    for (i = 0; i < l->len / 2; i++)
    {
        tmp = l->items[i];
        l->items[i] = l->items[l->len - 1 - i];
        l->items[l->len - 1 - i] = tmp;
    }
    *out = SELF;
    return (0);
}

/*
 * One merge of a bottom-up merge sort: the positions in stack[idx +
 * lo, idx + hi), sorted in two halves at mid, merged into stack[tmp...]
 * and copied back. keys[k] is what item k is compared by. Equal keys keep
 * their order (stable), also with reverse.
 */
static int  merge(t_interp *in, const t_node *call, size_t keys, size_t idx,
                size_t tmp, size_t lo, size_t mid, size_t hi, int rev,
                t_error *err)
{
    size_t  a;
    size_t  b;
    size_t  k;
    int     c;

    a = lo;
    b = mid;
    for (k = lo; k < hi; k++)
    {
        c = 0;
        if (a < mid && b < hi && rt_compare(in, call,
                in->stack[keys + (size_t)in->stack[idx + a].as.integer],
                in->stack[keys + (size_t)in->stack[idx + b].as.integer], &c,
                err) == -1)
            return (-1);
        if (a < mid && (b >= hi || (rev ? c >= 0 : c <= 0)))
            in->stack[tmp + k] = in->stack[idx + a++];
        else
            in->stack[tmp + k] = in->stack[idx + b++];
    }
    for (k = lo; k < hi; k++)
        in->stack[idx + k] = in->stack[tmp + k];
    return (0);
}

/*
 * sort(key = f, reverse = true), in place. Keys, positions and the work
 * area are all on the value stack (rooted, and nothing in C to go stale
 * if a key calls code that sorts something else). An error leaves the
 * list as it was.
 */
static int  m_sort(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    t_value key;
    t_value v;
    size_t  len;
    size_t  keys;
    size_t  w;
    size_t  lo;
    size_t  i;
    int     rev;

    key = nargs >= 1 ? ARG(0) : val_none();
    rev = 0;
    if (nargs == 2 && ARG(1).type != VAL_NONE
        && rt_truthy(call, ARG(1), "reverse of sort()", &rev, err) == -1)
        return (-1);
    len = val_list(SELF)->len;
    keys = in->top;
    for (i = 0; i < len; i++)
    {
        if (key.type == VAL_NONE)
            rt_push(in, val_list(SELF)->items[i]);
        else if (call1(in, call, key, val_list(SELF)->items[i], &v, err)
            == -1)
            return (-1);
        else
            rt_push(in, v);
        if (changed(in, base, len, "sort", call, err) == -1)
            return (-1);
    }
    for (i = 0; i < len; i++)
        rt_push(in, val_int((int64_t)i));
    for (i = 0; i < len; i++)
        rt_push(in, val_none());
    for (w = 1; w < len; w *= 2)
        for (lo = 0; lo + w < len; lo += 2 * w)
            if (merge(in, call, keys, keys + len, keys + 2 * len, lo, lo + w,
                    lo + 2 * w < len ? lo + 2 * w : len, rev, err) == -1)
                return (-1);
    if (changed(in, base, len, "sort", call, err) == -1)
        return (-1);
    for (i = 0; i < len; i++)
        in->stack[keys + 2 * len + i] = val_list(SELF)->items[
            in->stack[keys + len + i].as.integer];
    for (i = 0; i < len; i++)
        val_list(SELF)->items[i] = in->stack[keys + 2 * len + i];
    in->top = keys;
    *out = SELF;
    return (0);
}

static const char *const    g_sort_names[] = {"key", "reverse", NULL};
static const char *const    g_reduce_names[] = {"f", "start", NULL};

const t_method  g_list_methods[] = {
    {"len", 0, 0, m_len, NULL},
    {"append", 1, 1, m_append, NULL},
    {"extend", 1, 1, m_extend, NULL},
    {"insert", 2, 2, m_insert, NULL},
    {"pop", 0, 1, m_pop, NULL},
    {"remove", 1, 1, m_remove, NULL},
    {"clear", 0, 0, m_clear, NULL},
    {"index", 1, 1, m_index, NULL},
    {"count", 1, 1, m_count, NULL},
    {"sort", 0, 2, m_sort, g_sort_names},
    {"reverse", 0, 0, m_reverse, NULL},
    {"join", 1, 1, m_join, NULL},
    {"copy", 0, 0, m_to_list, NULL},
    {"to_tuple", 0, 0, m_to_tuple, NULL},
    {"to_set", 0, 0, m_to_set, NULL},
    {"map", 1, 1, m_map, NULL},
    {"filter", 1, 1, m_filter, NULL},
    {"reduce", 1, 2, m_reduce, g_reduce_names},
    {"any", 0, 1, m_any, NULL},
    {"all", 0, 1, m_all, NULL},
    {"each", 1, 1, m_each, NULL},
    {"sum", 0, 0, m_sum, NULL},
    {"min", 0, 0, m_min, NULL},
    {"max", 0, 0, m_max, NULL},
    {"enumerate", 0, 0, m_enumerate, NULL},
    {NULL, 0, 0, NULL, NULL},
};

const t_method  g_tuple_methods[] = {
    {"len", 0, 0, m_len, NULL},
    {"index", 1, 1, m_index, NULL},
    {"count", 1, 1, m_count, NULL},
    {"join", 1, 1, m_join, NULL},
    {"to_list", 0, 0, m_to_list, NULL},
    {"to_set", 0, 0, m_to_set, NULL},
    {"map", 1, 1, m_map, NULL},
    {"filter", 1, 1, m_filter, NULL},
    {"reduce", 1, 2, m_reduce, g_reduce_names},
    {"any", 0, 1, m_any, NULL},
    {"all", 0, 1, m_all, NULL},
    {"each", 1, 1, m_each, NULL},
    {"sum", 0, 0, m_sum, NULL},
    {"min", 0, 0, m_min, NULL},
    {"max", 0, 0, m_max, NULL},
    {"enumerate", 0, 0, m_enumerate, NULL},
    {NULL, 0, 0, NULL, NULL},
};
