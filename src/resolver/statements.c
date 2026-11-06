/* Pass 2: statements and function bodies. */

#include <string.h>
#include "core/errors.h"
#include "core/stack.h"
#include "resolver/resolver_internal.h"

static int  resolve_stmt(t_ctx *c, t_node *n);
static int  resolve_nested_func(t_ctx *c, t_node *n);

/* Every vemp the block declares dies when it closes. */
static int  resolve_block(t_ctx *c, t_node *block)
{
    t_node  *n;
    size_t  mark;

    mark = c->r->temps.count;
    for (n = block->as.list.items; n; n = n->next)
        if (resolve_stmt(c, n) == -1)
            return (-1);
    c->r->temps.count = mark;
    return (0);
}

/*
 * Declares a param, var, vemp or for variable of the current function
 * with the next frame slot. Slots are never reused, so a var declared
 * inside a block cannot share a slot with a vemp that outlived it.
 */
static int  declare_local(t_ctx *c, t_symtab *t, t_name name,
                t_sym_kind kind, const t_node *decl, int *slot)
{
    t_symbol    sym;

    sym = rs_make_symbol(c, name, kind, decl, 0);
    sym.slot = c->nslots;
    sym.constant = (decl->type == NODE_VAR && decl->as.var.is_const)
        || decl->type == NODE_FUNC;
    if (rs_declare(c, t, &sym) == -1)
        return (-1);
    *slot = c->nslots++;
    return (0);
}

/* A var's size or initial value. */
static int  resolve_var_exprs(t_ctx *c, t_node *n)
{
    if (n->as.var.size && rs_resolve_expr(c, n->as.var.size) == -1)
        return (-1);
    if (n->as.var.init && rs_resolve_expr(c, n->as.var.init) == -1)
        return (-1);
    return (0);
}

/*
 * var goes to locals (it lives until the end of the function, even when
 * declared inside a block) and is forbidden inside loops; vemp and a
 * const inside a function go to temps (they live until their block
 * ends, so a loop makes a new one each time round). The size or initial
 * value is resolved first, so "var x = x;" does not see the x being
 * declared. Outside any function (a statement typed at the top of the
 * REPL) var and const are globals and vemp does not exist.
 */
static int  declare_var(t_ctx *c, t_node *n)
{
    int temp;

    temp = n->as.var.is_vemp || (n->as.var.is_const && c->func);
    if (n->as.var.is_vemp && !c->func)
        return (error_set(c->err, ERR_SYNTAX, n->offset, 4, "'vemp' is only"
                " allowed inside functions; use 'var' for globals"));
    if (!temp && c->loop_depth > 0 && c->func)
        return (error_set(c->err, ERR_SYNTAX, n->offset, 3, "'var' is not"
                " allowed inside a loop: declare it before the loop or use"
                " 'vemp'"));
    if (!temp && c->loop_depth > 0)
        return (error_set(c->err, ERR_SYNTAX, n->offset,
                n->as.var.is_const ? 5 : 3, "a global '%s' is not allowed"
                " inside a loop: declare it before the loop",
                n->as.var.is_const ? "const" : "var"));
    if (!c->func)
        return (rs_declare_global_var(c, n));
    return (declare_local(c, temp ? &c->r->temps : &c->r->locals,
            n->as.var.name, temp ? SYM_VEMP : SYM_VAR, n, &n->as.var.slot));
}

static int  resolve_var(t_ctx *c, t_node *n)
{
    if (resolve_var_exprs(c, n) == -1)
        return (-1);
    return (declare_var(c, n));
}

/* var a, b = x, y: the values first, so they cannot see a or b. */
static int  resolve_multi_var(t_ctx *c, t_node *n)
{
    t_node  *e;

    for (e = n->as.multi.values; e; e = e->next)
        if (rs_resolve_expr(c, e) == -1)
            return (-1);
    for (e = n->as.multi.targets; e; e = e->next)
        if (declare_var(c, e) == -1)
            return (-1);
    return (0);
}

