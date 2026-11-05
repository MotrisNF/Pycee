/*
 * Objects of an obj: building one, its fields and methods (its own and
 * those of what it extends), static members, super, and the error inside
 * an obj that extends an error type. The resolver already checked what
 * it could see (self.x, super.m, Name.member); here are the rest.
 */

#include <string.h>
#include "runtime/interp_internal.h"

static int  name_eq(t_name a, t_name b)
{
    return (a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0);
}

static t_name   member_name(const t_node *m)
{
    return (m->type == NODE_VAR ? m->as.var.name : m->as.func.name);
}

/* A member of cls or of what it extends (the nearest), or NULL. */
const t_node    *rt_obj_find(const t_node *cls, t_name name)
{
    const t_node    *m;

    for (; cls; cls = cls->as.obj.base)
        for (m = cls->as.obj.members; m; m = m->next)
            if (name_eq(member_name(m), name))
                return (m);
    return (NULL);
}

static t_value  *static_slot(t_interp *in, const t_node *var)
{
    return (&in->modules[var->as.var.cls->as.obj.module]
        .values[var->as.var.slot]);
}

/*
 * The values of the fields, the parents' first, each computed in a frame
 * of the obj's own (init_slots); the object is at stack[obj].
 */
static int  init_fields(t_interp *in, const t_node *cls, size_t obj,
                t_error *err)
{
    const t_node    *m;
    size_t          frame;
    size_t          saved;
    t_cell          **cells;
    t_value         v;
    int             i;
    int             status;

    if (cls->as.obj.base && init_fields(in, cls->as.obj.base, obj, err) == -1)
        return (-1);
    for (m = cls->as.obj.members; m; m = m->next)
    {
        if (m->type != NODE_VAR || m->as.var.is_static || !m->as.var.init)
            continue ;
        frame = in->top;
        for (i = 0; i < cls->as.obj.init_slots; i++)
            rt_push(in, val_none());
        saved = in->frame;
        cells = in->cells;
        in->frame = frame;
        in->cells = NULL;
        status = rt_eval(in, m->as.var.init, &v, err);
        in->frame = saved;
        in->cells = cells;
        in->top = frame;
        if (status == -1)
            return (-1);
        ((t_instance *)in->stack[obj].as.obj)->fields[m->as.var.slot] = v;
    }
    return (0);
}

/* An obj that extends an error type: the message of its error. */
static void set_message(t_interp *in, t_instance *o, t_value msg)
{
    buf_clear(&in->scratch);
    rt_format(&in->scratch, msg, 0);
    error_set(&o->error->err, o->error->err.type, o->error->err.offset,
        o->error->err.len, "%.*s", (int)in->scratch.len,
        in->scratch.data ? in->scratch.data : "");
}

/*
 * Name(...): a new object with every field none, then their values,
 * then create (its own or inherited) on it. Without a create, an obj
 * that extends an error type takes its message, like ValueError("x").
 */
