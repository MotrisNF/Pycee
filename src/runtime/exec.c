/*
 * Statements. rt_exec runs one and says how it ended (t_exec): a break
 * or continue travels up to its loop, a return up to the frame. Like
 * rt_eval, each function leaves the value stack as it found it.
 */

#include <stddef.h>
#include <string.h>
#include "core/stack.h"
#include "core/utf8.h"
#include "runtime/interp_internal.h"

int rt_not_yet(t_error *err, const t_node *n, const char *what)
{
    return (rt_error(err, ERR_NOT_IMPLEMENTED, n, "%s is not implemented yet",
            what));
}

/* var l[n]: a list of n values, all none. */
static int  sized_list(t_interp *in, const t_node *n, t_value *out,
                t_error *err)
{
    t_list  *l;
    t_value size;
    size_t  i;

    if (rt_eval(in, n->as.var.size, &size, err) == -1)
        return (-1);
    if (size.type != VAL_INT || size.as.integer < 0)
        return (rt_error(err, size.type == VAL_INT ? ERR_VALUE : ERR_TYPE,
                n->as.var.size, "the size of a list must be an integer from"
                " 0 up, not %s", size.type == VAL_INT ? "negative"
                : val_type_name(size)));
    l = heap_list(&in->heap, (size_t)size.as.integer);
    for (i = 0; i < (size_t)size.as.integer; i++)
        l->items[i] = val_none();
    l->len = (size_t)size.as.integer;
    *out = val_obj(&l->obj);
    return (0);
}

/*
 * var, vemp or const: its value, or none. A global one (the top level,
 * or a statement typed at the top of the REPL) goes to the module's
 * table, a local one to the frame, in a new cell if an inner function
 * captures it (so a loop makes a new variable each time round). var
 * l[n] comes with lists.
 */
int rt_run_var(t_interp *in, const t_node *n, t_error *err)
{
    t_value v;

    v = val_none();
    if (n->as.var.size && sized_list(in, n, &v, err) == -1)
        return (-1);
    if (n->as.var.init && rt_eval(in, n->as.var.init, &v, err) == -1)
        return (-1);
    if (n->as.var.is_global)
        in->modules[in->module].values[n->as.var.slot] = v;
    else
    {
        in->stack[in->frame + n->as.var.slot] = v;
        if (n->as.var.captured)
            rt_box(in, in->frame + n->as.var.slot);
    }
    return (0);
}

/*
 * func name(...) { } inside a function: a closure in its slot. If it is
 * captured (it calls itself, or an inner function uses it) the cell is
 * made first, so the closure can capture it, and then filled.
 */
static int  exec_nested_func(t_interp *in, const t_node *n)
{
    t_cell  *cell;
    t_value v;
    size_t  slot;

    slot = in->frame + (size_t)n->as.func.slot;
    cell = NULL;
    if (n->as.func.captured)
    {
        in->stack[slot] = val_none();
        cell = rt_box(in, slot);
    }
    if (rt_make_function(in, n, &v) == -1)
        return (-1);
    if (cell)
        cell->value = v;
    else
        in->stack[slot] = v;
    return (0);
}

static t_token_type assign_to_binary(t_token_type op)
{
    if (op == TOK_PLUS_EQ)
        return (TOK_PLUS);
    if (op == TOK_MINUS_EQ)
        return (TOK_MINUS);
    if (op == TOK_STAR_EQ)
        return (TOK_STAR);
    if (op == TOK_SLASH_EQ)
        return (TOK_SLASH);
    if (op == TOK_SLASH_SLASH_EQ)
        return (TOK_SLASH_SLASH);
    if (op == TOK_PERCENT_EQ)
        return (TOK_PERCENT);
    if (op == TOK_AMP_EQ)
        return (TOK_AMP);
    if (op == TOK_PIPE_EQ)
        return (TOK_PIPE);
    if (op == TOK_CARET_EQ)
        return (TOK_CARET);
    if (op == TOK_SHL_EQ)
        return (TOK_SHL);
    return (TOK_SHR);
}

static int  compound_value(t_interp *in, const t_node *n, t_value *v,
                t_error *err);

/* What can be assigned to (the resolver let nothing else through). */
static int  check_target(const t_node *target, t_error *err)
{
    if (target->type == NODE_IDENT || target->type == NODE_INDEX
        || target->type == NODE_FIELD)
        return (0);
    return (rt_not_yet(err, target, "assigning to this"));
}

/*
 * x.f = v, x.f += v: x is evaluated once and stays on the stack (with
 * the current value, for op=) until it is stored.
 */
static int  assign_field(t_interp *in, const t_node *n, t_error *err)
{
    const t_node    *target;
    size_t          base;
    t_value         v;
    int             status;

    target = n->as.assign.target;
    base = in->top;
    status = rt_eval(in, target->as.field.object, &v, err);
    if (status == 0)
        rt_push(in, v);
    if (status == 0 && n->as.assign.op != TOK_ASSIGN)
    {
        status = rt_get_attr(in, target, base, &v, err);
        if (status == 0)
            rt_push(in, v);
    }
    if (status == 0)
        status = rt_eval(in, n->as.assign.value, &v, err);
    if (status == 0 && n->as.assign.op != TOK_ASSIGN)
    {
        rt_push(in, v);
        status = compound_value(in, n, &v, err);
    }
    if (status == 0)
        status = rt_set_attr(in, target, base, v, err);
    in->top = base;
    return (status);
}