static int  resolve_loop_body(t_ctx *c, t_node *body)
{
    int status;

    c->loop_depth++;
    status = resolve_block(c, body);
    c->loop_depth--;
    return (status);
}

/*
 * The loop variable is an implicit vemp of the body: it is not visible
 * in the else block, which runs after the loop (outside it, so a break
 * there belongs to an enclosing loop).
 */
static int  resolve_for(t_ctx *c, t_node *n)
{
    t_node  *v;
    size_t  mark;

    if (rs_resolve_expr(c, n->as.for_.iterable) == -1)
        return (-1);
    mark = c->r->temps.count;
    if (declare_local(c, &c->r->temps, n->as.for_.name, SYM_VEMP, n,
            &n->as.for_.slot) == -1)
        return (-1);
    for (v = n->as.for_.more; v; v = v->next)
        if (declare_local(c, &c->r->temps, v->as.var.name, SYM_VEMP, v,
                &v->as.var.slot) == -1)
            return (-1);
    if (resolve_loop_body(c, n->as.for_.body) == -1)
        return (-1);
    c->r->temps.count = mark;
    if (n->as.for_.else_)
        return (resolve_block(c, n->as.for_.else_));
    return (0);
}

/*
 * An int and a float are the same number only if the float is integral
 * and, converted, gives exactly that int. Comparing both as doubles would
 * be wrong past 2^53 (a double cannot hold every int64).
 */
static int  int_is_float(int64_t i, double f)
{
    if (f != f || f < -9223372036854775808.0 || f >= 9223372036854775808.0
        || f != (double)(int64_t)f)
        return (0);
    return ((int64_t)f == i);
}

/*
 * case values are INT, FLOAT or STRING literals (the parser folds a '-'
 * in). Numbers compare by value, as with is: 1 and 1.0 are the same.
 */
static int  same_case_value(const t_node *a, const t_node *b)
{
    if (a->type == NODE_INT && b->type == NODE_INT)
        return (a->as.integer == b->as.integer);
    if (a->type == NODE_FLOAT && b->type == NODE_FLOAT)
        return (a->as.number == b->as.number);
    if (a->type == NODE_INT && b->type == NODE_FLOAT)
        return (int_is_float(a->as.integer, b->as.number));
    if (a->type == NODE_FLOAT && b->type == NODE_INT)
        return (int_is_float(b->as.integer, a->as.number));
    if (a->type != b->type)
        return (0);
    return (a->as.string.len == b->as.string.len
        && memcmp(a->as.string.chars, b->as.string.chars,
            a->as.string.len) == 0);
}

/* A value repeated in the same switch could never match: it is an error. */
static int  check_case_value(t_ctx *c, const t_node *sw, const t_node *value)
{
    const t_node    *cs;
    const t_node    *v;

    for (cs = sw->as.switch_.cases; cs; cs = cs->next)
    {
        for (v = cs->as.case_.values; v; v = v->next)
        {
            if (v == value)
                return (0);
            if (same_case_value(v, value))
                return (error_set(c->err, ERR_SYNTAX, value->offset,
                        value->len, "duplicate case value (first at line %zu)",
                        source_line(c->src, v->offset)));
        }
    }
    return (0);
}

/*
 * There is no fallthrough, so break and continue inside a case belong to
 * the enclosing loop: the switch does not change loop_depth.
 */
static int  resolve_switch(t_ctx *c, t_node *n)
{
    t_node          *cs;
    const t_node    *v;

    if (rs_resolve_expr(c, n->as.switch_.subject) == -1)
        return (-1);
    for (cs = n->as.switch_.cases; cs; cs = cs->next)
    {
        for (v = cs->as.case_.values; v; v = v->next)
            if (check_case_value(c, n, v) == -1)
                return (-1);
        if (resolve_block(c, cs->as.case_.body) == -1)
            return (-1);
    }
    if (n->as.switch_.default_)
        return (resolve_block(c, n->as.switch_.default_));
    return (0);
}

