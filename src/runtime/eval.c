/*
 * Expressions. rt_eval leaves the value in *out; anything that must
 * survive evaluating something else is pushed on the value stack first
 * (see interp.h), and every function leaves the stack as it found it.
 */

#include "core/stack.h"
#include "runtime/interp_internal.h"

static int  not_yet(t_error *err, const t_node *n, const char *what)
{
    return (rt_not_yet(err, n, what));
}

/*
 * A name: the resolver says where it lives. A func or a native named as
 * a value becomes one now.
 */
static int  eval_name(t_interp *in, const t_node *n, t_value *out,
                t_error *err)
{
    const t_ref *ref;
    t_native    *native;
    t_class     *cls;

    ref = &n->as.ident.ref;
    if (ref->kind == REF_GLOBAL || ref->kind == REF_LOCAL
        || ref->kind == REF_CAPTURE)
    {
        *out = *rt_variable(in, ref);
        return (0);
    }
    if (ref->kind == REF_FUNC)
        return (rt_make_function(in, ref->decl, out));
    if (ref->kind == REF_NATIVE)
    {
        native = (t_native *)heap_alloc(&in->heap, OBJ_NATIVE,
                sizeof(t_native));
        native->slot = ref->slot;
        *out = val_obj(&native->obj);
        return (0);
    }
    (void)err;
    cls = (t_class *)heap_alloc(&in->heap, OBJ_CLASS, sizeof(t_class));
    cls->decl = ref->decl;
    *out = val_obj(&cls->obj);
    return (0);
}

static int  eval_ternary(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_value cond;
    int     b;

    if (rt_eval(in, n->as.ternary.cond, &cond, err) == -1
        || rt_truthy(n->as.ternary.cond, cond, "the condition of '?:'", &b,
            err) == -1)
        return (-1);
    return (rt_eval(in, b ? n->as.ternary.then : n->as.ternary.else_, out,
            err));
}

/* and / or stop as soon as the result is known; both sides are bools. */
static int  eval_logic(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_token_type    op;
    const char      *what;
    t_value         v;
    int             b;

    op = n->as.binary.op;
    what = op == TOK_AND ? "'and'" : "'or'";
    if (rt_eval(in, n->as.binary.left, &v, err) == -1
        || rt_truthy(n->as.binary.left, v, what, &b, err) == -1)
        return (-1);
    if ((op == TOK_AND && !b) || (op == TOK_OR && b))
    {
        *out = val_bool(b);
        return (0);
    }
    if (rt_eval(in, n->as.binary.right, &v, err) == -1
        || rt_truthy(n->as.binary.right, v, what, &b, err) == -1)
        return (-1);
    *out = val_bool(b);
    return (0);
}

/* Both operands go on the stack, in order, and stay there until the result exists. */
static int  eval_binary(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    size_t  top;
    t_value v;
    int     status;

    if (n->as.binary.op == TOK_AND || n->as.binary.op == TOK_OR)
        return (eval_logic(in, n, out, err));
    top = in->top;
    status = rt_eval(in, n->as.binary.left, &v, err);
    if (status == 0)
    {
        rt_push(in, v);
        status = rt_eval(in, n->as.binary.right, &v, err);
    }
    if (status == 0)
    {
        rt_push(in, v);
        status = rt_binary(in, n, n->as.binary.op, out, err);
    }
    in->top = top;
    return (status);
}

/* old + delta (1 or -1) for ++ / --: numbers only, ints checked. */
static int  step_value(const t_node *n, t_value old, t_value *new,
                t_error *err)
{
    int64_t delta;

    delta = n->as.incdec.op == TOK_PLUS_PLUS ? 1 : -1;
    *new = old;
    if (old.type == VAL_INT)
    {
        if (__builtin_add_overflow(old.as.integer, delta, &new->as.integer))
            return (rt_error(err, ERR_OVERFLOW, n, "integer overflow"));
        return (0);
    }
    if (old.type == VAL_FLOAT)
    {
        new->as.number = old.as.number + (double)delta;
        return (0);
    }
    return (rt_error(err, ERR_TYPE, n, "%s needs a number, not '%s'",
            token_type_desc(n->as.incdec.op), val_type_name(old)));
}

/*
 * ++ / -- on l[i] or x.f: the container (and index) are evaluated once
 * and stay on the stack until the new value is stored.
 */
static int  incdec_place(t_interp *in, const t_node *n, t_value *old,
                t_value *new, t_error *err)
{
    const t_node    *t;
    size_t          base;
    t_value         v;
    int             status;

    t = n->as.incdec.target;
    base = in->top;
    status = rt_eval(in, t->type == NODE_INDEX ? t->as.index.object
            : t->as.field.object, &v, err);
    if (status == 0)
        rt_push(in, v);
    if (status == 0 && t->type == NODE_INDEX)
    {
        status = rt_eval(in, t->as.index.index, &v, err);
        if (status == 0)
            rt_push(in, v);
    }
    if (status == 0)
        status = t->type == NODE_INDEX ? rt_index_value(in, t, base, old, err)
            : rt_get_attr(in, t, base, old, err);
    if (status == 0)
        status = step_value(n, *old, new, err);
    if (status == 0)
        status = t->type == NODE_INDEX ? rt_index_store(in, t, base, *new, err)
            : rt_set_attr(in, t, base, *new, err);
    in->top = base;
    return (status);
}