/*
 * l += x on a list changes that list (the variable keeps pointing to
 * it); x is a list or a tuple. 1 if it was done, 0 if it is not that.
 */
static int  extend_in_place(t_interp *in, t_value cur, t_value x)
{
    t_value *items;
    t_list  *l;
    size_t  len;
    size_t  i;

    if (!val_is_list(cur) || (!val_is_list(x) && !val_is_tuple(x)))
        return (0);
    l = val_list(cur);
    rt_seq_items(x, &len);
    heap_list_reserve(&in->heap, l, l->len + len);
    items = rt_seq_items(x, &len);
    for (i = 0; i < len; i++)
        l->items[l->len + i] = items[i];
    l->len += len;
    return (1);
}

/*
 * The value of x op= v: the current value and v are the two top values
 * of the stack (rooted while the operator runs).
 */
static int  compound_value(t_interp *in, const t_node *n, t_value *v,
                t_error *err)
{
    if (n->as.assign.op == TOK_PLUS_EQ
        && extend_in_place(in, rt_peek(in, 1), rt_peek(in, 0)))
    {
        *v = rt_peek(in, 1);
        return (0);
    }
    return (rt_binary(in, n, assign_to_binary(n->as.assign.op), v, err));
}

/*
 * l[i] = v, l[i] += v: the list and the index are evaluated once and
 * stay on the stack (with the current value, for op=) until it is stored.
 */
static int  assign_index(t_interp *in, const t_node *n, t_error *err)
{
    const t_node    *target;
    size_t          base;
    t_value         v;
    int             status;

    target = n->as.assign.target;
    base = in->top;
    status = rt_eval(in, target->as.index.object, &v, err);
    if (status == 0)
    {
        rt_push(in, v);
        status = rt_eval(in, target->as.index.index, &v, err);
    }
    if (status == 0)
        rt_push(in, v);
    if (status == 0 && n->as.assign.op != TOK_ASSIGN)
    {
        status = rt_index_value(in, target, base, &v, err);
        if (status == 0)
            rt_push(in, v);
    }
    if (status == 0)
        status = rt_eval(in, n->as.assign.value, &v, err);
    if (status == 0 && n->as.assign.op != TOK_ASSIGN)
    {
        rt_push(in, v);
        status = compound_value(in, n, &v, err);
    }
    if (status == 0)
        status = rt_index_store(in, target, base, v, err);
    in->top = base;
    return (status);
}

/*
 * x = v, x += v... on a variable, or on l[i]. For x += v the current
 * value and v go on the stack, so both stay rooted while the operator
 * runs. The slot is looked up only at the end: evaluating may move the
 * stack.
 */
static int  exec_assign(t_interp *in, const t_node *n, t_error *err)
{
    const t_node    *target;
    size_t          top;
    t_value         v;
    int             status;

    target = n->as.assign.target;
    if (check_target(target, err) == -1)
        return (-1);
    if (target->type == NODE_INDEX)
        return (assign_index(in, n, err));
    if (target->type == NODE_FIELD)
        return (assign_field(in, n, err));
    top = in->top;
    if (n->as.assign.op != TOK_ASSIGN)
        rt_push(in, *rt_variable(in, &target->as.ident.ref));
    status = rt_eval(in, n->as.assign.value, &v, err);
    if (status == 0 && n->as.assign.op != TOK_ASSIGN)
    {
        rt_push(in, v);
        status = compound_value(in, n, &v, err);
    }
    in->top = top;
    if (status == 0)
        *rt_variable(in, &target->as.ident.ref) = v;
    return (status);
}

/*
 * The values of a, b = x, y (or var a, b = ...), pushed in order: one
 * per expression, or the items of a single tuple, which must have one
 * per target (the parser already checked the case of several values).
 * Every value exists before any target changes, so a, b = b, a swaps.
 */
static int  push_values(t_interp *in, const t_node *n, int ntargets,
                t_error *err)
{
    t_node          *e;
    t_value         v;
    size_t          i;
    size_t          len;

    for (e = n->as.multi.values; e; e = e->next)
    {
        if (rt_eval(in, e, &v, err) == -1)
            return (-1);
        rt_push(in, v);
    }
    if (n->as.multi.values->next)
        return (0);
    v = rt_pop(in);
    if (!val_is_tuple(v) && !val_is_list(v))
        return (rt_error(err, ERR_TYPE, n->as.multi.values, "cannot unpack"
                " '%s' into %d targets: only a tuple or a list can be"
                " unpacked", val_type_name(v), ntargets));
    rt_seq_items(v, &len);
    if (len != (size_t)ntargets)
        return (rt_error(err, ERR_ASSIGNMENT, n->as.multi.values, "cannot"
                " assign %zu values to %d targets", len, ntargets));
    for (i = 0; i < len; i++)
        rt_push(in, rt_seq_items(v, &len)[i]);
    return (0);
}

/*
 * a, l[i], x.f = ...: l and i (or x) are evaluated now; the value is
 * stack[value].
 */