/* create always gives back the new object: "return;" only. */
static int  resolve_return(t_ctx *c, t_node *n)
{
    if (!c->func)
        return (error_set(c->err, ERR_SYNTAX, n->offset, 6,
                "'return' outside function"));
    if (c->finally_depth > 0)
        return (error_set(c->err, ERR_SYNTAX, n->offset, 6,
                "'return' cannot leave a 'finally' block: it would hide the"
                " error being raised"));
    if (!n->as.ret.value)
        return (0);
    if (c->obj && rs_is_named(c->func->as.func.name, "create"))
        return (error_set(c->err, ERR_SYNTAX, n->as.ret.value->offset,
                n->as.ret.value->len, "'create' cannot return a value:"
                " %.*s(...) always gives back the new object",
                (int)c->obj->as.obj.name.len, c->obj->as.obj.name.ptr));
    return (rs_resolve_expr(c, n->as.ret.value));
}

static int  resolve_if(t_ctx *c, t_node *n)
{
    if (rs_resolve_expr(c, n->as.if_.cond) == -1
        || resolve_block(c, n->as.if_.then) == -1)
        return (-1);
    if (!n->as.if_.else_)
        return (0);
    if (n->as.if_.else_->type == NODE_IF)
        return (resolve_if(c, n->as.if_.else_));
    return (resolve_block(c, n->as.if_.else_));
}

/*
 * break outer / continue outer act on the loop around the current one,
 * so they need two loops (inside the function, or the REPL's top level).
 */
static int  resolve_jump(t_ctx *c, const t_node *n)
{
    const char  *what;

    what = n->type == NODE_BREAK ? "break" : "continue";
    if (c->loop_depth == 0)
        return (error_set(c->err, ERR_SYNTAX, n->offset, strlen(what),
                "'%s' outside loop", what));
    if (n->as.jump.levels > c->loop_depth)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "'%s outer' needs a loop around this one", what));
    if (c->finally_depth > 0
        && n->as.jump.levels > c->loop_depth - c->finally_loops)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "'%s' cannot leave a 'finally' block: it would hide the"
                " error being raised", what));
    return (0);
}

/*
 * The types an except catches: each must name an error type (its
 * constructor is a native named like it).
 */
static int  resolve_error_types(t_ctx *c, t_node *t)
{
    t_symbol    *sym;
    t_name      name;

    for (; t; t = t->next)
    {
        name = t->as.ident.name;
        sym = rs_lookup(c, name);
        if (sym && sym->kind == SYM_OBJ
            && sym->decl->as.obj.error_base != -1)
        {
            rs_set_ref(t, sym);
            continue ;
        }
        if (!sym || sym->kind != SYM_NATIVE
            || error_type_named(name.ptr, name.len) == -1)
            return (error_set(c->err, ERR_SYNTAX, t->offset, name.len,
                    "'%.*s' is not an error type", (int)name.len, name.ptr));
        rs_set_ref(t, sym);
    }
    return (0);
}

/*
 * try / except / finally. The name of an except is a variable of its
 * block only (like a for variable). A finally block always ends where it
 * is: return, break and continue cannot leave it.
 */
static int  resolve_try(t_ctx *c, t_node *n)
{
    t_node  *h;
    size_t  mark;
    int     saved_loops;
    int     status;

    if (resolve_block(c, n->as.try_.body) == -1)
        return (-1);
    for (h = n->as.try_.handlers; h; h = h->next)
    {
        if (resolve_error_types(c, h->as.except.types) == -1)
            return (-1);
        mark = c->r->temps.count;
        if (h->as.except.name.ptr && declare_local(c, &c->r->temps,
                h->as.except.name, SYM_VEMP, h, &h->as.except.slot) == -1)
            return (-1);
        if (resolve_block(c, h->as.except.body) == -1)
            return (-1);
        c->r->temps.count = mark;
    }
    if (!n->as.try_.finally_)
        return (0);
    saved_loops = c->finally_loops;
    c->finally_depth++;
    c->finally_loops = c->loop_depth;
    status = resolve_block(c, n->as.try_.finally_);
    c->finally_depth--;
    c->finally_loops = saved_loops;
    return (status);
}

