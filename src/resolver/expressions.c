/* Expressions: names, calls, fields, self, assignment targets. */

#include "core/stack.h"
#include "resolver/natives.h"
#include "resolver/resolver_internal.h"

/*
 * The symbol an IDENT names, or a NameError. Globals are initialised in
 * order before prog runs, so a global's value may only use the globals
 * declared above it (functions run later and may use any).
 */
static int  find_name(t_ctx *c, const t_node *ident, t_symbol **out)
{
    char    where[ERR_MSG_SIZE];
    t_name  name;

    name = ident->as.ident.name;
    *out = rs_lookup(c, name);
    if (!*out)
        return (error_set(c->err, ERR_NAME, ident->offset, name.len,
                "name '%.*s' is not defined", (int)name.len, name.ptr));
    if (c->global_init && (*out)->kind == SYM_GLOBAL
        && (*out)->offset >= c->global_offset)
    {
        rs_where_str(c, (*out)->offset, where, sizeof(where));
        return (error_set(c->err, ERR_NAME, ident->offset, name.len,
                "global '%.*s' is used before its declaration at %s",
                (int)name.len, name.ptr, where));
    }
    return (0);
}

/*
 * A name used as a value: variables, functions, natives and objects are
 * values; the prog and modules are not.
 */
static int  resolve_value_name(t_ctx *c, t_node *ident)
{
    t_symbol    *sym;
    t_name      name;

    if (find_name(c, ident, &sym) == -1)
        return (-1);
    name = ident->as.ident.name;
    if (sym->kind == SYM_PROG || sym->kind == SYM_MODULE)
        return (error_set(c->err, ERR_TYPE, ident->offset, name.len,
                "'%.*s' is %s, not a value", (int)name.len, name.ptr,
                rs_kind_desc(sym->kind)));
    rs_ref_name(c, ident, sym);
    return (0);
}

/*
 * m.name, a FIELD whose object names a module: looked up now, among
 * what m declares. The node becomes an IDENT for that name with its ref
 * (offset and len still cover "m.name"), so callers treat it like any
 * other name.
 */
static int  resolve_qualified(t_ctx *c, t_node *n, const t_symbol *module,
                const t_symbol **out)
{
    const t_module  *m;
    const t_symbol  *sym;
    t_name          name;

    m = c->r->modules[module->module];
    name = n->as.field.name;
    sym = rs_exported(m, name);
    if (!sym)
        return (error_set(c->err, ERR_ATTRIBUTE, rs_name_offset(c, name),
                name.len, "module '%s' has no attribute '%.*s'", m->name,
                (int)name.len, name.ptr));
    n->type = NODE_IDENT;
    n->as.ident.name = name;
    rs_set_ref(n, sym);
    *out = sym;
    return (0);
}

/* An IDENT naming a module, for m.name; NULL (no error) when it is not one. */
static const t_symbol   *module_name(const t_ctx *c, const t_node *n)
{
    t_symbol    *sym;

    if (n->type != NODE_IDENT)
        return (NULL);
    sym = rs_lookup(c, n->as.ident.name);
    if (sym && sym->kind == SYM_MODULE)
        return (sym);
    return (NULL);
}

/*
 * self is the hidden first parameter of the method it is in (captured
 * like any variable inside a lambda there): the node becomes a name of
 * it. A static method has none.
 */
static int  resolve_self(t_ctx *c, t_node *n)
{
    t_symbol    *sym;
    t_name      name;

    name.ptr = "self";
    name.len = 4;
    sym = rs_lookup(c, name);
    if (!sym && c->obj && c->func)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len, "'%s' is"
                " not available in a static method", n->type == NODE_SUPER
                ? "super" : "self"));
    if (!sym)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "'%s' can only be used inside the methods of an obj",
                n->type == NODE_SUPER ? "super" : "self"));
    n->type = NODE_IDENT;
    n->as.ident.name = name;
    rs_ref_name(c, n, sym);
    return (0);
}

