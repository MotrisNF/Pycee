/* Names and scopes: lookup, declaration, references and shared helpers. */

#include <stdio.h>
#include <string.h>
#include "resolver/resolver_internal.h"

/*
 * The visible symbol a name means: the innermost one. locals and temps
 * hold the functions being resolved one on top of the other, so the last
 * one pushed in each is the innermost there, and of the two, the deeper
 * function wins (inside one function a name is in at most one of them).
 * Then the globals of the module, then the natives.
 */
t_symbol *rs_lookup(const t_ctx *c, t_name name)
{
    t_symbol    *temp;
    t_symbol    *local;

    temp = symtab_find_last(&c->r->temps, name);
    local = symtab_find_last(&c->r->locals, name);
    if (temp && (!local || temp->depth >= local->depth))
        return (temp);
    if (local)
        return (local);
    temp = symtab_find(&c->mod->globals, name);
    if (!temp)
        temp = symtab_find(&c->r->natives, name);
    return (temp);
}

size_t   rs_name_offset(const t_ctx *c, t_name name)
{
    return (source_offset(c->src, name.ptr));
}

/*
 * Writes "line N" for a global offset, or "line N of file" when it is in
 * another source than the one being resolved (an earlier REPL line, an
 * imported module). The offset may belong to any source, so it is always
 * looked up: source_line on the wrong source would read out of bounds.
 */
void rs_where_str(const t_ctx *c, size_t offset, char *buf, size_t size)
{
    const t_source  *where;

    where = sources_find(c->r->sources, offset);
    if (where == c->src)
        snprintf(buf, size, "line %zu", source_line(where, offset));
    else
        snprintf(buf, size, "line %zu of %s", source_line(where, offset),
            where->path);
}

/* NameError for a name that repeats a visible one. */
static int  error_redeclared(t_ctx *c, t_name name, const t_symbol *prev)
{
    char    where[ERR_MSG_SIZE];
    size_t  offset;

    offset = rs_name_offset(c, name);
    if (prev->kind == SYM_NATIVE)
        return (error_set(c->err, ERR_NAME, offset, name.len,
                "'%.*s' is a built-in function and cannot be redeclared",
                (int)name.len, name.ptr));
    rs_where_str(c, prev->offset, where, sizeof(where));
    return (error_set(c->err, ERR_NAME, offset, name.len,
            "'%.*s' was already declared at %s", (int)name.len, name.ptr,
            where));
}

/*
 * Adds a symbol to t. A name may hide a global or a name of an enclosing
 * function (decided by the author: each function is a unit of its own),
 * but never a native, nor another name of the same function (or of the
 * same top level).
 */
int  rs_declare(t_ctx *c, t_symtab *t, const t_symbol *sym)
{
    t_symbol    *prev;

    prev = rs_lookup(c, sym->name);
    if (prev && (prev->kind == SYM_NATIVE || prev->depth >= sym->depth))
        return (error_redeclared(c, sym->name, prev));
    symtab_push(t, sym);
    return (0);
}

int  rs_count_list(const t_node *n)
{
    int count;

    count = 0;
    for (; n; n = n->next)
        count++;
    return (count);
}

t_symbol rs_make_symbol(const t_ctx *c, t_name name, t_sym_kind kind,
                    const t_node *decl, int arity)
{
    t_symbol    sym;

    sym.name = name;
    sym.kind = kind;
    sym.decl = decl;
    sym.offset = rs_name_offset(c, name);
    sym.arity.min = arity;
    sym.arity.max = arity;
    sym.slot = -1;
    sym.module = c->mod->index;
    sym.imported = 0;
    sym.constant = 0;
    sym.depth = c->depth;
    return (sym);
}

int  rs_is_named(t_name name, const char *s)
{
    return (name.len == strlen(s) && memcmp(name.ptr, s, name.len) == 0);
}

int  rs_is_variable(t_sym_kind kind)
{
    return (kind == SYM_GLOBAL || kind == SYM_PARAM || kind == SYM_VAR
        || kind == SYM_VEMP);
}