/* ++x / x++ (and --) on a number. Prefix gives the new value. */
static int  eval_incdec(t_interp *in, const t_node *n, t_value *out,
                t_error *err)
{
    const t_node    *target;
    t_value         *slot;
    t_value         old;
    t_value         new;

    target = n->as.incdec.target;
    if (target->type == NODE_IDENT)
    {
        slot = rt_variable(in, &target->as.ident.ref);
        old = *slot;
        if (step_value(n, old, &new, err) == -1)
            return (-1);
        *slot = new;
    }
    else if (incdec_place(in, n, &old, &new, err) == -1)
        return (-1);
    *out = n->as.incdec.prefix ? new : old;
    return (0);
}

/*
 * x.name not called: built-in values have methods but no fields, so it
 * is a method bound to x (x stays on the stack while it is made).
 */
static int  eval_field(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    const t_method  *m;
    t_bound         *b;
    t_value         v;
    int             status;

    if (rt_eval(in, n->as.field.object, &v, err) == -1)
        return (-1);
    if (val_is_obj(v, OBJ_INSTANCE) || val_is_obj(v, OBJ_CLASS))
    {
        rt_push(in, v);
        status = rt_get_attr(in, n, in->top - 1, out, err);
        rt_pop(in);
        return (status);
    }
    m = rt_find_method(v, n->as.field.name);
    if (m)
    {
        rt_push(in, v);
        b = (t_bound *)heap_alloc(&in->heap, OBJ_BOUND, sizeof(t_bound));
        b->self = rt_pop(in);
        b->method = m;
        *out = val_obj(&b->obj);
        return (0);
    }
    return (rt_error(err, ERR_ATTRIBUTE, n, "'%s' object has no attribute"
            " '%.*s'", val_type_name(v), (int)n->as.field.name.len,
            n->as.field.name.ptr));
}

/*
 * What sum(... for ...), min(...) and max(...) give: the method of that
 * name on the list the comprehension made (stack[slot]).
 */
static int  comp_method(t_interp *in, t_node *n, size_t slot, t_value *out,
                t_error *err)
{
    t_name  name;
    size_t  top;
    int     status;

    name.ptr = n->as.comp.kind == COMP_SUM ? "sum"
        : n->as.comp.kind == COMP_MIN ? "min" : "max";
    name.len = 3;
    top = in->top;
    rt_push(in, in->stack[slot]);
    status = rt_call_method(in, n, name, top, 0, out, err);
    in->top = top;
    return (status);
}

/*
 * A comprehension: the result grows in its frame slot (rooted there)
 * while its loop runs; a tuple is made from the list at the end. For
 * any / all the slot holds the answer so far, and the loop stops as
 * soon as it is known (comp_add ends it as a return would).
 */
static int  eval_comprehension(t_interp *in, t_node *n, t_value *out,
                t_error *err)
{
    t_tuple *t;
    size_t  slot;
    size_t  i;
    int     kind;
    int     status;

    kind = n->as.comp.kind;
    slot = in->frame + (size_t)n->as.comp.slot;
    if (kind == COMP_SET || kind == COMP_DICT)
        in->stack[slot] = val_obj(&rt_table_new(&in->heap,
                    kind == COMP_SET ? OBJ_SET : OBJ_DICT)->obj);
    else if (kind == COMP_ANY || kind == COMP_ALL)
        in->stack[slot] = val_bool(kind == COMP_ALL);
    else
        in->stack[slot] = val_obj(&heap_list(&in->heap, 0)->obj);
    if (rt_exec(in, n->as.comp.loop, err) == EXEC_ERROR)
        return (-1);
    *out = in->stack[slot];
    status = 0;
    if (kind == COMP_SUM || kind == COMP_MIN || kind == COMP_MAX)
        status = comp_method(in, n, slot, out, err);
    if (kind == COMP_TUPLE)
    {
        t = heap_tuple(&in->heap, val_list(in->stack[slot])->len);
        for (i = 0; i < t->len; i++)
            t->items[i] = val_list(in->stack[slot])->items[i];
        *out = val_obj(&t->obj);
    }
    in->stack[slot] = val_none();
    return (status);
}