/* nargs must fit arity; call is the CALL node, the caret covers it. */
static int  check_arity(t_ctx *c, const t_node *call, t_name name,
                t_arity arity, int nargs)
{
    if (nargs >= arity.min && (arity.max == -1 || nargs <= arity.max))
        return (0);
    if (arity.max == -1)
        return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                "%.*s() takes at least %d argument%s but %d %s given",
                (int)name.len, name.ptr, arity.min, arity.min == 1 ? "" : "s",
                nargs, nargs == 1 ? "was" : "were"));
    if (arity.min != arity.max)
        return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                "%.*s() takes %d to %d arguments but %d %s given",
                (int)name.len, name.ptr, arity.min, arity.max, nargs,
                nargs == 1 ? "was" : "were"));
    return (error_set(c->err, ERR_TYPE, call->offset, call->len,
            "%.*s() takes %d argument%s but %d %s given", (int)name.len,
            name.ptr, arity.min, arity.min == 1 ? "" : "s", nargs,
            nargs == 1 ? "was" : "were"));
}

/*
 * self.name inside a method: the members of its obj (and of what it
 * extends) are known, so a missing one is an AttributeError now. A
 * field gets its index; a method is found when it runs (a child may
 * redefine it), but its arity is checked. call is the CALL node when it
 * is called (a field may hold a function: that is fine too).
 */
static int  resolve_self_member(t_ctx *c, t_node *field, const t_node *call,
                int nargs)
{
    const t_node    *m;
    t_name          name;

    if (resolve_self(c, field->as.field.object) == -1)
        return (-1);
    name = field->as.field.name;
    if (rs_obj_member(c, c->obj, name, rs_name_offset(c, name), &m) == -1)
        return (-1);
    if (m && m->type == NODE_VAR)
    {
        field->as.field.member = m;
        field->as.field.slot = m->as.var.is_static ? -1 : m->as.var.slot;
    }
    if (!call || !m || m->type != NODE_FUNC)
        return (0);
    return (check_arity(c, call, name, rs_func_arity(m), nargs));
}

/*
 * super.name(...): the method of what the obj extends, called on self
 * (found now: it is that one, whatever redefines it). On an error type,
 * super.create(message) sets the message.
 */
static int  resolve_super(t_ctx *c, t_node *field, const t_node *call,
                int nargs)
{
    const t_node    *cls;
    const t_node    *m;
    t_name          name;
    t_arity         arity;

    name = field->as.field.name;
    cls = c->obj;
    if (!call)
        return (error_set(c->err, ERR_SYNTAX, field->offset, field->len,
                "super is only for calling: super.%.*s(...)", (int)name.len,
                name.ptr));
    if (resolve_self(c, field->as.field.object) == -1)
        return (-1);
    m = cls->as.obj.base ? rs_obj_find(cls->as.obj.base, name) : NULL;
    if (!m && cls->as.obj.error_base != -1 && rs_is_named(name, "create"))
    {
        field->as.field.slot = -2;
        arity.min = 0;
        arity.max = 1;
        return (check_arity(c, call, name, arity, nargs));
    }
    if (!m || m->type != NODE_FUNC || m->as.func.is_static)
        return (error_set(c->err, ERR_ATTRIBUTE, rs_name_offset(c, name),
                name.len, "what '%.*s' extends has no method '%.*s'",
                (int)cls->as.obj.name.len, cls->as.obj.name.ptr,
                (int)name.len, name.ptr));
    field->as.field.member = m;
    field->as.field.slot = -3;
    return (check_arity(c, call, name, rs_func_arity(m), nargs));
}

/* An IDENT naming an obj, for Name.member; NULL (no error) if it is not one. */
static const t_symbol   *obj_name(const t_ctx *c, const t_node *n)
{
    t_symbol    *sym;

    if (n->type != NODE_IDENT)
        return (NULL);
    sym = rs_lookup(c, n->as.ident.name);
    if (sym && sym->kind == SYM_OBJ)
        return (sym);
    return (NULL);
}