static int  store_index_target(t_interp *in, const t_node *t, size_t value,
                t_error *err)
{
    size_t  base;
    t_value v;
    int     status;

    base = in->top;
    status = rt_eval(in, t->type == NODE_FIELD ? t->as.field.object
            : t->as.index.object, &v, err);
    if (status == 0)
        rt_push(in, v);
    if (status == 0 && t->type == NODE_FIELD)
        status = rt_set_attr(in, t, base, in->stack[value], err);
    else if (status == 0)
    {
        status = rt_eval(in, t->as.index.index, &v, err);
        if (status == 0)
        {
            rt_push(in, v);
            status = rt_index_store(in, t, base, in->stack[value], err);
        }
    }
    in->top = base;
    return (status);
}

static int  count_list(const t_node *n)
{
    int count;

    count = 0;
    for (; n; n = n->next)
        count++;
    return (count);
}

static int  exec_multi_assign(t_interp *in, const t_node *n, t_error *err)
{
    const t_node    *t;
    size_t          base;
    size_t          i;

    for (t = n->as.multi.targets; t; t = t->next)
        if (check_target(t, err) == -1)
            return (-1);
    base = in->top;
    if (push_values(in, n, count_list(n->as.multi.targets), err) == -1)
    {
        in->top = base;
        return (-1);
    }
    i = base;
    for (t = n->as.multi.targets; t; t = t->next)
    {
        if ((t->type == NODE_INDEX || t->type == NODE_FIELD)
            && store_index_target(in, t, i, err) == -1)
        {
            in->top = base;
            return (-1);
        }
        if (t->type == NODE_IDENT)
            *rt_variable(in, &t->as.ident.ref) = in->stack[i];
        i++;
    }
    in->top = base;
    return (0);
}

/* var a, b = ...: each target is a VAR node (global or local). */
static int  exec_multi_var(t_interp *in, const t_node *n, t_error *err)
{
    const t_node    *t;
    size_t          base;
    size_t          i;

    base = in->top;
    if (push_values(in, n, count_list(n->as.multi.targets), err) == -1)
    {
        in->top = base;
        return (-1);
    }
    i = base;
    for (t = n->as.multi.targets; t; t = t->next)
    {
        if (t->as.var.is_global)
            in->modules[in->module].values[t->as.var.slot] = in->stack[i++];
        else
            in->stack[in->frame + t->as.var.slot] = in->stack[i++];
    }
    in->top = base;
    return (0);
}

/* The condition of if, elif, while...: a bool. 1, 0 or -1. */
static int  eval_cond(t_interp *in, t_node *cond, const char *what,
                t_error *err)
{
    t_value v;
    int     b;

    if (rt_eval(in, cond, &v, err) == -1
        || rt_truthy(cond, v, what, &b, err) == -1)
        return (-1);
    return (b);
}

static t_exec   exec_if(t_interp *in, t_node *n, const char *what,
                    t_error *err)
{
    int b;

    b = eval_cond(in, n->as.if_.cond, what, err);
    if (b == -1)
        return (EXEC_ERROR);
    if (b)
        return (rt_exec_block(in, n->as.if_.then, err));
    if (!n->as.if_.else_)
        return (EXEC_OK);
    if (n->as.if_.else_->type == NODE_IF)
        return (exec_if(in, n->as.if_.else_, "the condition of 'elif'", err));
    return (rt_exec_block(in, n->as.if_.else_, err));
}

/* What a loop does after its body ran (see loop_step). */
typedef enum e_loop
{
    LOOP_NEXT,
    LOOP_BROKEN,
    LOOP_LEAVE
}   t_loop;

/*
 * How the body ended decides: the next round (it ended, or continue),
 * stop (its own break) or leave the loop giving back status as it is
 * (an error, a return, or a break or continue meant for an outer loop,
 * which then has one level less to go).
 */
static t_loop   loop_step(t_interp *in, t_exec status)
{
    if (status == EXEC_OK)
        return (LOOP_NEXT);
    if (status != EXEC_BREAK && status != EXEC_CONTINUE)
        return (LOOP_LEAVE);
    if (--in->jump_levels > 0)
        return (LOOP_LEAVE);
    return (status == EXEC_BREAK ? LOOP_BROKEN : LOOP_NEXT);
}

static t_exec   exec_while(t_interp *in, t_node *n, t_error *err)
{
    t_exec  status;
    t_loop  step;
    int     b;

    while (1)
    {
        b = eval_cond(in, n->as.while_.cond, "the condition of 'while'", err);
        if (b == -1)
            return (EXEC_ERROR);
        if (!b)
            return (EXEC_OK);
        status = rt_exec_block(in, n->as.while_.body, err);
        step = loop_step(in, status);
        if (step != LOOP_NEXT)
            return (step == LOOP_BROKEN ? EXEC_OK : status);
    }
}

/* The body runs once before the condition is first checked. */
static t_exec   exec_do_while(t_interp *in, t_node *n, t_error *err)
{
    t_exec  status;
    t_loop  step;
    int     b;

    while (1)
    {
        status = rt_exec_block(in, n->as.while_.body, err);
        step = loop_step(in, status);
        if (step != LOOP_NEXT)
            return (step == LOOP_BROKEN ? EXEC_OK : status);
        b = eval_cond(in, n->as.while_.cond, "the condition of 'while'", err);
        if (b == -1)
            return (EXEC_ERROR);
        if (!b)
            return (EXEC_OK);
    }
}

/*
 * Puts the item of this round in the loop variable(s): with several
 * (for (k, v in ...)) it must be a tuple or a list of exactly as many.
 * A captured one gets a new cell each round.
 */
