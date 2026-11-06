/*
 * Calls. From base, a call pushes the callee (when it is a value), then
 * its arguments: the positional ones (...t spreads a tuple into them),
 * then the named ones. A function's frame goes right after them, so
 * everything the call uses stays rooted until it returns.
 */

#include <string.h>
#include "resolver/natives.h"
#include "runtime/interp_internal.h"

/* "double", or "lambda" for a function without a name. */
static t_name   func_name(const t_node *f)
{
    t_name  name;

    if (!f->as.func.is_lambda)
        return (f->as.func.name);
    name.ptr = "lambda";
    name.len = 6;
    return (name);
}

/*
 * A function value for decl, with the cells of what it captures: from
 * the running frame (the variable's cell, made now if it has none yet)
 * or from the running closure. The new function stays on the stack while
 * its cells are taken, and ncells grows as they are set.
 */
int rt_make_function(t_interp *in, const t_node *decl, t_value *out)
{
    t_function      *fn;
    const t_capture *cap;
    t_cell          *cell;
    size_t          top;
    size_t          slot;

    fn = (t_function *)heap_alloc(&in->heap, OBJ_FUNCTION, sizeof(t_function)
            + (size_t)decl->as.func.ncaptures * sizeof(t_cell *));
    fn->decl = decl;
    fn->ncells = 0;
    top = in->top;
    rt_push(in, val_obj(&fn->obj));
    for (cap = decl->as.func.captures; cap; cap = cap->next)
    {
        if (cap->from_local)
        {
            slot = in->frame + (size_t)cap->index;
            if (val_is_obj(in->stack[slot], OBJ_CELL))
                cell = (t_cell *)in->stack[slot].as.obj;
            else
                cell = rt_box(in, slot);
        }
        else
            cell = in->cells[cap->index];
        fn->cells[fn->ncells++] = cell;
    }
    in->top = top;
    *out = val_obj(&fn->obj);
    return (0);
}

/*
 * Pushes the arguments: positional ones first (the items of ...t one by
 * one, t a tuple or a list: pushing never allocates on the heap, so t
 * needs no root meanwhile), then the values of the named ones. *named is the first
 * named argument (they all come after the positional ones).
 */
static int  push_args(t_interp *in, t_node *arg, int *npos, t_node **named,
                t_error *err)
{
    t_value         v;
    size_t          i;
    size_t          len;

    *npos = 0;
    *named = NULL;
    for (; arg; arg = arg->next)
    {
        if (arg->type == NODE_PAIR)
        {
            if (rt_eval(in, arg->as.pair.key, &v, err) == -1)
                return (-1);
            rt_push(in, v);
            if (rt_eval(in, arg->as.pair.value, &v, err) == -1)
                return (-1);
            rt_push(in, v);
            *npos += 2;
            continue ;
        }
        if (arg->type == NODE_NAMED_ARG && !*named)
            *named = arg;
        if (rt_eval(in, arg->type == NODE_NAMED_ARG ? arg->as.named.value
                : arg->type == NODE_SPREAD ? arg->as.spread.value : arg,
                &v, err) == -1)
            return (-1);
        if (arg->type != NODE_SPREAD)
        {
            rt_push(in, v);
            *npos += (arg->type != NODE_NAMED_ARG);
            continue ;
        }
        if (!val_is_tuple(v) && !val_is_list(v))
            return (rt_error(err, ERR_TYPE, arg, "'...' needs a tuple or a"
                    " list, not '%s'", val_type_name(v)));
        rt_seq_items(v, &len);
        for (i = 0; i < len; i++)
            rt_push(in, rt_seq_items(v, &len)[i]);
        *npos += (int)len;
    }
    return (0);
}

/* The parameter a named argument fills (the resolver's, when it knew f). */
static int  named_index(const t_node *f, const t_node *arg)
{
    const t_node    *p;
    int             i;

    if (arg->as.named.index >= 0)
        return (arg->as.named.index);
    i = 0;
    for (p = f->as.func.params; p; p = p->next, i++)
        if (!p->as.param.is_rest && p->as.param.name.len
            == arg->as.named.name.len && memcmp(p->as.param.name.ptr,
                arg->as.named.name.ptr, arg->as.named.name.len) == 0)
            return (i);
    return (-1);
}

static int  given_by_name(const t_node *f, const t_node *named, int i)
{
    for (; named; named = named->next)
        if (named_index(f, named) == i)
            return (1);
    return (0);
}