/* a, b = x, y: every value, then every target. */
static int  resolve_multi_assign(t_ctx *c, t_node *n)
{
    t_node  *e;

    for (e = n->as.multi.values; e; e = e->next)
        if (rs_resolve_expr(c, e) == -1)
            return (-1);
    for (e = n->as.multi.targets; e; e = e->next)
        if (rs_resolve_target(c, e, "assign to") == -1)
            return (-1);
    return (0);
}

/*
 * A comprehension: the result grows in a frame slot of its own (a
 * hidden local of the function, or of the top level), and its loop is a
 * for like any other (its variable lives in it only).
 */
int rs_resolve_comprehension(t_ctx *c, t_node *n)
{
    const t_node    *value;

    value = n->as.comp.add->as.comp.loop;
    if (n->as.comp.kind == COMP_GENERAL)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len, "a"
                " comprehension only goes in [...], or alone inside tuple(...),"
                " set(...), dict(...), any(...), all(...), sum(...), min(...)"
                " or max(...)"));
    if (n->as.comp.kind == COMP_DICT && value->type != NODE_PAIR)
        return (error_set(c->err, ERR_SYNTAX, value->offset, value->len,
                "a dict comprehension makes key: value pairs"));
    if (n->as.comp.kind != COMP_DICT && value->type == NODE_PAIR)
        return (error_set(c->err, ERR_SYNTAX, value->offset, value->len,
                "'key: value' pairs only go inside dict(...)"));
    n->as.comp.slot = c->nslots++;
    n->as.comp.add->as.comp.slot = n->as.comp.slot;
    n->as.comp.add->as.comp.kind = n->as.comp.kind;
    return (resolve_for(c, n->as.comp.loop));
}

static int  resolve_stmt(t_ctx *c, t_node *n)
{
    if (n->type == NODE_MAN)
        return (0);
    if (stack_exhausted())
        return (error_set(c->err, ERR_SYNTAX, n->offset, 1,
                "too deeply nested"));
    if (n->type == NODE_BLOCK)
        return (resolve_block(c, n));
    if (n->type == NODE_VAR)
        return (resolve_var(c, n));
    if (n->type == NODE_FUNC)
        return (resolve_nested_func(c, n));
    if (n->type == NODE_MULTI_VAR)
        return (resolve_multi_var(c, n));
    if (n->type == NODE_IF)
        return (resolve_if(c, n));
    if (n->type == NODE_WHILE)
    {
        if (rs_resolve_expr(c, n->as.while_.cond) == -1)
            return (-1);
        return (resolve_loop_body(c, n->as.while_.body));
    }
    if (n->type == NODE_DO_WHILE)
    {
        if (resolve_loop_body(c, n->as.while_.body) == -1)
            return (-1);
        return (rs_resolve_expr(c, n->as.while_.cond));
    }
    if (n->type == NODE_FOR)
        return (resolve_for(c, n));
    if (n->type == NODE_SWITCH)
        return (resolve_switch(c, n));
    if (n->type == NODE_TRY)
        return (resolve_try(c, n));
    if (n->type == NODE_COMP_ADD && n->as.comp.loop->type == NODE_PAIR)
    {
        if (rs_resolve_expr(c, n->as.comp.loop->as.pair.key) == -1)
            return (-1);
        return (rs_resolve_expr(c, n->as.comp.loop->as.pair.value));
    }
    if (n->type == NODE_COMP_ADD)
        return (rs_resolve_expr(c, n->as.comp.loop));
    if (n->type == NODE_RAISE || n->type == NODE_ASSERT)
    {
        if (rs_resolve_expr(c, n->as.raise.value) == -1)
            return (-1);
        if (n->as.raise.message)
            return (rs_resolve_expr(c, n->as.raise.message));
        return (0);
    }
    if (n->type == NODE_RETURN)
        return (resolve_return(c, n));
    if (n->type == NODE_BREAK || n->type == NODE_CONTINUE)
        return (resolve_jump(c, n));
    if (n->type == NODE_MULTI_ASSIGN)
        return (resolve_multi_assign(c, n));
    if (n->type == NODE_ASSIGN)
    {
        if (rs_resolve_target(c, n->as.assign.target, "assign to") == -1)
            return (-1);
        return (rs_resolve_expr(c, n->as.assign.value));
    }
    if (n->type == NODE_EXPR_STMT)
        return (rs_resolve_expr(c, n->as.expr_stmt.expr));
    return (0);
}