static int  bind_for(t_interp *in, const t_node *n, t_value item,
                t_error *err)
{
    const t_node    *v;
    const t_value   *items;
    size_t          len;
    size_t          want;
    size_t          i;

    if (!n->as.for_.more)
    {
        in->stack[in->frame + n->as.for_.slot] = item;
        if (n->as.for_.captured)
            rt_box(in, in->frame + n->as.for_.slot);
        return (0);
    }
    want = 1;
    for (v = n->as.for_.more; v; v = v->next)
        want++;
    if (!val_is_tuple(item) && !val_is_list(item))
        return (rt_error(err, ERR_TYPE, n->as.for_.iterable, "cannot unpack"
                " '%s' into %zu loop variables: each item must be a tuple or"
                " a list", val_type_name(item), want));
    items = rt_seq_items(item, &len);
    if (len != want)
        return (rt_error(err, ERR_ASSIGNMENT, n->as.for_.iterable, "cannot"
                " assign %zu values to %zu loop variables", len, want));
    in->stack[in->frame + n->as.for_.slot] = items[0];
    i = 1;
    for (v = n->as.for_.more; v; v = v->next)
        in->stack[in->frame + v->as.var.slot] = rt_seq_items(item,
                &len)[i++];
    if (n->as.for_.captured)
        rt_box(in, in->frame + n->as.for_.slot);
    for (v = n->as.for_.more; v; v = v->next)
        if (v->as.var.captured)
            rt_box(in, in->frame + v->as.var.slot);
    return (0);
}

/*
 * What a round of a for over a dict or a set gives (decided by the
 * author): a set gives each element; a dict, with one loop variable, a
 * new dict of that one pair (dict("a": 1)), with two the key and the
 * value.
 */
static int  bind_table_item(t_interp *in, const t_node *n, size_t table,
                size_t pos, t_error *err)
{
    t_dict  *one;
    t_entry e;
    size_t  slot;

    e = val_dict(in->stack[table])->entries[pos];
    if (val_is_set(in->stack[table]))
        return (bind_for(in, n, e.key, err));
    if (!n->as.for_.more)
    {
        slot = in->top;
        rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_DICT)->obj));
        one = val_dict(in->stack[slot]);
        if (rt_table_set(&in->heap, one, e.key, e.hash, e.value) == -1)
            return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
        in->top = slot;
        return (bind_for(in, n, val_obj(&one->obj), err));
    }
    if (n->as.for_.more->next)
        return (rt_error(err, ERR_ASSIGNMENT, n->as.for_.iterable, "a dict"
                " gives a key and a value: use one or two loop variables"));
    in->stack[in->frame + n->as.for_.slot] = e.key;
    in->stack[in->frame + n->as.for_.more->as.var.slot] = e.value;
    if (n->as.for_.captured)
        rt_box(in, in->frame + n->as.for_.slot);
    if (n->as.for_.more->as.var.captured)
        rt_box(in, in->frame + n->as.for_.more->as.var.slot);
    return (0);
}

/*
 * for over a dict or a set (at stack[table], rooted), in insertion
 * order. One that changes size while it is walked is a RuntimeError.
 */
static t_exec   exec_for_table(t_interp *in, t_node *n, size_t table,
                    int *broken, t_error *err)
{
    size_t  len;
    size_t  pos;
    t_exec  status;
    t_loop  step;

    *broken = 0;
    len = val_dict(in->stack[table])->len;
    for (pos = 0; pos < val_dict(in->stack[table])->nentries; pos++)
    {
        if (val_dict(in->stack[table])->len != len)
            return (rt_error(err, ERR_RUNTIME, n->as.for_.iterable, "the %s"
                    " changed size while the for walked it",
                    val_type_name(in->stack[table])));
        if (!val_dict(in->stack[table])->entries[pos].used)
            continue ;
        if (bind_table_item(in, n, table, pos, err) == -1)
            return (EXEC_ERROR);
        status = rt_exec_block(in, n->as.for_.body, err);
        step = loop_step(in, status);
        *broken = (step == LOOP_BROKEN);
        if (step != LOOP_NEXT)
            return (*broken ? EXEC_OK : status);
    }
    if (val_dict(in->stack[table])->len != len)
        return (rt_error(err, ERR_RUNTIME, n->as.for_.iterable, "the %s"
                " changed size while the for walked it",
                val_type_name(in->stack[table])));
    return (EXEC_OK);
}

/*
 * for over a string: a string of one character each time round, walked
 * with a byte offset (never by index, which would go back to the start).
 */
static t_exec   exec_for_string(t_interp *in, t_node *n, const t_string *s,
                    int *broken, t_error *err)
{
    t_string    *c;
    size_t      pos;
    int32_t     cp;
    int         len;
    t_exec      status;
    t_loop      step;

    pos = 0;
    while (pos < s->len)
    {
        len = utf8_decode(s->chars + pos, s->len - pos, &cp);
        c = heap_string(&in->heap, s->chars + pos, (size_t)len);
        c->nchars = 1;
        pos += (size_t)len;
        if (bind_for(in, n, val_obj(&c->obj), err) == -1)
            return (EXEC_ERROR);
        status = rt_exec_block(in, n->as.for_.body, err);
        step = loop_step(in, status);
        *broken = (step == LOOP_BROKEN);
        if (step != LOOP_NEXT)
            return (*broken ? EXEC_OK : status);
    }
    return (EXEC_OK);
}

