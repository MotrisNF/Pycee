/*
 * Methods of dict and set (the table is in table.c). A key is checked
 * (hashed) before anything changes, so an error leaves the table as it
 * was. Every value in flight is on the stack. Those that change it
 * (add, remove, clear) give it back, so they chain, as a list's do.
 */

#include "runtime/interp_internal.h"

#define SELF    (in->stack[base])
#define ARG(i)  (in->stack[base + 1 + (i)])

/* key in the table: its entry (*pos) or 0; -1 with err filled. */
static int  lookup(const t_node *call, t_value table, t_value key,
                const char *what, size_t *pos, t_error *err)
{
    uint64_t    h;
    int         r;

    if (rt_hash(call, key, what, &h, err) == -1)
        return (-1);
    r = rt_table_find(val_dict(table), key, h, pos);
    if (r == -1)
        return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    return (r);
}

/* Adds key (-> value) to the table at stack[slot]. */
int rt_table_put(t_interp *in, const t_node *call, size_t slot, t_value key,
        t_value value, t_error *err)
{
    uint64_t    h;
    const char  *what;

    what = val_is_set(in->stack[slot]) ? "a set element" : "a dict key";
    if (rt_hash(call, key, what, &h, err) == -1)
        return (-1);
    if (rt_table_set(&in->heap, val_dict(in->stack[slot]), key, h, value)
        == -1)
        return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    return (0);
}

/* KeyError: key "x" is not in the dict (the key as it is written). */
static int  key_error(t_interp *in, const t_node *call, t_value key,
                t_error *err)
{
    buf_clear(&in->scratch);
    rt_format(&in->scratch, key, 1);
    if (in->scratch.len > 60)
    {
        in->scratch.len = 57;
        buf_add_str(&in->scratch, "...");
    }
    return (rt_error(err, ERR_KEY, call, "key %.*s is not in the dict",
            (int)in->scratch.len, in->scratch.data));
}

/* d[k]: its value, or a KeyError. */
int rt_dict_get(t_interp *in, const t_node *call, t_value d, t_value key,
        t_value *out, t_error *err)
{
    size_t  pos;
    int     r;

    r = lookup(call, d, key, "a dict key", &pos, err);
    if (r == -1)
        return (-1);
    if (r == 0)
        return (key_error(in, call, key, err));
    *out = val_dict(d)->entries[pos].value;
    return (0);
}

/* k in d / x in s. */
int rt_table_has(const t_node *call, t_value table, t_value key, int *out,
        t_error *err)
{
    size_t  pos;
    int     r;

    r = lookup(call, table, key, val_is_set(table) ? "a set element"
            : "a dict key", &pos, err);
    if (r == -1)
        return (-1);
    *out = r;
    return (0);
}

static int  m_len(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_int((int64_t)val_dict(SELF)->len);
    return (0);
}

/* get(k) / get(k, default): never a KeyError (none by default). */
static int  m_get(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    size_t  pos;
    int     r;

    r = lookup(call, SELF, ARG(0), "a dict key", &pos, err);
    if (r == -1)
        return (-1);
    if (r == 1)
        *out = val_dict(SELF)->entries[pos].value;
    else
        *out = nargs == 2 ? ARG(1) : val_none();
    return (0);
}

/*
 * keys() / values() / items() of a dict, and to_list() / to_tuple() of a
 * set: a new list (or tuple) in insertion order. mode: 0 keys, 1 values,
 * 2 items (tuple(k, v) each).
 */