/*
 * x.name, not called. The name itself is checked at runtime, except on
 * self and on a module (m.f is the function f of m, a value too).
 */
static int  resolve_field(t_ctx *c, t_node *n)
{
    t_node          *object;
    const t_symbol  *module;
    const t_symbol  *sym;
    const t_node    *m;

    object = n->as.field.object;
    if (object->type == NODE_SELF)
        return (resolve_self_member(c, n, NULL, 0));
    if (object->type == NODE_SUPER)
        return (resolve_super(c, n, NULL, 0));
    if (obj_name(c, object))
        return (rs_obj_static(c, n, obj_name(c, object)->decl, &m));
    module = module_name(c, object);
    if (!module)
        return (rs_resolve_expr(c, object));
    return (resolve_qualified(c, n, module, &sym));
}

/* What a call passes: positional arguments, named ones, and ...t. */
typedef struct s_args
{
    int npos;
    int nnamed;
    int spread;
    int npairs;
}   t_args;

/* The parameter of f called name (never ...rest), or -1. */
static int  param_index(const t_node *f, t_name name)
{
    const t_node    *p;
    int             i;

    i = 0;
    for (p = f->as.func.params; p; p = p->next)
    {
        if (!p->as.param.is_rest && rs_name_eq(p->as.param.name, name))
            return (i);
        i++;
    }
    return (-1);
}

/*
 * The named arguments of a call to f, a func known now: each must be a
 * parameter not already given, and (without ...t, whose length is only
 * known when it runs) every parameter without a default must be given.
 */
static int  check_named(t_ctx *c, t_node *call, const t_node *f,
                const t_args *a)
{
    t_node          *arg;
    const t_node    *other;
    const t_node    *p;
    t_name          fn;
    int             i;

    fn = f->as.func.name;
    for (arg = call->as.call.args; arg; arg = arg->next)
    {
        if (arg->type != NODE_NAMED_ARG)
            continue ;
        arg->as.named.index = param_index(f, arg->as.named.name);
        if (arg->as.named.index == -1)
            return (error_set(c->err, ERR_TYPE, arg->offset,
                    arg->as.named.name.len, "%.*s() has no parameter"
                    " '%.*s'", (int)fn.len, fn.ptr,
                    (int)arg->as.named.name.len, arg->as.named.name.ptr));
        for (other = call->as.call.args; other != arg; other = other->next)
            if (other->type == NODE_NAMED_ARG
                && other->as.named.index == arg->as.named.index)
                return (error_set(c->err, ERR_SYNTAX, arg->offset,
                        arg->as.named.name.len, "argument '%.*s' is given"
                        " twice", (int)arg->as.named.name.len,
                        arg->as.named.name.ptr));
        if (!a->spread && arg->as.named.index < a->npos)
            return (error_set(c->err, ERR_TYPE, arg->offset,
                    arg->as.named.name.len, "%.*s() got two values for"
                    " '%.*s'", (int)fn.len, fn.ptr,
                    (int)arg->as.named.name.len, arg->as.named.name.ptr));
    }
    if (a->spread)
        return (0);
    i = 0;
    for (p = f->as.func.params; p; p = p->next, i++)
    {
        if (p->as.param.is_rest || p->as.param.default_ || i < a->npos)
            continue ;
        for (other = call->as.call.args; other; other = other->next)
            if (other->type == NODE_NAMED_ARG && other->as.named.index == i)
                break ;
        if (!other)
            return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                    "%.*s() is missing argument '%.*s'", (int)fn.len, fn.ptr,
                    (int)p->as.param.name.len, p->as.param.name.ptr));
    }
    return (0);
}

/*
 * A call to a func known now: too many positional arguments (or, with
 * none named, too few) is the usual arity error; named ones are checked
 * one by one. With ...t only the names can be checked.
 */