/* The same message as the resolver's for a call it could check. */
static int  arity_error(const t_node *call, const t_node *f, int npos,
                t_error *err)
{
    t_name  fn;
    int     min;
    int     max;

    fn = func_name(f);
    min = f->as.func.nrequired;
    max = f->as.func.has_rest ? -1 : f->as.func.nparams;
    if (max == -1)
        return (rt_error(err, ERR_TYPE, call, "%.*s() takes at least %d"
                " argument%s but %d %s given", (int)fn.len, fn.ptr, min,
                min == 1 ? "" : "s", npos, npos == 1 ? "was" : "were"));
    if (min != max)
        return (rt_error(err, ERR_TYPE, call, "%.*s() takes %d to %d"
                " arguments but %d %s given", (int)fn.len, fn.ptr, min, max,
                npos, npos == 1 ? "was" : "were"));
    return (rt_error(err, ERR_TYPE, call, "%.*s() takes %d argument%s but %d"
            " %s given", (int)fn.len, fn.ptr, min, min == 1 ? "" : "s", npos,
            npos == 1 ? "was" : "were"));
}

/*
 * Puts what the call gave into the new frame: positional arguments in
 * order, the rest of them as a tuple in ...rest, named ones in their
 * parameter. Checks what the resolver could not (a call through a value,
 * or with ...t).
 */
static int  bind_args(t_interp *in, const t_node *call, const t_node *f,
                size_t argbase, int npos, const t_node *named, size_t frame,
                t_error *err)
{
    const t_node    *a;
    const t_node    *b;
    t_tuple         *rest;
    t_name          fn;
    int             fixed;
    int             i;
    int             j;

    fn = func_name(f);
    fixed = f->as.func.nparams - f->as.func.has_rest;
    if ((npos > fixed && !f->as.func.has_rest)
        || (!named && npos < f->as.func.nrequired))
        return (arity_error(call, f, npos, err));
    frame += (f->as.func.self != NULL);
    for (i = 0; i < npos && i < fixed; i++)
        in->stack[frame + i] = in->stack[argbase + i];
    if (f->as.func.has_rest)
    {
        rest = heap_tuple(&in->heap, npos > fixed ? (size_t)(npos - fixed) : 0);
        for (i = fixed; i < npos; i++)
            rest->items[i - fixed] = in->stack[argbase + i];
        in->stack[frame + fixed] = val_obj(&rest->obj);
    }
    j = 0;
    for (a = named; a; a = a->next, j++)
    {
        i = named_index(f, a);
        if (i == -1)
            return (rt_error(err, ERR_TYPE, a, "%.*s() has no parameter"
                    " '%.*s'", (int)fn.len, fn.ptr, (int)a->as.named.name.len,
                    a->as.named.name.ptr));
        for (b = named; b != a && named_index(f, b) != i; b = b->next)
            ;
        if (i < npos || b != a)
            return (rt_error(err, ERR_TYPE, a, "%.*s() got two values for"
                    " '%.*s'", (int)fn.len, fn.ptr, (int)a->as.named.name.len,
                    a->as.named.name.ptr));
        in->stack[frame + i] = in->stack[argbase + npos + j];
    }
    return (0);
}

/*
 * Inside the new frame: each parameter not given takes its default
 * (which may use the ones before it), in order; then, if an inner
 * function captures it, it moves to a cell.
 */
static int  fill_params(t_interp *in, const t_node *call, const t_node *f,
                int npos, const t_node *named, t_error *err)
{
    const t_node    *p;
    t_name          fn;
    t_value         v;
    size_t          frame;
    int             i;

    if (f->as.func.self && f->as.func.self->as.param.captured)
        rt_box(in, in->frame);
    frame = in->frame + (f->as.func.self != NULL);
    i = 0;
    for (p = f->as.func.params; p; p = p->next, i++)
    {
        if (!p->as.param.is_rest && i >= npos && !given_by_name(f, named, i))
        {
            fn = func_name(f);
            if (!p->as.param.default_)
                return (rt_error(err, ERR_TYPE, call, "%.*s() is missing"
                        " argument '%.*s'", (int)fn.len, fn.ptr,
                        (int)p->as.param.name.len, p->as.param.name.ptr));
            if (rt_eval(in, p->as.param.default_, &v, err) == -1)
                return (-1);
            in->stack[frame + (size_t)i] = v;
        }
        if (p->as.param.captured)
            rt_box(in, frame + (size_t)i);
    }
    return (0);
}

