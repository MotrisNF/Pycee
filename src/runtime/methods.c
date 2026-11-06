/*
 * Method dispatch: x.name(...) looks in the table of x's type, then in
 * the table every value shares. Tables end with a NULL name. Lookup is by
 * name for now; interning names (phase 11) will make it a pointer check.
 */

#include <string.h>
#include "runtime/interp_internal.h"

static int  name_is(t_name name, const char *s)
{
    return (name.len == strlen(s) && memcmp(name.ptr, s, name.len) == 0);
}

/* x.type(): "number", "string", "bool", "none"... */
static int  m_type(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const char  *name;
    t_string    *s;

    (void)nargs;
    (void)call;
    (void)err;
    name = val_type_name(in->stack[base]);
    s = heap_string(&in->heap, name, strlen(name));
    *out = val_obj(&s->obj);
    return (0);
}

/*
 * An object is each obj of its chain, and, if that ends in an error
 * type, each error type from there to Error.
 */
static int  instance_is(const t_node *cls, const t_string *wanted)
{
    int type;

    for (; cls; cls = cls->as.obj.base)
    {
        if (strlen(cls->as.obj.cname) == wanted->len
            && memcmp(cls->as.obj.cname, wanted->chars, wanted->len) == 0)
            return (1);
        if (cls->as.obj.error_base != -1)
        {
            type = error_type_named(wanted->chars, wanted->len);
            return (type != -1 && error_is_a(
                    (t_error_type)cls->as.obj.error_base, (t_error_type)type));
        }
    }
    return (0);
}

/*
 * x.is_type("list"): the same as x.type() is "list". An error also is
 * each type it descends from: ValueError("x").is_type("Error").
 */
static int  m_is_type(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    const char  *name;
    t_string    *wanted;
    int         type;

    (void)nargs;
    if (!val_is_string(in->stack[base + 1]))
        return (rt_error(err, ERR_TYPE, call, "is_type() needs the name of a"
                " type as a string, not '%s'",
                val_type_name(in->stack[base + 1])));
    name = val_type_name(in->stack[base]);
    wanted = val_string(in->stack[base + 1]);
    if (val_is_obj(in->stack[base], OBJ_INSTANCE))
    {
        *out = val_bool(instance_is(((t_instance *)in->stack[base].as.obj)
                    ->cls, wanted));
        return (0);
    }
    if (val_is_obj(in->stack[base], OBJ_ERROR))
    {
        type = error_type_named(wanted->chars, wanted->len);
        *out = val_bool(type != -1 && error_is_a(
                    ((t_error_value *)in->stack[base].as.obj)->err.type,
                    (t_error_type)type));
        return (0);
    }
    *out = val_bool(wanted->len == strlen(name)
            && memcmp(wanted->chars, name, wanted->len) == 0);
    return (0);
}

/*
 * x.id(): which value it is, the same for every name that holds it
 * (var b = a), another for a copy. Numbers, bools and none are plain
 * values, not one of a kind: they have none.
 */
static int  m_id(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    if (in->stack[base].type != VAL_OBJ)
        return (rt_error(err, ERR_TYPE, call, "a %s has no id(): numbers,"
                " bools and none are plain values",
                val_type_name(in->stack[base])));
    *out = val_int((int64_t)in->stack[base].as.obj->id);
    return (0);
}

static const t_method   g_common_methods[] = {
    {"type", 0, 0, m_type, NULL},
    {"is_type", 1, 1, m_is_type, NULL},
    {"id", 0, 0, m_id, NULL},
    {NULL, 0, 0, NULL, NULL},
};

static const t_method   *type_methods(t_value v)
{
    if (v.type == VAL_INT || v.type == VAL_FLOAT)
        return (g_number_methods);
    if (val_is_tuple(v))
        return (g_tuple_methods);
    if (val_is_list(v))
        return (g_list_methods);
    if (val_is_dict(v))
        return (g_dict_methods);
    if (val_is_set(v))
        return (g_set_methods);
    if (val_is_string(v))
        return (g_string_methods);
    if (val_is_obj(v, OBJ_ERROR))
        return (g_error_methods);
    if (val_is_obj(v, OBJ_FILE))
        return (g_file_methods);
    return (NULL);
}

static const t_method   *find_method(const t_method *table, t_name name)
{
    if (!table)
        return (NULL);
    for (; table->name; table++)
        if (name_is(name, table->name))
            return (table);
    return (NULL);
}

/* The method of v called name: its type's, or a common one. NULL if none. */
const t_method  *rt_find_method(t_value v, t_name name)
{
    const t_method  *m;

    m = find_method(type_methods(v), name);
    if (!m)
        m = find_method(g_common_methods, name);
    return (m);
}

int rt_has_method(t_value v, t_name name)
{
    return (rt_find_method(v, name) != NULL);
}

/* Runs m on self (in->stack[base]) and its arguments after it. */
int rt_run_method(t_interp *in, const t_node *call, const t_method *m,
        size_t base, int nargs, t_value *out, t_error *err)
{
    if (rt_check_arity(call, m->name, m->min, m->max, nargs, err) == -1)
        return (-1);
    return (m->fn(in, base, nargs, call, out, err));
}

/* The same messages as the resolver's check of direct calls. */
int rt_check_arity(const t_node *call, const char *name, int min, int max,
        int nargs, t_error *err)
{
    if (nargs >= min && (max == -1 || nargs <= max))
        return (0);
    if (max == -1)
        return (rt_error(err, ERR_TYPE, call, "%s() takes at least %d"
                " argument%s but %d %s given", name, min, min == 1 ? "" : "s",
                nargs, nargs == 1 ? "was" : "were"));
    if (min != max)
        return (rt_error(err, ERR_TYPE, call, "%s() takes %d to %d arguments"
                " but %d %s given", name, min, max, nargs,
                nargs == 1 ? "was" : "were"));
    return (rt_error(err, ERR_TYPE, call, "%s() takes %d argument%s but %d %s"
            " given", name, min, min == 1 ? "" : "s", nargs,
            nargs == 1 ? "was" : "were"));
}

/* self is in->stack[base], its arguments follow it. */
int rt_call_method(t_interp *in, const t_node *call, t_name name,
        size_t base, int nargs, t_value *out, t_error *err)
{
    const t_method  *m;
    t_value         self;

    self = in->stack[base];
    m = rt_find_method(self, name);
    if (!m)
        return (rt_error(err, ERR_ATTRIBUTE, call, "'%s' object has no"
                " attribute '%.*s'", val_type_name(self), (int)name.len,
                name.ptr));
    return (rt_run_method(in, call, m, base, nargs, out, err));
}