/*
 * for over a range (its numbers are made one by one), a tuple or a list.
 * The caller keeps c rooted. A tuple cannot change; a list that changes
 * size while it is walked is a RuntimeError (the items would be missed
 * or seen twice), checked before every round, after the body ran.
 * *broken says the loop ended with its own break.
 */
static t_exec   exec_for_items(t_interp *in, t_node *n, t_value c,
                    int *broken, t_error *err)
{
    uint64_t    len;
    uint64_t    i;
    size_t      size;
    t_exec      status;
    t_loop      step;

    *broken = 0;
    if (val_is_string(c))
        return (exec_for_string(in, n, val_string(c), broken, err));
    if (val_is_range(c))
        len = rt_range_len(val_range(c));
    else
    {
        rt_seq_items(c, &size);
        len = size;
    }
    for (i = 0; i < len; i++)
    {
        if (val_is_list(c) && val_list(c)->len != len)
            return (rt_error(err, ERR_RUNTIME, n->as.for_.iterable, "the"
                    " list changed size while the for walked it (from %zu to"
                    " %zu items)", (size_t)len, val_list(c)->len));
        if (bind_for(in, n, val_is_range(c)
                ? val_int(rt_range_at(val_range(c), i))
                : rt_seq_items(c, &size)[i], err) == -1)
            return (EXEC_ERROR);
        status = rt_exec_block(in, n->as.for_.body, err);
        step = loop_step(in, status);
        *broken = (step == LOOP_BROKEN);
        if (step != LOOP_NEXT)
            return (*broken ? EXEC_OK : status);
    }
    if (val_is_list(c) && val_list(c)->len != len)
        return (rt_error(err, ERR_RUNTIME, n->as.for_.iterable, "the list"
                " changed size while the for walked it (from %zu to %zu"
                " items)", (size_t)len, val_list(c)->len));
    return (EXEC_OK);
}

/*
 * for over an object: its iter() gives an iterator (kept on the stack
 * at it, and pushed again as self for each call), then has_next() (a
 * bool) and next() each round.
 */
static t_exec   exec_for_iter(t_interp *in, t_node *n, size_t obj,
                    int *broken, t_error *err)
{
    t_value v;
    t_exec  status;
    t_loop  step;
    size_t  it;
    int     more;

    *broken = 0;
    it = in->top;
    rt_push(in, in->stack[obj]);
    if (rt_call_named(in, n->as.for_.iterable, it, "iter", 0, &v, err) == -1)
        return (EXEC_ERROR);
    in->top = it + 1;
    in->stack[it] = v;
    while (1)
    {
        rt_push(in, in->stack[it]);
        if (rt_call_named(in, n->as.for_.iterable, it + 1, "has_next", 0, &v,
                err) == -1 || rt_truthy(n->as.for_.iterable, v,
                "has_next()", &more, err) == -1)
            return (EXEC_ERROR);
        in->top = it + 1;
        if (!more)
            return (EXEC_OK);
        rt_push(in, in->stack[it]);
        if (rt_call_named(in, n->as.for_.iterable, it + 1, "next", 0, &v, err)
            == -1)
            return (EXEC_ERROR);
        in->top = it + 1;
        if (bind_for(in, n, v, err) == -1)
            return (EXEC_ERROR);
        status = rt_exec_block(in, n->as.for_.body, err);
        step = loop_step(in, status);
        *broken = (step == LOOP_BROKEN);
        if (step != LOOP_NEXT)
            return (*broken ? EXEC_OK : status);
    }
}

/*
 * for over a file: one line per round, each with its "\n" (read_line),
 * until the end. The file stays rooted at at.
 */
static t_exec   exec_for_file(t_interp *in, t_node *n, size_t at,
                    int *broken, t_error *err)
{
    t_value v;
    t_exec  status;
    t_loop  step;

    while (1)
    {
        if (rt_file_read_line(in, n->as.for_.iterable, in->stack[at], &v,
                err) == -1)
            return (EXEC_ERROR);
        if (val_string(v)->len == 0)
            return (EXEC_OK);
        if (bind_for(in, n, v, err) == -1)
            return (EXEC_ERROR);
        status = rt_exec_block(in, n->as.for_.body, err);
        step = loop_step(in, status);
        *broken = (step == LOOP_BROKEN);
        if (step != LOOP_NEXT)
            return (*broken ? EXEC_OK : status);
    }
}

/* The loop of n over stack[at] (rooted there), whatever it is. */
static t_exec   exec_for_value(t_interp *in, t_node *n, size_t at,
                    int *broken, t_error *err)
{
    t_value v;

    *broken = 0;
    v = in->stack[at];
    if (!val_is_range(v) && !val_is_tuple(v) && !val_is_string(v)
        && !val_is_list(v) && !val_is_dict(v) && !val_is_set(v)
        && !val_is_obj(v, OBJ_FILE) && !rt_special(v, "iter"))
        return (rt_error(err, ERR_TYPE, n->as.for_.iterable,
                "'%s' object is not iterable", val_type_name(v)));
    if (val_is_obj(v, OBJ_INSTANCE))
        return (exec_for_iter(in, n, at, broken, err));
    if (val_is_obj(v, OBJ_FILE))
        return (exec_for_file(in, n, at, broken, err));
    if (val_is_dict(v) || val_is_set(v))
        return (exec_for_table(in, n, at, broken, err));
    return (exec_for_items(in, n, v, broken, err));
}