static int  check_func_call(t_ctx *c, t_node *call, const t_node *f,
                const t_args *a)
{
    t_arity arity;
    t_arity positional;

    arity = rs_func_arity(f);
    if (!a->spread && (a->nnamed == 0 || (arity.max != -1
                && a->npos > arity.max)))
    {
        positional = arity;
        if (a->nnamed && a->npos > arity.max)
            positional.min = 0;
        if (check_arity(c, call, f->as.func.name, positional, a->npos) == -1)
            return (-1);
    }
    return (check_named(c, call, f, a));
}

/*
 * A call to a native: named arguments only where it has them (see
 * resolver/natives.h), each once, and a positional one that may be
 * named counts as given. Then the arity, unless ...t hides the count.
 */
static int  check_native_call(t_ctx *c, t_node *call, t_name name,
                const t_symbol *sym, const t_args *a)
{
    const t_native_def  *def;
    const t_node        *arg;
    const t_node        *other;
    t_name              an;
    int                 nargs;
    int                 i;

    def = &g_native_defs[sym->slot];
    nargs = a->npos;
    for (arg = call->as.call.args; arg; arg = arg->next)
    {
        if (arg->type != NODE_NAMED_ARG)
            continue ;
        an = arg->as.named.name;
        if (!def->names && !def->kw)
            return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                    "%.*s() takes no named arguments", (int)name.len,
                    name.ptr));
        i = native_param(def->names, an.ptr, an.len);
        if (i == -1 && native_param(def->kw, an.ptr, an.len) == -1)
            return (error_set(c->err, ERR_TYPE, arg->offset, an.len,
                    "%.*s() has no parameter '%.*s'", (int)name.len,
                    name.ptr, (int)an.len, an.ptr));
        for (other = call->as.call.args; other != arg; other = other->next)
            if (other->type == NODE_NAMED_ARG
                && rs_name_eq(other->as.named.name, an))
                return (error_set(c->err, ERR_SYNTAX, arg->offset, an.len,
                        "argument '%.*s' is given twice", (int)an.len,
                        an.ptr));
        if (i != -1 && !a->spread && i < a->npos)
            return (error_set(c->err, ERR_TYPE, arg->offset, an.len,
                    "%.*s() got two values for '%.*s'", (int)name.len,
                    name.ptr, (int)an.len, an.ptr));
        if (i + 1 > nargs)
            nargs = i + 1;
    }
    if (a->spread)
        return (0);
    if (def->max != -1 && nargs > def->max && native_kw_hint(def))
        return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                "%.*s() takes %d positional argument%s but %d %s given: %s",
                (int)name.len, name.ptr, def->max, def->max == 1 ? "" : "s",
                nargs, nargs == 1 ? "was" : "were", native_kw_hint(def)));
    return (check_arity(c, call, name, sym->arity, nargs));
}

/*
 * callee (an IDENT, maybe rewritten from m.f) names sym. A func, an obj
 * or a native is known now, so what it is given is checked here; a
 * variable may hold anything callable, checked when it runs.
 */
static int  check_callable(t_ctx *c, t_node *call, t_node *callee,
                const t_symbol *sym, const t_args *a)
{
    t_name  name;

    name = callee->as.ident.name;
    if (sym->kind == SYM_PROG || sym->kind == SYM_MODULE)
        return (error_set(c->err, ERR_TYPE, callee->offset, callee->len,
                "'%.*s' is %s and cannot be called", (int)name.len,
                name.ptr, rs_kind_desc(sym->kind)));
    rs_ref_name(c, callee, sym);
    if (sym->kind == SYM_FUNC)
        return (check_func_call(c, call, sym->decl, a));
    if (rs_is_variable(sym->kind))
        return (0);
    if (sym->kind == SYM_NATIVE)
        return (check_native_call(c, call, name, sym, a));
    if (a->nnamed)
        return (error_set(c->err, ERR_TYPE, call->offset, call->len,
                "%.*s() takes no named arguments", (int)name.len, name.ptr));
    if (a->spread)
        return (0);
    return (check_arity(c, call, name, sym->arity, a->npos));
}