/*
 * Runs f with the arguments at argbase. cells are its captures (NULL for
 * a func of the top level); self, for a method, goes in slot 0 (it is
 * rooted where the caller has it). An error that leaves the body adds
 * this call to the traceback; one in the arguments is the caller's.
 */
static int  call_function(t_interp *in, const t_node *call, const t_node *f,
                t_cell **cells, size_t argbase, int npos, const t_node *named,
                const t_value *self, t_value *out, t_error *err)
{
    size_t  frame;
    size_t  saved_frame;
    t_cell  **saved_cells;
    t_name  saved_name;
    t_exec  status;
    t_value v;
    t_name  name;
    int     i;

    if (in->depth >= in->max_depth)
        return (rt_error(err, ERR_RECURSION, call, "maximum recursion depth"
                " exceeded (%d calls)", in->max_depth));
    if (RT_POLL(in, call, err) == -1)
        return (-1);
    frame = in->top;
    for (i = 0; i < f->as.func.nslots; i++)
        rt_push(in, val_none());
    if (f->as.func.self)
        in->stack[frame] = self ? *self : val_none();
    if (bind_args(in, call, f, argbase, npos, named, frame, err) == -1)
        return (-1);
    saved_frame = in->frame;
    saved_cells = in->cells;
    saved_name = in->func_name;
    in->frame = frame;
    in->cells = cells;
    in->func_name = func_name(f);
    in->depth++;
    v = val_none();
    if (fill_params(in, call, f, npos, named, err) == -1)
        status = EXEC_ERROR;
    else if (f->as.func.expr_body)
        status = rt_eval(in, f->as.func.body, &v, err) == -1
            ? EXEC_ERROR : EXEC_OK;
    else
    {
        status = rt_exec_block(in, f->as.func.body, err);
        if (status == EXEC_RETURN)
            v = in->ret;
    }
    in->ret = val_none();
    in->depth--;
    in->frame = saved_frame;
    in->cells = saved_cells;
    in->func_name = saved_name;
    if (status == EXEC_ERROR)
    {
        name = func_name(f);
        error_add_frame(err, call->offset, call->len, name.ptr, name.len);
        return (-1);
    }
    *out = v;
    return (0);
}

static int  no_named(const t_node *call, const t_node *named,
                const char *name, t_error *err)
{
    if (!named)
        return (0);
    return (rt_error(err, ERR_TYPE, call, "%s() takes no named arguments",
            name));
}

/* Which argument of m a name is (m->names), or -1. */
static int  method_param(const t_method *m, t_name name)
{
    int i;

    for (i = 0; m->names && m->names[i]; i++)
        if (strlen(m->names[i]) == name.len
            && memcmp(m->names[i], name.ptr, name.len) == 0)
            return (i);
    return (-1);
}

/*
 * Calls method m on self (stack[base]) with npos positional arguments
 * after it and named ones after those. With names, the arguments are
 * laid out again, in order, after everything (none where one is left
 * out), and the method runs on that copy.
 */
static int  run_method_named(t_interp *in, const t_node *call,
                const t_method *m, size_t base, int npos,
                const t_node *named, t_value *out, t_error *err)
{
    const t_node    *a;
    size_t          args;
    int             nargs;
    int             i;
    int             j;

    if (!named)
        return (rt_run_method(in, call, m, base, npos, out, err));
    if (!m->names)
        return (no_named(call, named, m->name, err));
    nargs = npos;
    for (a = named; a; a = a->next)
    {
        i = method_param(m, a->as.named.name);
        if (i == -1)
            return (rt_error(err, ERR_TYPE, a, "%s() has no parameter"
                    " '%.*s'", m->name, (int)a->as.named.name.len,
                    a->as.named.name.ptr));
        if (i < npos)
            return (rt_error(err, ERR_TYPE, a, "%s() got two values for"
                    " '%s'", m->name, m->names[i]));
        nargs = i + 1 > nargs ? i + 1 : nargs;
    }
    args = in->top;
    rt_push(in, in->stack[base]);
    for (i = 0; i < nargs; i++)
        rt_push(in, i < npos ? in->stack[base + 1 + (size_t)i] : val_none());
    j = 0;
    for (a = named; a; a = a->next, j++)
        in->stack[args + 1 + (size_t)method_param(m, a->as.named.name)]
            = in->stack[base + 1 + (size_t)npos + (size_t)j];
    return (rt_run_method(in, call, m, args, nargs, out, err));
}