/* The else block runs when the loop ends without a break. */
static t_exec   exec_for(t_interp *in, t_node *n, t_error *err)
{
    t_value v;
    size_t  top;
    t_exec  status;
    int     broken;

    if (rt_eval(in, n->as.for_.iterable, &v, err) == -1)
        return (EXEC_ERROR);
    top = in->top;
    rt_push(in, v);
    status = exec_for_value(in, n, top, &broken, err);
    in->top = top;
    if (status != EXEC_OK || broken || !n->as.for_.else_)
        return (status);
    return (rt_exec_block(in, n->as.for_.else_, err));
}

/*
 * What a for over v gives, in a new list (*out), for C code that takes
 * any iterable (sum(r), l.extend(x)): the same loop, so the same items
 * and errors, run with nodes made here (for (x in v) add x). x and the
 * list live in two slots of the stack; at is where errors point.
 */
int rt_collect(t_interp *in, const t_node *at, t_value v, t_value *out,
        t_error *err)
{
    t_node  loop;
    t_node  body;
    t_node  add;
    t_node  item;
    size_t  top;
    t_exec  status;
    int     broken;

    top = in->top;
    rt_push(in, v);
    rt_push(in, val_none());
    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    memset(&loop, 0, sizeof(loop));
    memset(&body, 0, sizeof(body));
    memset(&add, 0, sizeof(add));
    memset(&item, 0, sizeof(item));
    item.type = NODE_IDENT;
    item.as.ident.ref.kind = REF_LOCAL;
    item.as.ident.ref.slot = (int)(top + 1 - in->frame);
    add.type = NODE_COMP_ADD;
    add.as.comp.kind = COMP_LIST;
    add.as.comp.slot = (int)(top + 2 - in->frame);
    add.as.comp.loop = &item;
    body.type = NODE_BLOCK;
    body.as.list.items = &add;
    loop.type = NODE_FOR;
    loop.offset = at->offset;
    loop.len = at->len;
    loop.as.for_.slot = item.as.ident.ref.slot;
    loop.as.for_.iterable = (t_node *)at;
    loop.as.for_.body = &body;
    status = exec_for_value(in, &loop, top, &broken, err);
    *out = in->stack[top + 2];
    in->top = top;
    return (status == EXEC_ERROR ? -1 : 0);
}

/*
 * Whether a case value (an INT, FLOAT or STRING literal) is v, as with
 * is. Compared in place: building the literal's value would allocate.
 * A literal is never a tuple, so rt_equal cannot fail here.
 */
static int  case_matches(t_value v, const t_node *lit)
{
    t_string    *s;

    if (lit->type == NODE_INT)
        return (rt_equal(v, val_int(lit->as.integer)));
    if (lit->type == NODE_FLOAT)
        return (rt_equal(v, val_float(lit->as.number)));
    if (!val_is_string(v))
        return (0);
    s = val_string(v);
    return (s->len == lit->as.string.len
        && memcmp(s->chars, lit->as.string.chars, s->len) == 0);
}

/* The first case with a matching value runs, or default. No fallthrough. */
static t_exec   exec_switch(t_interp *in, t_node *n, t_error *err)
{
    t_node          *cs;
    const t_node    *lit;
    t_value         v;

    if (rt_eval(in, n->as.switch_.subject, &v, err) == -1)
        return (EXEC_ERROR);
    for (cs = n->as.switch_.cases; cs; cs = cs->next)
        for (lit = cs->as.case_.values; lit; lit = lit->next)
            if (case_matches(v, lit))
                return (rt_exec_block(in, cs->as.case_.body, err));
    if (n->as.switch_.default_)
        return (rt_exec_block(in, n->as.switch_.default_, err));
    return (EXEC_OK);
}

/* The value goes up in in->ret (none for "return;"). */
static t_exec   exec_return(t_interp *in, const t_node *n, t_error *err)
{
    t_value v;

    v = val_none();
    if (n->as.ret.value && rt_eval(in, n->as.ret.value, &v, err) == -1)
        return (EXEC_ERROR);
    in->ret = v;
    in->ret_node = n;
    return (EXEC_RETURN);
}

/* The error value that holds e (a context always lives in one). */
static t_value  holder_of(const t_error *e)
{
    return (val_obj((t_obj *)((char *)e - offsetof(t_error_value, err))));
}

/*
 * A new error value with a copy of err (the error being caught). The
 * value it was raised while handling stays rooted while it is made.
 */
static t_error_value    *catch_error(t_interp *in, const t_error *err)
{
    t_error_value   *e;
    size_t          top;

    top = in->top;
    if (err->context)
        rt_push(in, holder_of(err->context));
    e = (t_error_value *)heap_alloc(&in->heap, OBJ_ERROR,
            sizeof(t_error_value));
    e->err = *err;
    e->err.top_name = in->func_name.ptr;
    e->err.top_len = in->func_name.len;
    e->raised = 1;
    in->top = top;
    return (e);
}

/*
 * raise e: an error made but never raised starts here; one raised
 * before (raise e in an except) keeps where it came from and its
 * traceback, which grows as it goes up again.
 */
