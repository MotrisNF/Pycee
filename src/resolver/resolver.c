/* Entry points of the resolver (see resolver.h). */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "resolver/natives.h"
#include "resolver/resolver_internal.h"

/*
 * Natives are shared by every module; module 0 is the main one. A
 * REPL-only native is left out of a file's session, so there its name
 * is free; its slot is still its place in the list.
 */
void    resolver_init(t_resolver *r, t_arena *arena, t_sources *sources,
            int repl)
{
    t_symbol    sym;
    int         i;

    r->arena = arena;
    r->sources = sources;
    symtab_init(&r->natives, 0);
    r->modules = NULL;
    r->nmodules = 0;
    r->modules_cap = 0;
    r->ndone = 0;
    symtab_init(&r->locals, 0);
    symtab_init(&r->temps, 0);
    r->pending = NULL;
    for (i = 0; i < g_native_global_count; i++)
    {
        if (g_native_defs[i].repl && !repl)
            continue ;
        sym.name.ptr = g_native_defs[i].name;
        sym.name.len = strlen(g_native_defs[i].name);
        sym.kind = SYM_NATIVE;
        sym.decl = NULL;
        sym.offset = 0;
        sym.arity.min = g_native_defs[i].min;
        sym.arity.max = g_native_defs[i].max;
        sym.slot = i;
        sym.module = -1;
        sym.imported = 0;
        sym.constant = 0;
        sym.depth = 0;
        symtab_push(&r->natives, &sym);
    }
    rs_module_new(r, "__main__");
}

/*
 * The main file is identified (and named) the first time, so a module
 * that imports it back is a circular import. A REPL line has no file.
 */
static void identify_main(t_module *top, const t_source *src)
{
    struct stat st;

    if (top->has_file || src->path[0] == '<' || stat(src->path, &st) == -1)
        return ;
    top->has_file = 1;
    top->dev = st.st_dev;
    top->ino = st.st_ino;
    top->name = src->path;
}

/*
 * The main module never becomes DONE: later REPL lines keep adding to
 * it, and its globals are initialised after every imported module's.
 */
int resolve_program(t_resolver *r, const t_source *src, t_node *root,
        t_error *err)
{
    t_module    *top;
    size_t      saved;
    int         saved_nglobals;
    int         saved_nmodules;
    int         saved_ndone;

    top = r->modules[0];
    identify_main(top, src);
    top->src = src;
    top->root = root;
    saved = top->globals.count;
    saved_nglobals = top->nglobals;
    saved_nmodules = r->nmodules;
    saved_ndone = r->ndone;
    if (rs_resolve_module(r, top, err) == -1)
    {
        symtab_truncate(&top->globals, saved);
        top->nglobals = saved_nglobals;
        rs_modules_truncate(r, saved_nmodules);
        r->ndone = saved_ndone;
        r->locals.count = 0;
        r->temps.count = 0;
        return (-1);
    }
    return (0);
}

void    resolver_free(t_resolver *r)
{
    rs_modules_truncate(r, 0);
    free(r->modules);
    r->modules = NULL;
    r->modules_cap = 0;
    symtab_free(&r->natives);
    symtab_free(&r->locals);
    symtab_free(&r->temps);
    free(r->pending);
    r->pending = NULL;
}