static int  collect(t_interp *in, size_t base, int mode, int as_tuple,
                t_value *out)
{
    t_dict  *d;
    t_tuple *t;
    t_list  *l;
    size_t  slot;
    size_t  i;

    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, val_dict(SELF)->len)->obj));
    for (i = 0; i < val_dict(SELF)->nentries; i++)
    {
        d = val_dict(SELF);
        if (!d->entries[i].used)
            continue ;
        if (mode == 2)
        {
            t = heap_tuple(&in->heap, 2);
            t->items[0] = val_dict(SELF)->entries[i].key;
            t->items[1] = val_dict(SELF)->entries[i].value;
            rt_push(in, val_obj(&t->obj));
        }
        else
            rt_push(in, mode ? d->entries[i].value : d->entries[i].key);
        l = val_list(in->stack[slot]);
        l->items[l->len++] = rt_pop(in);
    }
    if (as_tuple)
    {
        t = heap_tuple(&in->heap, val_list(in->stack[slot])->len);
        for (i = 0; i < t->len; i++)
            t->items[i] = val_list(in->stack[slot])->items[i];
        in->stack[slot] = val_obj(&t->obj);
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

static int  m_keys(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (collect(in, base, 0, 0, out));
}

static int  m_values(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (collect(in, base, 1, 0, out));
}

static int  m_items(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (collect(in, base, 2, 0, out));
}

static int  m_to_list(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (collect(in, base, 0, 0, out));
}

static int  m_to_tuple(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (collect(in, base, 0, 1, out));
}

/* remove(k): KeyError in a dict, ValueError in a set, if it is not there. */
static int  m_remove(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    size_t  pos;
    int     r;

    (void)nargs;
    r = lookup(call, SELF, ARG(0), val_is_set(SELF) ? "a set element"
            : "a dict key", &pos, err);
    if (r == -1)
        return (-1);
    if (r == 0 && val_is_dict(SELF))
        return (key_error(in, call, ARG(0), err));
    if (r == 0)
        return (rt_error(err, ERR_VALUE, call, "remove(): the value is not in"
                " the set"));
    rt_table_remove(val_dict(SELF), pos);
    *out = SELF;
    return (0);
}

static int  m_clear(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    rt_table_clear(val_dict(SELF));
    *out = SELF;
    return (0);
}

/*
 * A new table of the same kind as stack[from] with its entries (and,
 * with filter, only those whose key is / is not in stack[other]).
 * filter: 0 all, 1 in other, 2 not in other.
 */
static int  copy_table(t_interp *in, const t_node *call, size_t from,
                size_t other, int filter, t_value *out, t_error *err)
{
    size_t  slot;
    size_t  i;
    int     has;
    t_entry e;

    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap,
                    in->stack[from].as.obj->type)->obj));
    for (i = 0; i < val_dict(in->stack[from])->nentries; i++)
    {
        e = val_dict(in->stack[from])->entries[i];
        if (!e.used)
            continue ;
        if (filter && rt_table_has(call, in->stack[other], e.key, &has, err)
            == -1)
            return (-1);
        if ((filter == 1 && !has) || (filter == 2 && has))
            continue ;
        if (rt_table_set(&in->heap, val_dict(in->stack[slot]), e.key, e.hash,
                e.value) == -1)
            return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

static int  m_copy(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (copy_table(in, call, base, base, 0, out, err));
}

/* ---------- set ---------- */

static int  need_set(const t_node *call, const char *what, t_value v,
                t_error *err)
{
    if (val_is_set(v))
        return (0);
    return (rt_error(err, ERR_TYPE, call, "%s() needs a set, not '%s'", what,
            val_type_name(v)));
}

static int  m_add(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    *out = SELF;
    return (rt_table_put(in, call, base, ARG(0), val_none(), err));
}

static int  m_has(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    int has;

    (void)nargs;
    if (rt_table_has(call, SELF, ARG(0), &has, err) == -1)
        return (-1);
    *out = val_bool(has);
    return (0);
}

/* union: a copy of self with the elements of the other added. */
static int  m_union(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    size_t  slot;
    size_t  i;
    t_entry e;

    (void)nargs;
    if (need_set(call, "union", ARG(0), err) == -1
        || copy_table(in, call, base, base, 0, out, err) == -1)
        return (-1);
    slot = in->top;
    rt_push(in, *out);
    for (i = 0; i < val_dict(ARG(0))->nentries; i++)
    {
        e = val_dict(ARG(0))->entries[i];
        if (e.used && rt_table_set(&in->heap, val_dict(in->stack[slot]),
                e.key, e.hash, val_none()) == -1)
            return (rt_error(err, ERR_RECURSION, call, "stack exhausted"));
    }
    in->top = slot;
    return (0);
}

static int  m_intersection(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    if (need_set(call, "intersection", ARG(0), err) == -1)
        return (-1);
    return (copy_table(in, call, base, base + 1, 1, out, err));
}

static int  m_difference(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    if (need_set(call, "difference", ARG(0), err) == -1)
        return (-1);
    return (copy_table(in, call, base, base + 1, 2, out, err));
}

/* sum() / min() / max() of a set: on a list of its elements. */
static int  on_list(t_interp *in, size_t base, const char *method,
                const t_node *call, t_value *out, t_error *err)
{
    t_name  name;
    size_t  slot;
    int     status;

    slot = in->top;
    collect(in, base, 0, 0, out);
    rt_push(in, *out);
    name.ptr = method;
    name.len = 3;
    status = rt_call_method(in, call, name, slot, 0, out, err);
    in->top = slot;
    return (status);
}

static int  m_sum(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (on_list(in, base, "sum", call, out, err));
}

static int  m_min(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (on_list(in, base, "min", call, out, err));
}

static int  m_max(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (on_list(in, base, "max", call, out, err));
}

const t_method  g_dict_methods[] = {
    {"len", 0, 0, m_len, NULL},
    {"get", 1, 2, m_get, NULL},
    {"keys", 0, 0, m_keys, NULL},
    {"values", 0, 0, m_values, NULL},
    {"items", 0, 0, m_items, NULL},
    {"remove", 1, 1, m_remove, NULL},
    {"clear", 0, 0, m_clear, NULL},
    {"copy", 0, 0, m_copy, NULL},
    {NULL, 0, 0, NULL, NULL},
};

const t_method  g_set_methods[] = {
    {"len", 0, 0, m_len, NULL},
    {"add", 1, 1, m_add, NULL},
    {"remove", 1, 1, m_remove, NULL},
    {"has", 1, 1, m_has, NULL},
    {"union", 1, 1, m_union, NULL},
    {"intersection", 1, 1, m_intersection, NULL},
    {"difference", 1, 1, m_difference, NULL},
    {"clear", 0, 0, m_clear, NULL},
    {"copy", 0, 0, m_copy, NULL},
    {"to_list", 0, 0, m_to_list, NULL},
    {"to_tuple", 0, 0, m_to_tuple, NULL},
    {"sum", 0, 0, m_sum, NULL},
    {"min", 0, 0, m_min, NULL},
    {"max", 0, 0, m_max, NULL},
    {NULL, 0, 0, NULL, NULL},
};
