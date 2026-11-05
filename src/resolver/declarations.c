/* Pass 1: top-level declarations of a module. */

#include <string.h>
#include "resolver/resolver_internal.h"

static t_name   member_name(const t_node *m)
{
    if (m->type == NODE_VAR)
        return (m->as.var.name);
    return (m->as.func.name);
}

const t_node *rs_find_member(const t_node *obj, t_name name)
{
    const t_node    *m;

    for (m = obj->as.obj.members; m; m = m->next)
        if (rs_name_eq(member_name(m), name))
            return (m);
    return (NULL);
}

/*
 * Fields and methods share one namespace inside an obj. Returns the
 * arity of create, the constructor (0 when there is none: Point()).
 */
static int  check_obj_members(t_ctx *c, const t_node *obj, t_arity *arity)
{
    const t_node    *m;
    const t_node    *prev;
    t_name          name;

    arity->min = 0;
    arity->max = 0;
    for (m = obj->as.obj.members; m; m = m->next)
    {
        name = member_name(m);
        prev = rs_find_member(obj, name);
        if (prev != m)
            return (error_set(c->err, ERR_NAME, rs_name_offset(c, name),
                    name.len, "'%.*s' was already declared in object"
                    " '%.*s' at line %zu", (int)name.len, name.ptr,
                    (int)obj->as.obj.name.len, obj->as.obj.name.ptr,
                    source_line(c->src, rs_name_offset(c, member_name(prev)))));
        if (m->type == NODE_FUNC && rs_is_named(name, "create"))
            *arity = rs_func_arity(m);
    }
    return (0);
}

/* At most one prog per file, with 0 or 2 parameters (argc, argv). */
static int  check_prog(t_ctx *c, const t_node *n, const t_node **first)
{
    char    where[ERR_MSG_SIZE];
    t_name  name;
    int     params;

    name = n->as.func.name;
    if (*first)
    {
        rs_where_str(c, (*first)->offset, where, sizeof(where));
        return (error_set(c->err, ERR_SYNTAX, rs_name_offset(c, name), name.len,
                "only one 'prog' is allowed; the first one is at %s",
                where));
    }
    *first = n;
    params = n->as.func.nparams;
    if ((params != 0 && params != 2) || n->as.func.has_rest
        || n->as.func.nrequired != params)
        return (error_set(c->err, ERR_SYNTAX, rs_name_offset(c, name), name.len,
                "'prog' takes no parameters or two plain ones (argc, argv)"));
    return (0);
}

/*
 * A global var or const gets the next global slot, in declaration order.
 * Also used for a var inside a statement typed at the top of the REPL.
 */
int rs_declare_global_var(t_ctx *c, t_node *n)
{
    t_symbol    sym;

    sym = rs_make_symbol(c, n->as.var.name, SYM_GLOBAL, n, 0);
    sym.slot = c->mod->nglobals;
    sym.constant = n->as.var.is_const;
    if (rs_declare(c, &c->mod->globals, &sym) == -1)
        return (-1);
    n->as.var.is_global = 1;
    n->as.var.slot = c->mod->nglobals++;
    return (0);
}

static int  declare_top(t_ctx *c, t_node *n, const t_node **prog)
{
    t_symbol    sym;
    t_node      *v;
    t_arity     arity;

    if (n->type == NODE_VAR)
        return (rs_declare_global_var(c, n));
    if (n->type == NODE_MULTI_VAR)
    {
        for (v = n->as.multi.targets; v; v = v->next)
            if (rs_declare_global_var(c, v) == -1)
                return (-1);
        return (0);
    }
    if (n->type != NODE_IMPORT && n->type != NODE_FUNC && n->type != NODE_OBJ)
        return (0);
    if (n->type == NODE_IMPORT)
        return (rs_resolve_import(c, n));
    if (n->type == NODE_FUNC && n->as.func.is_prog)
    {
        if (check_prog(c, n, prog) == -1)
            return (-1);
        sym = rs_make_symbol(c, n->as.func.name, SYM_PROG, n, 0);
    }
    else if (n->type == NODE_FUNC)
    {
        sym = rs_make_symbol(c, n->as.func.name, SYM_FUNC, n, 0);
        sym.arity = rs_func_arity(n);
    }
    else
    {
        if (check_obj_members(c, n, &arity) == -1)
            return (-1);
        sym = rs_make_symbol(c, n->as.obj.name, SYM_OBJ, n, 0);
        sym.arity = arity;
    }
    return (rs_declare(c, &c->mod->globals, &sym));
}

/*
 * Once every name is known: what each obj extends, where its fields go,
 * and how many arguments building one takes (its create may be its
 * parent's).
 */
int rs_layout_objects(t_ctx *c, t_node *root)
{
    t_node      *n;
    t_symbol    *sym;

    for (n = root->as.list.items; n; n = n->next)
    {
        if (n->type != NODE_OBJ)
            continue ;
        if (rs_obj_layout(c, n) == -1)
            return (-1);
        sym = symtab_find(&c->mod->globals, n->as.obj.name);
        if (sym && sym->decl == n)
            sym->arity = rs_obj_arity(n);
    }
    return (0);
}

/*
 * Registers every top-level name before any body is looked at, so a
 * function can call another one declared further down. A prog declared
 * on an earlier REPL line also counts.
 */
int  rs_declare_globals(t_ctx *c, t_node *root)
{
    t_node          *n;
    const t_node    *prog;
    size_t          i;

    prog = NULL;
    for (i = 0; i < c->mod->globals.count; i++)
        if (c->mod->globals.items[i].kind == SYM_PROG)
            prog = c->mod->globals.items[i].decl;
    for (n = root->as.list.items; n; n = n->next)
        if (declare_top(c, n, &prog) == -1)
            return (-1);
    return (0);
}
