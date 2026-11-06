#ifndef RESOLVER_INTERNAL_H
# define RESOLVER_INTERNAL_H

/*
 * Shared by the files of src/resolver/ only. Everything here is internal
 * to the resolver: the rest of the program uses resolver.h.
 * Functions shared between those files are prefixed rs_ (symtab_ for
 * the symbol tables); the rest stay static in their file.
 */

#include <stddef.h>
#include "resolver/resolver.h"

/*
 * What one resolve_program call works on (mod is the module whose tree
 * is being resolved, src its source), and where the walk is: func is
 * the FUNC whose body is being resolved (NULL outside any), obj the OBJ
 * it belongs to (NULL for plain functions), loop_depth how many loops
 * enclose the current statement inside func (or, in the REPL, at the
 * top level, whose for variables also take frame slots: nslots).
 */
/*
 * The functions being resolved, innermost first (each one lives on the C
 * stack of its resolve_function call): depth counts them, so the top
 * level is 0.
 */
typedef struct s_fscope
{
    t_node              *func;
    int                 depth;
    struct s_fscope     *outer;
    const t_name        *names;     /* what func declares: see statements.c */
    size_t              names_cap;
}   t_fscope;

typedef struct s_ctx
{
    t_resolver      *r;
    t_module        *mod;
    const t_source  *src;
    t_error         *err;
    const t_node    *func;
    const t_node    *obj;
    int             loop_depth;
    int             nslots;         /* frame slots used so far in func */
    int             global_init;    /* resolving a global var's value */
    size_t          global_offset;  /* ... and where that var is declared */
    t_fscope        *scope;         /* NULL at the top level */
    int             depth;
    int             finally_depth;  /* finally blocks around, in func */
    int             finally_loops;  /* loop_depth at the innermost one */
}   t_ctx;

/* symtab.c: symbol tables */
void            symtab_init(t_symtab *t, int hashed);
void            symtab_push(t_symtab *t, const t_symbol *sym);
void            symtab_truncate(t_symtab *t, size_t count);
void            symtab_free(t_symtab *t);
t_symbol        *symtab_find(const t_symtab *t, t_name name);
t_symbol        *symtab_find_last(const t_symtab *t, t_name name);
int             rs_name_eq(t_name a, t_name b);
size_t          rs_name_hash(t_name name);

/* scope.c: names, declarations and references */
t_symbol        *rs_lookup(const t_ctx *c, t_name name);
int             rs_declare(t_ctx *c, t_symtab *t, const t_symbol *sym);
t_symbol        rs_make_symbol(const t_ctx *c, t_name name, t_sym_kind kind,
                    const t_node *decl, int arity);
void            rs_set_ref(t_node *ident, const t_symbol *sym);
void            rs_ref_name(t_ctx *c, t_node *ident, const t_symbol *sym);
t_arity         rs_func_arity(const t_node *func);
const t_symbol  *rs_exported(const t_module *m, t_name name);
size_t          rs_name_offset(const t_ctx *c, t_name name);
void            rs_where_str(const t_ctx *c, size_t offset, char *buf,
                    size_t size);
int             rs_is_variable(t_sym_kind kind);
const char      *rs_kind_desc(t_sym_kind kind);
int             rs_is_named(t_name name, const char *s);
int             rs_count_list(const t_node *n);

/* declarations.c: pass 1 */
int             rs_declare_globals(t_ctx *c, t_node *root);
int             rs_declare_global_var(t_ctx *c, t_node *n);
int             rs_layout_objects(t_ctx *c, t_node *root);
const t_node    *rs_find_member(const t_node *obj, t_name name);

/* objects.c */
const t_node    *rs_obj_find(const t_node *obj, t_name name);
const t_node    *rs_member_cls(const t_node *m);
int             rs_obj_layout(t_ctx *c, t_node *obj);
t_arity         rs_obj_arity(const t_node *obj);
int             rs_obj_member(t_ctx *c, const t_node *cls, t_name name,
                    size_t offset, const t_node **m);
int             rs_obj_static(t_ctx *c, t_node *field, const t_node *cls,
                    const t_node **m);

/* expressions.c */
int             rs_resolve_expr(t_ctx *c, t_node *n);
int             rs_resolve_target(t_ctx *c, t_node *n, const char *action);

/* statements.c: pass 2 */
int             rs_resolve_bodies(t_ctx *c, t_node *root);
int             rs_resolve_function(t_ctx *c, t_node *f, const t_node *obj);
int             rs_resolve_comprehension(t_ctx *c, t_node *n);
const t_name    *rs_scope_declares(const t_fscope *s, t_name name);

/* modules.c */
t_module        *rs_module_new(t_resolver *r, const char *name);
void            rs_modules_truncate(t_resolver *r, int count);
int             rs_resolve_import(t_ctx *c, t_node *n);
int             rs_resolve_module(t_resolver *r, t_module *m, t_error *err);

#endif