static t_exec   exec_raise(t_interp *in, const t_node *n, t_error *err)
{
    t_error_value   *e;
    t_instance      *o;
    t_value         v;

    if (rt_eval(in, n->as.raise.value, &v, err) == -1)
        return (EXEC_ERROR);
    o = val_is_obj(v, OBJ_INSTANCE) ? (t_instance *)v.as.obj : NULL;
    if (!val_is_obj(v, OBJ_ERROR) && !(o && o->error))
        return (rt_error(err, ERR_TYPE, n->as.raise.value, "raise needs an"
                " error, not '%s': raise ValueError(\"...\")",
                val_type_name(v)));
    e = o ? o->error : (t_error_value *)v.as.obj;
    if (!e->raised)
    {
        e->err.offset = n->offset;
        e->err.len = n->len;
        e->raised = 1;
    }
    *err = e->err;
    err->self = e;
    if (o)
    {
        err->instance = o;
        err->type_name = o->cls->as.obj.cname;
    }
    return (EXEC_ERROR);
}

/* assert(c) / assert(c, message): AssertionError when c is false. */
static t_exec   exec_assert(t_interp *in, const t_node *n, t_error *err)
{
    t_buffer    *b;
    t_value     v;
    int         ok;

    ok = eval_cond(in, n->as.raise.value, "the condition of 'assert'", err);
    if (ok != 0)
        return (ok == 1 ? EXEC_OK : EXEC_ERROR);
    if (!n->as.raise.message)
        return (rt_error(err, ERR_ASSERTION, n, "assertion failed"));
    if (rt_eval(in, n->as.raise.message, &v, err) == -1)
        return (EXEC_ERROR);
    rt_push(in, v);
    b = rt_buf_take(in);
    if (rt_show(in, n, b, v, 0, err) == -1)
        ok = -1;
    else
        ok = rt_error(err, ERR_ASSERTION, n, "%.*s", (int)b->len,
                b->data ? b->data : "");
    rt_buf_give(in);
    rt_pop(in);
    return (EXEC_ERROR);
}

/* Whether cls is base or extends it (through any number of parents). */
static int  obj_extends(const t_node *cls, const t_node *base)
{
    for (; cls; cls = cls->as.obj.base)
        if (cls == base)
            return (1);
    return (0);
}

/*
 * The first except that catches err: one without types catches any
 * error, otherwise one of its types must be err's or an ancestor. A
 * SystemExit (exit) is not an Error: nothing catches it. Nor is a
 * KeyboardInterrupt, which only an except naming it catches (an except
 * without types must never swallow Ctrl+C).
 */
static t_node   *find_handler(const t_node *n, const t_error *err)
{
    t_node          *h;
    const t_node    *t;
    t_name          name;

    if (!error_is_a(err->type, ERR_ERROR)
        && err->type != ERR_KEYBOARD_INTERRUPT)
        return (NULL);
    for (h = n->as.try_.handlers; h; h = h->next)
    {
        if (!h->as.except.types && error_is_a(err->type, ERR_ERROR))
            return (h);
        for (t = h->as.except.types; t; t = t->next)
        {
            name = t->as.ident.name;
            if (t->as.ident.ref.kind == REF_OBJ && err->instance
                && obj_extends(((const t_instance *)err->instance)->cls,
                    t->as.ident.ref.decl))
                return (h);
            if (t->as.ident.ref.kind != REF_OBJ && error_is_a(err->type,
                    (t_error_type)error_type_named(name.ptr, name.len)))
                return (h);
        }
    }
    return (NULL);
}

/*
 * Runs the except that catches err, with the error as a value in its
 * variable (rooted there, or on the stack when it has no name). An
 * error raised inside it was raised while handling this one: its
 * context, unless it is this very error raised again.
 */
static t_exec   handle_error(t_interp *in, const t_node *n, t_error *err)
{
    t_node          *h;
    t_error_value   *e;
    t_value         caught;
    size_t          top;
    size_t          slot;
    t_exec          status;

    h = find_handler(n, err);
    if (!h)
        return (EXEC_ERROR);
    top = in->top;
    if (err->instance)
    {
        caught = val_obj((t_obj *)err->instance);
        e = ((t_instance *)err->instance)->error;
        e->err = *err;
        e->err.top_name = in->func_name.ptr;
        e->err.top_len = in->func_name.len;
    }
    else
    {
        e = catch_error(in, err);
        caught = val_obj(&e->obj);
    }
    rt_push(in, caught);
    if (h->as.except.name.ptr)
    {
        slot = in->frame + (size_t)h->as.except.slot;
        in->stack[slot] = caught;
        if (h->as.except.captured)
            rt_box(in, slot);
    }
    status = rt_exec_block(in, h->as.except.body, err);
    if (status == EXEC_ERROR && err->type != ERR_EXIT && !err->context
        && err->self != e)
        err->context = &e->err;
    in->top = top;
    return (status);
}

/*
 * finally runs however the try ended, and then it ends the same way:
 * the error in flight (a copy, its context rooted on the stack), the
 * value of a return (rooted too) or a break are put back. If the block
 * raises an error itself, that one goes on, with the first one as its
 * context. It cannot end with return, break or continue (the resolver).
 */