/*
 * Where a named argument of a native goes: its position among the
 * positional parameters, -2 - i for keyword-only parameter i, -1 if the
 * native has no parameter called that.
 */
static int  native_slot(const t_native_def *def, t_name name)
{
    int i;

    i = native_param(def->names, name.ptr, name.len);
    if (i != -1)
        return (i);
    i = native_param(def->kw, name.ptr, name.len);
    if (i != -1)
        return (-2 - i);
    return (-1);
}

/*
 * The named arguments of a native call that the resolver could not see
 * (through a value): each one a parameter it has, once, not already given
 * by position. *nargs grows to the last positional one named.
 */
static int  check_native_named(const t_native_def *def, int npos,
                const t_node *named, int *nargs, t_error *err)
{
    const t_node    *a;
    const t_node    *b;
    t_name          name;
    int             i;

    *nargs = npos;
    for (a = named; a; a = a->next)
    {
        name = a->as.named.name;
        i = native_slot(def, name);
        if (i == -1)
            return (rt_error(err, ERR_TYPE, a, "%s() has no parameter"
                    " '%.*s'", def->name, (int)name.len, name.ptr));
        for (b = named; b != a; b = b->next)
            if (b->as.named.name.len == name.len
                && memcmp(b->as.named.name.ptr, name.ptr, name.len) == 0)
                return (rt_error(err, ERR_TYPE, a, "argument '%.*s' is"
                        " given twice", (int)name.len, name.ptr));
        if (i >= 0 && i < npos)
            return (rt_error(err, ERR_TYPE, a, "%s() got two values for"
                    " '%.*s'", def->name, (int)name.len, name.ptr));
        if (i + 1 > *nargs)
            *nargs = i + 1;
    }
    return (0);
}

/*
 * Calls the native in slot with npos positional arguments from base and
 * the values of the named ones after them. A native without parameters
 * that can be named gets them where they are. One with them gets its
 * arguments laid out again after everything: its nargs positional ones
 * (none where one is left out), then one slot per keyword-only one
 * (none when not given), at base + nargs.
 */
int rt_call_native(t_interp *in, const t_node *call, int slot, size_t base,
        int npos, const t_node *named, t_value *out, t_error *err)
{
    const t_native_def  *def;
    const t_node        *a;
    size_t              args;
    int                 nargs;
    int                 nkw;
    int                 i;
    int                 j;
    int                 status;

    def = &g_native_defs[slot];
    if (!def->names && !def->kw)
    {
        if (no_named(call, named, def->name, err) == -1
            || rt_check_arity(call, def->name, def->min, def->max, npos,
                err) == -1)
            return (-1);
        return (g_native_fns[slot](in, base, npos, call, out, err));
    }
    if (check_native_named(def, npos, named, &nargs, err) == -1)
        return (-1);
    if (def->max != -1 && nargs > def->max && native_kw_hint(def))
        return (rt_error(err, ERR_TYPE, call, "%s() takes %d positional"
                " argument%s but %d %s given: %s", def->name, def->max,
                def->max == 1 ? "" : "s", nargs, nargs == 1 ? "was" : "were",
                native_kw_hint(def)));
    if (rt_check_arity(call, def->name, def->min, def->max, nargs, err) == -1)
        return (-1);
    nkw = native_param_count(def->kw);
    args = in->top;
    for (i = 0; i < nargs + nkw; i++)
        rt_push(in, i < npos ? in->stack[base + (size_t)i] : val_none());
    j = 0;
    for (a = named; a; a = a->next, j++)
    {
        i = native_slot(def, a->as.named.name);
        in->stack[args + (size_t)(i >= 0 ? i : nargs - 2 - i)]
            = in->stack[base + (size_t)npos + (size_t)j];
    }
    status = g_native_fns[slot](in, args, nargs, call, out, err);
    in->top = args;
    return (status);
}