/* "a function", "an object"... for messages. */
const char   *rs_kind_desc(t_sym_kind kind)
{
    if (kind == SYM_NATIVE)
        return ("a built-in function");
    if (kind == SYM_FUNC)
        return ("a function");
    if (kind == SYM_PROG)
        return ("the prog");
    if (kind == SYM_OBJ)
        return ("an object");
    if (kind == SYM_MODULE)
        return ("a module");
    return ("a variable");
}

void rs_set_ref(t_node *ident, const t_symbol *sym)
{
    t_ref   *ref;

    ref = &ident->as.ident.ref;
    ref->slot = sym->slot;
    ref->module = sym->module;
    ref->decl = sym->decl;
    if (sym->kind == SYM_GLOBAL)
        ref->kind = REF_GLOBAL;
    else if (rs_is_variable(sym->kind))
        ref->kind = REF_LOCAL;
    else if (sym->kind == SYM_NATIVE)
        ref->kind = REF_NATIVE;
    else if (sym->kind == SYM_FUNC)
        ref->kind = REF_FUNC;
    else if (sym->kind == SYM_OBJ)
        ref->kind = REF_OBJ;
    else if (sym->kind == SYM_MODULE)
        ref->kind = REF_MODULE;
    else
        ref->kind = REF_NONE;
}

/* How many arguments a function takes: its defaults and ...rest count. */
t_arity  rs_func_arity(const t_node *func)
{
    t_arity arity;

    arity.min = func->as.func.nrequired;
    arity.max = func->as.func.has_rest ? -1 : func->as.func.nparams;
    return (arity);
}

/* The inner function now keeps this variable in a cell. */
static void set_captured(const t_node *decl)
{
    t_node  *d;

    d = (t_node *)decl;
    if (d->type == NODE_VAR)
        d->as.var.captured = 1;
    else if (d->type == NODE_PARAM)
        d->as.param.captured = 1;
    else if (d->type == NODE_FOR)
        d->as.for_.captured = 1;
    else if (d->type == NODE_FUNC)
        d->as.func.captured = 1;
    else if (d->type == NODE_EXCEPT)
        d->as.except.captured = 1;
}

/*
 * The position of sym in the captures of s's function, added if it is
 * not there yet. The closure takes it from the frame that creates it
 * when sym is declared right there (one level up), otherwise from that
 * frame's own captures, which then need it too.
 */
static int  capture_index(t_ctx *c, t_fscope *s, const t_symbol *sym)
{
    t_capture   *cap;
    t_capture   **tail;
    int         i;
    int         index;
    int         from_local;

    i = 0;
    for (tail = &s->func->as.func.captures; *tail; tail = &(*tail)->next)
    {
        if ((*tail)->decl == sym->decl)
            return (i);
        i++;
    }
    from_local = (sym->depth == s->depth - 1);
    index = from_local ? sym->slot : capture_index(c, s->outer, sym);
    for (tail = &s->func->as.func.captures; *tail; tail = &(*tail)->next)
        ;
    cap = arena_alloc(c->r->arena, sizeof(*cap));
    cap->decl = sym->decl;
    cap->from_local = from_local;
    cap->index = index;
    *tail = cap;
    return (s->func->as.func.ncaptures++);
}

/*
 * Like rs_set_ref, but a local of an enclosing function is a capture:
 * the variable moves to a cell, and every function between it and here
 * passes the cell on.
 */
void rs_ref_name(t_ctx *c, t_node *ident, const t_symbol *sym)
{
    rs_set_ref(ident, sym);
    if (ident->as.ident.ref.kind != REF_LOCAL || sym->depth >= c->depth)
        return ;
    set_captured(sym->decl);
    ident->as.ident.ref.kind = REF_CAPTURE;
    ident->as.ident.ref.slot = capture_index(c, c->scope, sym);
}

/*
 * What a module offers: its own func, obj and var (no prog, no imports);
 * for a module of the language, its natives and constants.
 */
const t_symbol   *rs_exported(const t_module *m, t_name name)
{
    const t_symbol  *sym;

    sym = symtab_find(&m->globals, name);
    if (!sym || sym->imported || (sym->kind != SYM_FUNC
            && sym->kind != SYM_OBJ && sym->kind != SYM_GLOBAL
            && sym->kind != SYM_NATIVE))
        return (NULL);
    return (sym);
}