/* [a, b, c]: the items are pushed (rooted) and then copied into the list. */
static int  eval_list(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_node  *item;
    t_list  *l;
    t_value v;
    size_t  base;
    size_t  i;

    base = in->top;
    for (item = n->as.list.items; item; item = item->next)
    {
        if (rt_eval(in, item, &v, err) == -1)
        {
            in->top = base;
            return (-1);
        }
        rt_push(in, v);
    }
    l = heap_list(&in->heap, in->top - base);
    for (i = base; i < in->top; i++)
        l->items[l->len++] = in->stack[i];
    in->top = base;
    *out = val_obj(&l->obj);
    return (0);
}

/*
 * f"...": each field is evaluated and formatted into a string of its
 * own, kept on the stack (a field may itself use the text buffers, or
 * be another f-string); then the literal parts and those strings are
 * joined, in order.
 */
static int  eval_fstring(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    const t_node    *part;
    t_string        *s;
    t_value         v;
    size_t          base;
    size_t          k;

    base = in->top;
    for (part = n->as.list.items; part; part = part->next)
    {
        if (part->type != NODE_FORMAT)
            continue ;
        if (rt_eval(in, part->as.format.expr, &v, err) == -1)
        {
            in->top = base;
            return (-1);
        }
        rt_push(in, v);
        if (rt_format_spec(in, &in->piece, v, &part->as.format.spec, part,
                err) == -1)
        {
            in->top = base;
            return (-1);
        }
        s = heap_string(&in->heap, in->piece.data ? in->piece.data : "",
                in->piece.len);
        buf_clear(&in->piece);
        in->stack[in->top - 1] = val_obj(&s->obj);
    }
    buf_clear(&in->piece);
    k = base;
    for (part = n->as.list.items; part; part = part->next)
    {
        if (part->type == NODE_STRING)
            buf_add(&in->piece, part->as.string.chars, part->as.string.len);
        else
        {
            s = val_string(in->stack[k++]);
            buf_add(&in->piece, s->chars, s->len);
        }
    }
    s = heap_string(&in->heap, in->piece.data ? in->piece.data : "",
            in->piece.len);
    buf_clear(&in->piece);
    in->top = base;
    *out = val_obj(&s->obj);
    return (0);
}

/*
 * c[i] or c[start:end:step]: the container and then each part (none
 * where a slice leaves one out) go on the stack, so all of them stay
 * rooted while the others are computed and the result is made.
 */
static int  eval_index(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_node  *parts[3];
    t_value v;
    size_t  base;
    int     nparts;
    int     i;
    int     status;

    parts[0] = n->as.index.index;
    parts[1] = n->as.index.end;
    parts[2] = n->as.index.step;
    nparts = n->as.index.slice ? 3 : 1;
    base = in->top;
    status = rt_eval(in, n->as.index.object, &v, err);
    if (status == 0)
        rt_push(in, v);
    for (i = 0; i < nparts && status == 0; i++)
    {
        v = val_none();
        if (parts[i])
            status = rt_eval(in, parts[i], &v, err);
        rt_push(in, v);
    }
    if (status == 0)
        status = rt_index_value(in, n, base, out, err);
    in->top = base;
    return (status);
}

static int  eval_unary(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_value v;

    if (rt_eval(in, n->as.unary.operand, &v, err) == -1)
        return (-1);
    return (rt_unary(in, n, v, out, err));
}

int rt_eval(t_interp *in, t_node *n, t_value *out, t_error *err)
{
    t_string    *s;

    if (stack_exhausted())
        return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
    if (n->type == NODE_INT)
        *out = val_int(n->as.integer);
    else if (n->type == NODE_FLOAT)
        *out = val_float(n->as.number);
    else if (n->type == NODE_BOOL)
        *out = val_bool(n->as.boolean);
    else if (n->type == NODE_NONE)
        *out = val_none();
    else if (n->type == NODE_FSTRING)
        return (eval_fstring(in, n, out, err));
    else if (n->type == NODE_STRING)
    {
        s = heap_string(&in->heap, n->as.string.chars, n->as.string.len);
        *out = val_obj(&s->obj);
    }
    else if (n->type == NODE_IDENT)
        return (eval_name(in, n, out, err));
    else if (n->type == NODE_TERNARY)
        return (eval_ternary(in, n, out, err));
    else if (n->type == NODE_UNARY)
        return (eval_unary(in, n, out, err));
    else if (n->type == NODE_BINARY)
        return (eval_binary(in, n, out, err));
    else if (n->type == NODE_INCDEC)
        return (eval_incdec(in, n, out, err));
    else if (n->type == NODE_CALL)
        return (rt_eval_call(in, n, out, err));
    else if (n->type == NODE_FUNC)
        return (rt_make_function(in, n, out));
    else if (n->type == NODE_FIELD)
        return (eval_field(in, n, out, err));
    else if (n->type == NODE_INDEX)
        return (eval_index(in, n, out, err));
    else if (n->type == NODE_LIST)
        return (eval_list(in, n, out, err));
    else if (n->type == NODE_COMPREHENSION)
        return (eval_comprehension(in, n, out, err));
    else
        return (not_yet(err, n, "this expression"));
    return (0);
}