/* Calls the value at in->stack[base], its arguments after it. */
static int  call_value(t_interp *in, const t_node *call, size_t base,
                int npos, const t_node *named, t_value *out, t_error *err)
{
    t_value         v;
    t_function      *fn;
    const t_bound   *b;
    int             slot;

    v = in->stack[base];
    if (val_is_obj(v, OBJ_FUNCTION))
    {
        fn = (t_function *)v.as.obj;
        return (call_function(in, call, fn->decl, fn->cells, base + 1, npos,
                named, NULL, out, err));
    }
    if (val_is_obj(v, OBJ_NATIVE))
    {
        slot = ((const t_native *)v.as.obj)->slot;
        return (rt_call_native(in, call, slot, base + 1, npos, named, out,
                err));
    }
    if (val_is_obj(v, OBJ_CLASS))
        return (rt_construct(in, call, ((t_class *)v.as.obj)->decl, base + 1,
                npos, named, out, err));
    if (val_is_obj(v, OBJ_METHOD))
        return (rt_call_method_func(in, call, ((t_method_value *)v.as.obj)
                ->func, &((t_method_value *)v.as.obj)->self, base + 1, npos,
                named, out, err));
    if (!val_is_obj(v, OBJ_BOUND))
        return (rt_error(err, ERR_TYPE, call, "'%s' object is not callable",
                val_type_name(v)));
    b = (const t_bound *)v.as.obj;
    in->stack[base] = b->self;
    return (run_method_named(in, call, b->method, base, npos, named, out,
            err));
}

/*
 * f, a method of an obj, on self (rooted by the caller), with the
 * arguments from argbase (for object.c: building, super, bound methods).
 */
int rt_call_method_func(t_interp *in, const t_node *call, const t_node *f,
        const t_value *self, size_t argbase, int npos, const t_node *named,
        t_value *out, t_error *err)
{
    return (call_function(in, call, f, NULL, argbase, npos, named, self, out,
            err));
}

/*
 * Calls the value at in->stack[base] with the nargs values after it, for
 * code in C that calls a function of the program (l.map(f)...).
 */
int rt_call(t_interp *in, const t_node *call, size_t base, int nargs,
        t_value *out, t_error *err)
{
    return (call_value(in, call, base, nargs, NULL, out, err));
}

/* x.m(...): the method of x called name, with named arguments if it has them. */
static int  call_method(t_interp *in, const t_node *call, t_name name,
                size_t base, int npos, const t_node *named, t_value *out,
                t_error *err)
{
    const t_method  *m;

    if (!named)
        return (rt_call_method(in, call, name, base, npos, out, err));
    m = rt_find_method(in->stack[base], name);
    if (!m)
        return (rt_call_method(in, call, name, base, npos, out, err));
    return (run_method_named(in, call, m, base, npos, named, out, err));
}

/*
 * f(...): a func or a native named directly (no value needed), a method
 * x.m(...), or anything else that gives a callable value.
 */
int rt_eval_call(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_node      *callee;
    t_node      *named;
    const t_ref *ref;
    t_value     v;
    size_t      base;
    int         npos;
    int         status;

    callee = n->as.call.callee;
    ref = &callee->as.ident.ref;
    base = in->top;
    status = 0;
    if (callee->type == NODE_FIELD)
    {
        status = rt_eval(in, callee->as.field.object, &v, err);
        if (status == 0)
            rt_push(in, v);
    }
    else if (callee->type != NODE_IDENT || (ref->kind != REF_NATIVE
            && ref->kind != REF_FUNC && ref->kind != REF_OBJ))
    {
        status = rt_eval(in, callee, &v, err);
        if (status == 0)
            rt_push(in, v);
    }
    if (status == 0)
        status = push_args(in, n->as.call.args, &npos, &named, err);
    if (status == 0 && callee->type == NODE_FIELD
        && (val_is_obj(in->stack[base], OBJ_INSTANCE)
            || val_is_obj(in->stack[base], OBJ_CLASS)))
        status = rt_call_attr(in, n, base, npos, named, out, err);
    else if (status == 0 && callee->type == NODE_FIELD)
        status = call_method(in, n, callee->as.field.name, base, npos,
                named, out, err);
    else if (status == 0 && callee->type == NODE_IDENT
        && ref->kind == REF_OBJ)
        status = rt_construct(in, n, ref->decl, base, npos, named, out, err);
    else if (status == 0 && callee->type == NODE_IDENT
        && ref->kind == REF_NATIVE)
        status = rt_call_native(in, n, ref->slot, base, npos, named, out,
                err);
    else if (status == 0 && callee->type == NODE_IDENT
        && ref->kind == REF_FUNC)
        status = call_function(in, n, ref->decl, NULL, base, npos, named,
                NULL, out, err);
    else if (status == 0)
        status = call_value(in, n, base, npos, named, out, err);
    in->top = base;
    return (status);
}