int rt_construct(t_interp *in, const t_node *call, const t_node *cls,
        size_t argbase, int npos, const t_node *named, t_value *out,
        t_error *err)
{
    t_instance      *o;
    t_error_value   *e;
    const t_node    *create;
    t_value         self;
    t_value         ignored;
    t_name          name;
    size_t          slot;
    size_t          i;

    o = (t_instance *)heap_alloc(&in->heap, OBJ_INSTANCE, sizeof(t_instance)
            + (size_t)cls->as.obj.nfields * sizeof(t_value));
    o->cls = cls;
    o->error = NULL;
    o->nfields = (size_t)cls->as.obj.nfields;
    for (i = 0; i < o->nfields; i++)
        o->fields[i] = val_none();
    slot = in->top;
    rt_push(in, val_obj(&o->obj));
    if (cls->as.obj.error_base != -1)
    {
        e = (t_error_value *)heap_alloc(&in->heap, OBJ_ERROR,
                sizeof(t_error_value));
        error_set(&e->err, (t_error_type)cls->as.obj.error_base,
            call->offset, call->len, "%s", "");
        e->raised = 0;
        ((t_instance *)in->stack[slot].as.obj)->error = e;
    }
    if (init_fields(in, cls, slot, err) == -1)
        return (-1);
    name.ptr = "create";
    name.len = 6;
    create = rt_obj_find(cls, name);
    self = in->stack[slot];
    if (create && create->type == NODE_FUNC)
    {
        if (rt_call_method_func(in, call, create, &self, argbase, npos, named,
                &ignored, err) == -1)
            return (-1);
    }
    else if (named || npos > (cls->as.obj.error_base != -1))
        return (rt_error(err, ERR_TYPE, call, "%s() takes %s", cls->as.obj.cname,
                cls->as.obj.error_base != -1 ? "an optional message"
                : "no arguments"));
    else if (npos == 1)
        set_message(in, (t_instance *)self.as.obj, in->stack[argbase]);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/* A method value of f bound to the object at stack[base]. */
static int  method_value(t_interp *in, size_t base, const t_node *f,
                t_value *out)
{
    t_method_value  *m;

    m = (t_method_value *)heap_alloc(&in->heap, OBJ_METHOD,
            sizeof(t_method_value));
    m->self = in->stack[base];
    m->func = f;
    *out = val_obj(&m->obj);
    return (0);
}

static int  no_attr(const t_node *n, const char *type, t_error *err)
{
    return (rt_error(err, ERR_ATTRIBUTE, n, "'%s' object has no attribute"
            " '%.*s'", type, (int)n->as.field.name.len, n->as.field.name.ptr));
}

/*
 * Whether x.name is self.name: self is a keyword, so a name "self" can
 * only be the hidden parameter of a method (the resolver checked that a
 * private member reached this way is its own obj's).
 */
static int  via_self(const t_node *n)
{
    const t_node    *o;

    o = n->as.field.object;
    return (o->type == NODE_IDENT && o->as.ident.name.len == 4
        && memcmp(o->as.ident.name.ptr, "self", 4) == 0);
}

/* A private member is only for self, inside its obj. */
static int  check_private(const t_node *n, const t_node *m, t_error *err)
{
    int priv;

    priv = m->type == NODE_VAR ? m->as.var.is_private : m->as.func.is_private;
    if (!priv || via_self(n))
        return (0);
    return (rt_error(err, ERR_ATTRIBUTE, n, "'%.*s' is private: only self"
            " can reach it, inside '%s'", (int)n->as.field.name.len,
            n->as.field.name.ptr, (m->type == NODE_VAR ? m->as.var.cls
                : m->as.func.cls)->as.obj.cname));
}

/* p.type as a value: the methods every value has. */
static int  common_method(t_interp *in, const t_node *n, size_t base,
                t_value *out, t_error *err)
{
    const t_method  *m;
    t_bound         *b;

    m = rt_find_method(in->stack[base], n->as.field.name);
    if (!m)
        return (no_attr(n, val_type_name(in->stack[base]), err));
    b = (t_bound *)heap_alloc(&in->heap, OBJ_BOUND, sizeof(t_bound));
    b->self = in->stack[base];
    b->method = m;
    *out = val_obj(&b->obj);
    return (0);
}

/* e.message as a value, on an obj that extends an error type. */
static int  error_method(t_interp *in, const t_node *n, t_instance *o,
                t_value *out, t_error *err)
{
    const t_method  *m;
    t_bound         *b;

    m = rt_find_method(val_obj(&o->error->obj), n->as.field.name);
    if (!m)
        return (no_attr(n, o->cls->as.obj.cname, err));
    b = (t_bound *)heap_alloc(&in->heap, OBJ_BOUND, sizeof(t_bound));
    b->self = val_obj(&o->error->obj);
    b->method = m;
    *out = val_obj(&b->obj);
    return (0);
}

/* x.name on an object or an obj (at stack[base]): a field, a method... */
int rt_get_attr(t_interp *in, const t_node *n, size_t base, t_value *out,
        t_error *err)
{
    const t_node    *m;
    t_value         v;
    t_instance      *o;

    v = in->stack[base];
    if (val_is_obj(v, OBJ_INSTANCE) && n->as.field.slot >= 0)
    {
        *out = ((t_instance *)v.as.obj)->fields[n->as.field.slot];
        return (0);
    }
    m = rt_obj_find(val_is_obj(v, OBJ_CLASS) ? ((t_class *)v.as.obj)->decl
            : ((t_instance *)v.as.obj)->cls, n->as.field.name);
    o = val_is_obj(v, OBJ_INSTANCE) ? (t_instance *)v.as.obj : NULL;
    if (!m && o && o->error)
        return (error_method(in, n, o, out, err));
    if (!m)
        return (common_method(in, n, base, out, err));
    if (check_private(n, m, err) == -1)
        return (-1);
    if (m->type == NODE_VAR && m->as.var.is_static)
        *out = *static_slot(in, m);
    else if (m->type == NODE_VAR && o)
        *out = o->fields[m->as.var.slot];
    else if (m->type == NODE_FUNC && m->as.func.is_static)
        return (rt_make_function(in, m, out));
    else if (m->type == NODE_FUNC && o)
        return (method_value(in, base, m, out));
    else
        return (rt_error(err, ERR_TYPE, n, "'%.*s' is not static: reach it"
                " through an object", (int)n->as.field.name.len,
                n->as.field.name.ptr));
    return (0);
}

/* x.name = v on an object or an obj (at stack[base]). */
int rt_set_attr(t_interp *in, const t_node *n, size_t base, t_value v,
        t_error *err)
{
    const t_node    *m;
    t_value         x;

    x = in->stack[base];
    if (!val_is_obj(x, OBJ_INSTANCE) && !val_is_obj(x, OBJ_CLASS))
        return (rt_error(err, ERR_ATTRIBUTE, n, "a '%s' has no fields to"
                " set", val_type_name(x)));
    if (val_is_obj(x, OBJ_INSTANCE) && n->as.field.slot >= 0)
    {
        ((t_instance *)x.as.obj)->fields[n->as.field.slot] = v;
        return (0);
    }
    m = rt_obj_find(val_is_obj(x, OBJ_CLASS) ? ((t_class *)x.as.obj)->decl
            : ((t_instance *)x.as.obj)->cls, n->as.field.name);
    if (!m)
        return (no_attr(n, val_type_name(x), err));
    if (check_private(n, m, err) == -1)
        return (-1);
    if (m->type == NODE_FUNC)
        return (rt_error(err, ERR_TYPE, n, "cannot assign to a method"));
    if (m->as.var.is_static)
        *static_slot(in, m) = v;
    else if (val_is_obj(x, OBJ_INSTANCE))
        ((t_instance *)x.as.obj)->fields[m->as.var.slot] = v;
    else
        return (rt_error(err, ERR_TYPE, n, "'%.*s' is not static: reach it"
                " through an object", (int)n->as.field.name.len,
                n->as.field.name.ptr));
    return (0);
}

/*
 * x.name(...) on an object or an obj (x at stack[base], the arguments
 * after it): a method (on x), a static one, super's, or a field that
 * holds a function.
 */
int rt_call_attr(t_interp *in, const t_node *call, size_t base, int npos,
        const t_node *named, t_value *out, t_error *err)
{
    const t_node    *n;
    const t_node    *m;
    t_instance      *o;
    t_value         self;

    n = call->as.call.callee;
    self = in->stack[base];
    o = val_is_obj(self, OBJ_INSTANCE) ? (t_instance *)self.as.obj : NULL;
    if (n->as.field.slot == -3)
        return (rt_call_method_func(in, call, n->as.field.member, &self,
                base + 1, npos, named, out, err));
    if (n->as.field.slot == -2 && o && o->error)
    {
        if (npos == 1)
            set_message(in, o, in->stack[base + 1]);
        *out = val_none();
        return (0);
    }
    m = rt_obj_find(o ? o->cls : ((t_class *)self.as.obj)->decl,
            n->as.field.name);
    if (m && m->type == NODE_FUNC && (m->as.func.is_static || o))
    {
        if (check_private(n, m, err) == -1)
            return (-1);
        return (rt_call_method_func(in, call, m, m->as.func.is_static ? NULL
                : &self, base + 1, npos, named, out, err));
    }
    if (!m && o && o->error && rt_find_method(val_obj(&o->error->obj),
            n->as.field.name) != rt_find_method(self, n->as.field.name))
    {
        in->stack[base] = val_obj(&o->error->obj);
        return (rt_call_method(in, call, n->as.field.name, base, npos, out,
                err));
    }
    if (!m)
        return (rt_call_method(in, call, n->as.field.name, base, npos, out,
                err));
    if (rt_get_attr(in, n, base, &self, err) == -1)
        return (-1);
    in->stack[base] = self;
    if (named)
        return (rt_error(err, ERR_TYPE, call, "named arguments only go to a"
                " function or method called directly"));
    return (rt_call(in, call, base, npos, out, err));
}

/* The method name of v's obj (not a static one), or NULL. */
const t_node    *rt_special(t_value v, const char *name)
{
    const t_node    *f;
    t_name          n;

    if (!val_is_obj(v, OBJ_INSTANCE))
        return (NULL);
    n.ptr = name;
    n.len = strlen(name);
    f = rt_obj_find(((const t_instance *)v.as.obj)->cls, n);
    if (f && f->type == NODE_FUNC && !f->as.func.is_static)
        return (f);
    return (NULL);
}

/*
 * v.name(...) for code in C (the language calling a method: get, set,
 * iter, has_next, next): v at stack[base], its arguments after it. An
 * object without that method in its obj is an AttributeError; any other
 * value uses its type's methods.
 */
int rt_call_named(t_interp *in, const t_node *call, size_t base,
        const char *name, int npos, t_value *out, t_error *err)
{
    const t_node    *f;
    t_value         self;
    t_name          n;

    self = in->stack[base];
    f = rt_special(self, name);
    if (f)
        return (rt_call_method_func(in, call, f, &self, base + 1, npos, NULL,
                out, err));
    if (val_is_obj(self, OBJ_INSTANCE))
        return (rt_error(err, ERR_TYPE, call, "'%s' has no %s() method",
                val_type_name(self), name));
    n.ptr = name;
    n.len = strlen(name);
    return (rt_call_method(in, call, n, base, npos, out, err));
}