/* f(...), Point(...) or print(...): what is called is known, so is its arity. */
static int  resolve_named_call(t_ctx *c, t_node *call, t_node *callee,
                const t_args *a)
{
    t_symbol    *sym;

    if (find_name(c, callee, &sym) == -1)
        return (-1);
    return (check_callable(c, call, callee, sym, a));
}

/*
 * key: value pairs only go inside dict(...), the native named directly,
 * and there every argument is one.
 */
static int  check_pairs(t_ctx *c, const t_node *call, const t_args *a)
{
    const t_node    *callee;
    const t_node    *arg;
    const t_symbol  *sym;
    int             is_dict;

    callee = call->as.call.callee;
    sym = callee->type == NODE_IDENT ? rs_lookup(c, callee->as.ident.name)
        : NULL;
    is_dict = sym && sym->kind == SYM_NATIVE && rs_is_named(sym->name, "dict");
    if (a->npairs == 0 && (!is_dict || a->npos == 0))
        return (0);
    for (arg = call->as.call.args; arg; arg = arg->next)
    {
        if (!is_dict && arg->type == NODE_PAIR)
            return (error_set(c->err, ERR_SYNTAX, arg->offset, arg->len,
                    "'key: value' pairs only go inside dict(...)"));
        if (is_dict && arg->type != NODE_PAIR)
            return (error_set(c->err, ERR_SYNTAX, arg->offset, arg->len,
                    "dict() takes key: value pairs: dict(\"a\": 1)"));
    }
    return (0);
}

/* Resolves every argument and counts them by kind. */
static int  resolve_args(t_ctx *c, t_node *arg, t_args *a)
{
    a->npos = 0;
    a->nnamed = 0;
    a->spread = 0;
    a->npairs = 0;
    for (; arg; arg = arg->next)
    {
        if (arg->type == NODE_PAIR)
        {
            a->npairs++;
            a->npos += 2;
            if (rs_resolve_expr(c, arg->as.pair.key) == -1
                || rs_resolve_expr(c, arg->as.pair.value) == -1)
                return (-1);
            continue ;
        }
        if (arg->type == NODE_NAMED_ARG)
        {
            a->nnamed++;
            if (rs_resolve_expr(c, arg->as.named.value) == -1)
                return (-1);
            continue ;
        }
        if (arg->type == NODE_SPREAD)
            a->spread = 1;
        else
            a->npos++;
        if (rs_resolve_expr(c, arg->type == NODE_SPREAD
                ? arg->as.spread.value : arg) == -1)
            return (-1);
    }
    return (0);
}

/* The natives a comprehension can go alone in, and what it makes there. */
static const struct { const char *name; int kind; }   g_comp_calls[] = {
    {"tuple", COMP_TUPLE}, {"set", COMP_SET}, {"dict", COMP_DICT},
    {"any", COMP_ANY}, {"all", COMP_ALL}, {"sum", COMP_SUM},
    {"min", COMP_MIN}, {"max", COMP_MAX}, {NULL, 0},
};

/*
 * tuple(x for ...), set(...), dict(k: v for ...), any(...)...: the call
 * becomes the comprehension itself (it makes the tuple, set or dict
 * directly, or the answer), on the same span of source.
 */
static int  resolve_comprehension_call(t_ctx *c, t_node *n)
{
    const t_node    *callee;
    const t_symbol  *sym;
    t_node          *next;
    size_t          offset;
    size_t          len;
    int             kind;
    int             i;

    callee = n->as.call.callee;
    sym = callee->type == NODE_IDENT ? rs_lookup(c, callee->as.ident.name)
        : NULL;
    kind = COMP_GENERAL;
    for (i = 0; sym && sym->kind == SYM_NATIVE && g_comp_calls[i].name; i++)
        if (rs_is_named(sym->name, g_comp_calls[i].name))
            kind = g_comp_calls[i].kind;
    if (n->as.call.args->next)
        kind = COMP_GENERAL;
    offset = n->offset;
    len = n->len;
    next = n->next;
    *n = *n->as.call.args;
    n->offset = offset;
    n->len = len;
    n->next = next;
    n->as.comp.kind = kind;
    return (rs_resolve_comprehension(c, n));
}