static t_exec   run_finally(t_interp *in, t_node *block, t_exec status,
                    t_error *err)
{
    t_error         saved;
    t_error_value   *e;
    t_value         ret;
    size_t          top;
    int             levels;

    if (status == EXEC_ERROR)
        saved = *err;
    ret = in->ret;
    levels = in->jump_levels;
    top = in->top;
    rt_push(in, ret);
    if (status == EXEC_ERROR && saved.context)
        rt_push(in, holder_of(saved.context));
    if (status == EXEC_ERROR && saved.instance)
        rt_push(in, val_obj((t_obj *)saved.instance));
    if (rt_exec_block(in, block, err) == EXEC_ERROR)
    {
        if (status == EXEC_ERROR && saved.type != ERR_EXIT && !err->context)
        {
            e = catch_error(in, &saved);
            err->context = &e->err;
        }
        in->top = top;
        return (EXEC_ERROR);
    }
    in->top = top;
    in->ret = ret;
    in->jump_levels = levels;
    if (status == EXEC_ERROR)
        *err = saved;
    return (status);
}

static t_exec   exec_try(t_interp *in, t_node *n, t_error *err)
{
    t_exec  status;

    status = rt_exec_block(in, n->as.try_.body, err);
    if (status == EXEC_ERROR)
        status = handle_error(in, n, err);
    if (n->as.try_.finally_)
        status = run_finally(in, n->as.try_.finally_, status, err);
    return (status);
}

/*
 * One value of a comprehension goes into its result (in its frame slot):
 * appended to a list, added to a set, or key: value into a dict. For any
 * / all it must be a bool, and the one that decides (true for any, false
 * for all) is the answer: 1, to end the loop.
 */
static int  comp_add(t_interp *in, const t_node *n, t_error *err)
{
    t_node          *value;
    t_list          *l;
    t_value         v;
    size_t          slot;
    size_t          top;
    int             status;

    slot = in->frame + (size_t)n->as.comp.slot;
    value = n->as.comp.loop;
    top = in->top;
    if (value->type == NODE_PAIR)
    {
        status = rt_eval(in, value->as.pair.key, &v, err);
        if (status == 0)
        {
            rt_push(in, v);
            status = rt_eval(in, value->as.pair.value, &v, err);
        }
        if (status == 0)
            status = rt_table_put(in, value, slot, in->stack[top], v, err);
        in->top = top;
        return (status);
    }
    if (rt_eval(in, value, &v, err) == -1)
        return (-1);
    if (n->as.comp.kind == COMP_ANY || n->as.comp.kind == COMP_ALL)
    {
        if (rt_truthy(value, v, n->as.comp.kind == COMP_ANY ? "each value"
                " of any()" : "each value of all()", &status, err) == -1)
            return (-1);
        if (status == (n->as.comp.kind == COMP_ALL))
            return (0);
        in->stack[slot] = v;
        return (1);
    }
    if (n->as.comp.kind == COMP_SET)
        return (rt_table_put(in, value, slot, v, val_none(), err));
    l = val_list(in->stack[slot]);
    heap_list_reserve(&in->heap, l, l->len + 1);
    l->items[l->len++] = v;
    return (0);
}

t_exec  rt_exec_block(t_interp *in, t_node *block, t_error *err)
{
    t_node  *n;
    t_exec  status;

    if (RT_POLL(in, block, err) == -1)
        return (EXEC_ERROR);
    for (n = block->as.list.items; n; n = n->next)
    {
        status = rt_exec(in, n, err);
        if (status != EXEC_OK)
            return (status);
    }
    return (EXEC_OK);
}

t_exec  rt_exec(t_interp *in, t_node *n, t_error *err)
{
    t_value v;
    int     status;

    if (stack_exhausted())
        return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
    if (RT_POLL(in, n, err) == -1)
        return (EXEC_ERROR);
    if (n->type == NODE_VAR)
        return (rt_run_var(in, n, err));
    if (n->type == NODE_FUNC)
        return (exec_nested_func(in, n));
    if (n->type == NODE_EXPR_STMT)
        return (rt_eval(in, n->as.expr_stmt.expr, &v, err));
    if (n->type == NODE_ASSIGN)
        return (exec_assign(in, n, err));
    if (n->type == NODE_MULTI_ASSIGN)
        return (exec_multi_assign(in, n, err));
    if (n->type == NODE_MULTI_VAR)
        return (exec_multi_var(in, n, err));
    if (n->type == NODE_IF)
        return (exec_if(in, n, "the condition of 'if'", err));
    if (n->type == NODE_WHILE)
        return (exec_while(in, n, err));
    if (n->type == NODE_DO_WHILE)
        return (exec_do_while(in, n, err));
    if (n->type == NODE_FOR)
        return (exec_for(in, n, err));
    if (n->type == NODE_SWITCH)
        return (exec_switch(in, n, err));
    if (n->type == NODE_RETURN)
        return (exec_return(in, n, err));
    if (n->type == NODE_TRY)
        return (exec_try(in, n, err));
    if (n->type == NODE_COMP_ADD)
    {
        status = comp_add(in, n, err);
        return (status == -1 ? EXEC_ERROR : status ? EXEC_RETURN : EXEC_OK);
    }
    if (n->type == NODE_RAISE)
        return (exec_raise(in, n, err));
    if (n->type == NODE_ASSERT)
        return (exec_assert(in, n, err));
    if (n->type == NODE_BREAK || n->type == NODE_CONTINUE)
    {
        in->jump_levels = n->as.jump.levels;
        return (n->type == NODE_BREAK ? EXEC_BREAK : EXEC_CONTINUE);
    }
    return (rt_not_yet(err, n, "this statement"));
}