/* What resolving a function body changes in the context, to put back. */
typedef struct s_saved
{
    const t_node    *func;
    const t_node    *obj;
    t_fscope        *scope;
    size_t          locals;
    size_t          temps;
    int             loop_depth;
    int             nslots;
    int             global_init;
    int             depth;
    int             finally_depth;
    int             finally_loops;
}   t_saved;

static void save_ctx(const t_ctx *c, t_saved *s)
{
    s->func = c->func;
    s->obj = c->obj;
    s->scope = c->scope;
    s->locals = c->r->locals.count;
    s->temps = c->r->temps.count;
    s->loop_depth = c->loop_depth;
    s->nslots = c->nslots;
    s->global_init = c->global_init;
    s->depth = c->depth;
    s->finally_depth = c->finally_depth;
    s->finally_loops = c->finally_loops;
}

static void restore_ctx(t_ctx *c, const t_saved *s)
{
    c->func = s->func;
    c->obj = s->obj;
    c->scope = s->scope;
    c->r->locals.count = s->locals;
    c->r->temps.count = s->temps;
    c->loop_depth = s->loop_depth;
    c->nslots = s->nslots;
    c->global_init = s->global_init;
    c->depth = s->depth;
    c->finally_depth = s->finally_depth;
    c->finally_loops = s->finally_loops;
}

/*
 * A method has self as a hidden first parameter (slot 0), so an inner
 * function captures it like any other variable. Its declaration is a
 * PARAM node of its own; self is a keyword, so no name can clash.
 */
static int  declare_self(t_ctx *c, t_node *f)
{
    t_node  *self;

    self = arena_alloc(c->r->arena, sizeof(t_node));
    self->type = NODE_PARAM;
    self->offset = f->offset;
    self->as.param.name.ptr = "self";
    self->as.param.name.len = 4;
    f->as.func.self = self;
    return (declare_local(c, &c->r->locals, self->as.param.name, SYM_PARAM,
            self, &self->as.param.slot));
}

/*
 * What a function declares (params and, anywhere in its body, var, vemp,
 * const, for and except variables and nested funcs; not what its inner
 * functions declare), so that a name means one thing in the whole
 * function: a use that finds a global or a name of an enclosing function
 * while this function declares that name somewhere else is an error
 * (rs_scope_declares). With out NULL it only counts.
 */
static size_t   decl_names(const t_node *n, t_name *out, size_t count);

static size_t   decl_list(const t_node *n, t_name *out, size_t count)
{
    for (; n; n = n->next)
        count = decl_names(n, out, count);
    return (count);
}

static size_t   add_name(t_name name, t_name *out, size_t count)
{
    if (out && name.ptr)
        out[count] = name;
    return (name.ptr ? count + 1 : count);
}