/*
 * Anything can be called: a name, a method (x.m(...): its arity is only
 * known on self, on anything else when it runs), or any expression that
 * gives a function (l[0](), make()(...)).
 */
static int  resolve_call(t_ctx *c, t_node *n)
{
    t_node          *callee;
    const t_symbol  *module;
    const t_symbol  *sym;
    const t_node    *m;
    t_args          a;

    if (n->as.call.args && n->as.call.args->type == NODE_COMPREHENSION
        && n->as.call.args->as.comp.kind == COMP_GENERAL)
        return (resolve_comprehension_call(c, n));
    if (resolve_args(c, n->as.call.args, &a) == -1
        || check_pairs(c, n, &a) == -1)
        return (-1);
    callee = n->as.call.callee;
    if (callee->type == NODE_IDENT)
        return (resolve_named_call(c, n, callee, &a));
    if (callee->type != NODE_FIELD)
        return (rs_resolve_expr(c, callee));
    if (callee->as.field.object->type == NODE_SELF)
        return (resolve_self_member(c, callee, n, a.npos));
    if (callee->as.field.object->type == NODE_SUPER)
        return (resolve_super(c, callee, n, a.npos));
    if (obj_name(c, callee->as.field.object))
    {
        if (rs_obj_static(c, callee, obj_name(c,
                    callee->as.field.object)->decl, &m) == -1)
            return (-1);
        return (m->type == NODE_FUNC ? check_func_call(c, n, m, &a) : 0);
    }
    module = module_name(c, callee->as.field.object);
    if (!module)
        return (rs_resolve_expr(c, callee->as.field.object));
    if (resolve_qualified(c, callee, module, &sym) == -1)
        return (-1);
    return (check_callable(c, n, callee, sym, &a));
}

/* "a function call", "a literal"... for "cannot assign to" messages. */
static const char   *target_desc(const t_node *n)
{
    if (n->type == NODE_CALL)
        return ("a function call");
    if (n->type == NODE_INCDEC)
        return ("the result of '++' or '--'");
    if (n->type == NODE_INT || n->type == NODE_FLOAT || n->type == NODE_STRING
        || n->type == NODE_BOOL || n->type == NODE_NONE
        || n->type == NODE_LIST)
        return ("a literal");
    if (n->type == NODE_SELF)
        return ("'self'");
    return ("an expression");
}

/*
 * A name being assigned to: it must be a variable, and not one that
 * belongs to another module (read-only outside it: only that module's
 * own functions can change it). n is an IDENT, maybe rewritten from m.x.
 */
static int  check_assignable(t_ctx *c, t_node *n, const t_symbol *sym,
                const char *action)
{
    t_name  name;

    name = n->as.ident.name;
    if (!rs_is_variable(sym->kind))
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "cannot %s '%.*s': it is %s", action, (int)name.len,
                name.ptr, rs_kind_desc(sym->kind)));
    if (sym->constant)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "cannot %s '%.*s': it is a constant", action, (int)name.len,
                name.ptr));
    if (sym->module != c->mod->index)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "cannot %s '%.*s': it belongs to module '%s', and only its"
                " own functions can change it", action, (int)name.len,
                name.ptr, c->r->modules[sym->module]->name));
    rs_ref_name(c, n, sym);
    return (0);
}

/*
 * The left side of an assignment, or the operand of ++/--: a variable,
 * x[i] or x.field. action is "assign to", "use '++' on"...
 */