static size_t   decl_names(const t_node *n, t_name *out, size_t count)
{
    const t_node    *h;

    if (!n || stack_exhausted())
        return (count);
    if (n->type == NODE_BLOCK)
        return (decl_list(n->as.list.items, out, count));
    if (n->type == NODE_VAR)
        return (add_name(n->as.var.name, out, count));
    if (n->type == NODE_PARAM)
        return (add_name(n->as.param.name, out, count));
    if (n->type == NODE_MULTI_VAR)
        return (decl_list(n->as.multi.targets, out, count));
    if (n->type == NODE_FUNC)
        return (add_name(n->as.func.name, out, count));
    if (n->type == NODE_IF)
        return (decl_names(n->as.if_.else_, out,
                decl_names(n->as.if_.then, out, count)));
    if (n->type == NODE_WHILE || n->type == NODE_DO_WHILE)
        return (decl_names(n->as.while_.body, out, count));
    if (n->type == NODE_FOR)
    {
        count = decl_list(n->as.for_.more, out,
                add_name(n->as.for_.name, out, count));
        return (decl_names(n->as.for_.else_, out,
                decl_names(n->as.for_.body, out, count)));
    }
    if (n->type == NODE_SWITCH)
    {
        for (h = n->as.switch_.cases; h; h = h->next)
            count = decl_names(h->as.case_.body, out, count);
        return (decl_names(n->as.switch_.default_, out, count));
    }
    if (n->type != NODE_TRY)
        return (count);
    count = decl_names(n->as.try_.body, out, count);
    for (h = n->as.try_.handlers; h; h = h->next)
        count = decl_names(h->as.except.body, out,
                add_name(h->as.except.name, out, count));
    return (decl_names(n->as.try_.finally_, out, count));
}

/*
 * Fills s->names: an open-addressing set (a power of two, at most half
 * full, empty slots with ptr NULL) of what f declares. It lives in the
 * arena, so an allocation failure leaves nothing to free.
 */
static void scope_names(t_ctx *c, t_fscope *s, const t_node *f)
{
    t_name  *list;
    t_name  *set;
    size_t  n;
    size_t  cap;
    size_t  i;
    size_t  j;

    n = decl_list(f->as.func.params, NULL, 0);
    if (!f->as.func.expr_body)
        n = decl_names(f->as.func.body, NULL, n);
    s->names = NULL;
    s->names_cap = 0;
    if (n == 0)
        return ;
    list = arena_alloc(c->r->arena, n * sizeof(t_name));
    i = decl_list(f->as.func.params, list, 0);
    if (!f->as.func.expr_body)
        decl_names(f->as.func.body, list, i);
    for (cap = 8; cap < 2 * n; cap *= 2)
        ;
    set = arena_alloc(c->r->arena, cap * sizeof(t_name));
    for (i = 0; i < n; i++)
    {
        j = rs_name_hash(list[i]) & (cap - 1);
        while (set[j].ptr && !rs_name_eq(set[j], list[i]))
            j = (j + 1) & (cap - 1);
        if (!set[j].ptr)
            set[j] = list[i];
    }
    s->names = set;
    s->names_cap = cap;
}

/* Where s's function declares name (its first declaration), or NULL. */
const t_name    *rs_scope_declares(const t_fscope *s, t_name name)
{
    size_t  j;

    if (s->names_cap == 0)
        return (NULL);
    j = rs_name_hash(name) & (s->names_cap - 1);
    while (s->names[j].ptr)
    {
        if (rs_name_eq(s->names[j], name))
            return (&s->names[j]);
        j = (j + 1) & (s->names_cap - 1);
    }
    return (NULL);
}

/*
 * A function body, at any depth: top level, inside another function
 * (its names stay visible, so using them is a capture) or a lambda.
 * Params and var go to locals, on top of the enclosing function's, and
 * take this function's own frame slots: params 0..n-1 first, the frame
 * size ends up in func.nslots. Each default is resolved before its own
 * param is declared, so it sees the previous params and not the later
 * ones. A body runs later, never while a global is being initialised;
 * break and continue cannot reach the loops around it.
 */
int rs_resolve_function(t_ctx *c, t_node *f, const t_node *obj)
{
    t_saved     saved;
    t_fscope    scope;
    t_node      *p;
    int         status;

    save_ctx(c, &saved);
    scope.func = f;
    scope.depth = c->depth + 1;
    scope.outer = c->scope;
    scope_names(c, &scope, f);
    c->scope = &scope;
    c->depth = scope.depth;
    c->func = f;
    c->obj = obj;
    c->loop_depth = 0;
    c->nslots = 0;
    c->global_init = 0;
    c->finally_depth = 0;
    c->finally_loops = 0;
    status = 0;
    if (obj && f->type == NODE_FUNC && f->as.func.cls == obj
        && !f->as.func.is_static)
        status = declare_self(c, f);
    for (p = f->as.func.params; p && status == 0; p = p->next)
    {
        if (p->as.param.default_)
            status = rs_resolve_expr(c, p->as.param.default_);
        if (status == 0)
            status = declare_local(c, &c->r->locals, p->as.param.name,
                    SYM_PARAM, p, &p->as.param.slot);
    }
    if (status == 0 && f->as.func.expr_body)
        status = rs_resolve_expr(c, f->as.func.body);
    else if (status == 0)
        status = resolve_block(c, f->as.func.body);
    f->as.func.nslots = c->nslots;
    restore_ctx(c, &saved);
    return (status);
}

/*
 * func name(...) { } inside a function: a constant of its block (like
 * vemp) that holds a closure. It is declared before its body is
 * resolved, so it can call itself (a capture of its own name).
 */
static int  resolve_nested_func(t_ctx *c, t_node *n)
{
    if (declare_local(c, &c->r->temps, n->as.func.name, SYM_VEMP, n,
            &n->as.func.slot) == -1)
        return (-1);
    return (rs_resolve_function(c, n, c->obj));
}

/*
 * The fields' values are computed when an object is built, in a frame
 * of their own (init_slots: a comprehension there needs slots); self is
 * not allowed in them. Static vars are computed with the globals, in
 * the place of the obj. Methods get self (resolve_function).
 */
static int  resolve_obj(t_ctx *c, t_node *obj)
{
    t_node  *m;
    int     status;
    int     saved;

    saved = c->nslots;
    c->nslots = 0;
    status = 0;
    for (m = obj->as.obj.members; m && status == 0; m = m->next)
    {
        c->obj = obj;
        if (m->type == NODE_FUNC)
            status = rs_resolve_function(c, m, obj);
        else if (m->as.var.is_static)
        {
            c->global_init = 1;
            c->global_offset = rs_name_offset(c, obj->as.obj.name);
            status = resolve_var_exprs(c, m);
            c->global_init = 0;
        }
        else
            status = resolve_var_exprs(c, m);
        c->obj = NULL;
    }
    obj->as.obj.init_slots = c->nslots;
    c->nslots = saved;
    return (status);
}

/*
 * The value of a top-level var (or of every name of var a, b = ...),
 * already declared: it may only use the globals declared above it.
 */
static int  resolve_global_values(t_ctx *c, t_node *n)
{
    t_node  *e;
    int     status;

    c->global_init = 1;
    if (n->type == NODE_VAR)
    {
        c->global_offset = rs_name_offset(c, n->as.var.name);
        status = resolve_var_exprs(c, n);
    }
    else
    {
        c->global_offset = rs_name_offset(c,
                n->as.multi.targets->as.var.name);
        status = 0;
        for (e = n->as.multi.values; e && status == 0; e = e->next)
            status = rs_resolve_expr(c, e);
    }
    c->global_init = 0;
    return (status);
}

/*
 * The bodies and values of a module's declarations. A REPL line may also
 * hold statements at the top level: they are resolved outside any
 * function, and the slots their for variables need (root's nslots) are
 * counted apart from those of the functions in between.
 */
int  rs_resolve_bodies(t_ctx *c, t_node *root)
{
    t_node  *n;
    int     status;
    int     top_slots;

    top_slots = 0;
    for (n = root->as.list.items; n; n = n->next)
    {
        status = 0;
        c->nslots = top_slots;
        if (n->type == NODE_FUNC)
            status = rs_resolve_function(c, n, NULL);
        else if (n->type == NODE_OBJ)
            status = resolve_obj(c, n);
        else if (n->type == NODE_VAR || n->type == NODE_MULTI_VAR)
        {
            status = resolve_global_values(c, n);
            top_slots = c->nslots;
        }
        else if (n->type != NODE_IMPORT)
        {
            status = resolve_stmt(c, n);
            top_slots = c->nslots;
        }
        if (status == -1)
            return (-1);
    }
    root->as.list.nslots = top_slots;
    return (0);
}