int  rs_resolve_target(t_ctx *c, t_node *n, const char *action)
{
    t_symbol        *sym;
    const t_symbol  *module;
    const t_symbol  *qualified;
    const t_node    *m;

    if (n->type == NODE_IDENT)
    {
        if (find_name(c, n, &sym) == -1)
            return (-1);
        return (check_assignable(c, n, sym, action));
    }
    if (n->type == NODE_INDEX && n->as.index.slice)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "cannot %s a slice: a slice is a new value", action));
    if (n->type == NODE_INDEX)
        return (rs_resolve_expr(c, n));
    if (n->type != NODE_FIELD)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len,
                "cannot %s %s", action, target_desc(n)));
    if (n->as.field.object->type == NODE_SELF
        && resolve_self_member(c, n, NULL, 0) == -1)
        return (-1);
    if (n->as.field.object->type == NODE_IDENT && n->as.field.member
        && n->as.field.member->type == NODE_FUNC)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len, "cannot %s"
                " a method", action));
    if (n->as.field.object->type == NODE_IDENT && n->as.field.member)
        return (0);
    if (n->as.field.object->type == NODE_SUPER)
        return (resolve_super(c, n, NULL, 0));
    if (obj_name(c, n->as.field.object))
    {
        if (rs_obj_static(c, n, obj_name(c, n->as.field.object)->decl, &m)
            == -1)
            return (-1);
        if (m->type == NODE_FUNC)
            return (error_set(c->err, ERR_SYNTAX, n->offset, n->len, "cannot"
                    " %s a method", action));
        return (0);
    }
    module = module_name(c, n->as.field.object);
    if (!module)
        return (rs_resolve_expr(c, n->as.field.object));
    if (resolve_qualified(c, n, module, &qualified) == -1)
        return (-1);
    return (check_assignable(c, n, qualified, action));
}

static int  resolve_list(t_ctx *c, t_node *n)
{
    for (; n; n = n->next)
        if (rs_resolve_expr(c, n) == -1)
            return (-1);
    return (0);
}

int  rs_resolve_expr(t_ctx *c, t_node *n)
{
    if (stack_exhausted())
        return (error_set(c->err, ERR_SYNTAX, n->offset, 1,
                "too deeply nested"));
    if (n->type == NODE_IDENT)
        return (resolve_value_name(c, n));
    if (n->type == NODE_SELF)
        return (resolve_self(c, n));
    if (n->type == NODE_SUPER)
        return (error_set(c->err, ERR_SYNTAX, n->offset, n->len, "super is"
                " only for calling a method: super.name(...)"));
    if (n->type == NODE_LIST)
        return (resolve_list(c, n->as.list.items));
    if (n->type == NODE_UNARY)
        return (rs_resolve_expr(c, n->as.unary.operand));
    if (n->type == NODE_BINARY)
    {
        if (rs_resolve_expr(c, n->as.binary.left) == -1)
            return (-1);
        return (rs_resolve_expr(c, n->as.binary.right));
    }
    if (n->type == NODE_TERNARY)
    {
        if (rs_resolve_expr(c, n->as.ternary.cond) == -1
            || rs_resolve_expr(c, n->as.ternary.then) == -1)
            return (-1);
        return (rs_resolve_expr(c, n->as.ternary.else_));
    }
    if (n->type == NODE_INCDEC)
        return (rs_resolve_target(c, n->as.incdec.target,
                n->as.incdec.op == TOK_PLUS_PLUS ? "use '++' on"
                : "use '--' on"));
    if (n->type == NODE_CALL)
        return (resolve_call(c, n));
    if (n->type == NODE_INDEX)
    {
        if (rs_resolve_expr(c, n->as.index.object) == -1
            || (n->as.index.index
                && rs_resolve_expr(c, n->as.index.index) == -1)
            || (n->as.index.end && rs_resolve_expr(c, n->as.index.end) == -1))
            return (-1);
        if (n->as.index.step)
            return (rs_resolve_expr(c, n->as.index.step));
        return (0);
    }
    if (n->type == NODE_FIELD)
        return (resolve_field(c, n));
    if (n->type == NODE_FUNC)
        return (rs_resolve_function(c, n, c->obj));
    if (n->type == NODE_COMPREHENSION)
        return (rs_resolve_comprehension(c, n));
    if (n->type == NODE_FSTRING)
    {
        for (n = n->as.list.items; n; n = n->next)
            if (n->type == NODE_FORMAT
                && rs_resolve_expr(c, n->as.format.expr) == -1)
                return (-1);
    }
    return (0);
}
