/* Modules: finding, loading and resolving imported files. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "core/reader.h"
#include "front/parser.h"
#include "resolver/natives.h"
#include "resolver/resolver_internal.h"

/* Grows the table before allocating, so no module is ever unreachable. */
t_module *rs_module_new(t_resolver *r, const char *name)
{
    t_module    **grown;
    t_module    *m;
    int         cap;

    if (r->nmodules == r->modules_cap)
    {
        cap = r->modules_cap ? r->modules_cap * 2 : 8;
        grown = realloc(r->modules, cap * sizeof(*r->modules));
        if (!grown)
            fatal_oom();
        r->modules = grown;
        r->modules_cap = cap;
    }
    m = malloc(sizeof(*m));
    if (!m)
        fatal_oom();
    m->name = name;
    m->src = NULL;
    m->root = NULL;
    symtab_init(&m->globals, 1);
    m->nglobals = 0;
    m->index = r->nmodules;
    m->rank = -1;
    m->state = MOD_LOADING;
    m->has_file = 0;
    m->native = -1;
    r->modules[r->nmodules++] = m;
    return (m);
}

static void module_free(t_module *m)
{
    symtab_free(&m->globals);
    free(m);
}

/* Drops the modules from index count on (a failed REPL line). */
void rs_modules_truncate(t_resolver *r, int count)
{
    while (r->nmodules > count)
        module_free(r->modules[--r->nmodules]);
}

/*
 * "dir/name.pce", dir being the importer's directory; "name.pce" (the
 * current directory) when it has none or is the REPL. Returns 0 if it
 * does not fit in buf.
 */
static int  module_path(const t_ctx *c, t_name name, char *buf, size_t size)
{
    const char  *path;
    const char  *slash;
    int         n;

    path = c->src->path;
    slash = strrchr(path, '/');
    if (path[0] == '<' || !slash)
        n = snprintf(buf, size, "%.*s.pce", (int)name.len, name.ptr);
    else
        n = snprintf(buf, size, "%.*s/%.*s.pce", (int)(slash - path), path,
                (int)name.len, name.ptr);
    return (n >= 0 && (size_t)n < size);
}

/*
 * The modules still LOADING are the chain of imports that led here, in
 * order: "main.pce -> a -> b -> a".
 */
static int  error_circular(t_ctx *c, t_name name, size_t offset)
{
    char    chain[ERR_MSG_SIZE];
    size_t  len;
    int     i;
    int     n;

    len = 0;
    chain[0] = '\0';
    for (i = 0; i < c->r->nmodules && len < sizeof(chain); i++)
    {
        if (c->r->modules[i]->state != MOD_LOADING)
            continue ;
        n = snprintf(chain + len, sizeof(chain) - len, "%s -> ",
                c->r->modules[i]->name);
        if (n < 0)
            break ;
        len += (size_t)n;
    }
    return (error_set(c->err, ERR_IMPORT, offset, name.len,
            "circular import: %s%.*s", chain, (int)name.len, name.ptr));
}

/*
 * Resolves a module's tree with a context of its own. Its imports are
 * loaded during pass 1, before any of its bodies, when no function is
 * being resolved: locals and temps are free, and the importer's pass 1
 * (which called this) does not use them either.
 */
int  rs_resolve_module(t_resolver *r, t_module *m, t_error *err)
{
    t_ctx   c;

    c.r = r;
    c.mod = m;
    c.src = m->src;
    c.err = err;
    c.func = NULL;
    c.obj = NULL;
    c.loop_depth = 0;
    c.nslots = 0;
    c.global_init = 0;
    c.global_offset = 0;
    c.scope = NULL;
    c.depth = 0;
    c.finally_depth = 0;
    c.finally_loops = 0;
    if (rs_declare_globals(&c, m->root) == -1 || rs_layout_objects(&c, m->root)
        == -1 || rs_resolve_bodies(&c, m->root) == -1)
        return (-1);
    return (0);
}

/* A native of module mod, or one of its constants, as a global of m. */
static void add_native_symbol(t_module *m, const char *name, t_sym_kind kind,
                int slot, const t_native_def *def)
{
    t_symbol    sym;

    sym.name.ptr = name;
    sym.name.len = strlen(name);
    sym.kind = kind;
    sym.decl = NULL;
    sym.offset = 0;
    sym.arity.min = def ? def->min : 0;
    sym.arity.max = def ? def->max : 0;
    sym.slot = slot;
    sym.module = m->index;
    sym.imported = 0;
    sym.constant = (kind == SYM_GLOBAL);
    sym.depth = 0;
    symtab_push(&m->globals, &sym);
}

/*
 * A module of the language: loaded once, already DONE (it imports
 * nothing). A user's file with its name next to the importer would be
 * silently hidden, so it is an error: the name is taken.
 */
static int  load_native(t_ctx *c, t_name name, t_native_module which,
                t_module **out)
{
    char            path[PATH_MAX];
    struct stat     st;
    t_resolver      *r;
    t_module        *m;
    int             i;

    r = c->r;
    if (module_path(c, name, path, sizeof(path)) && stat(path, &st) == 0
        && S_ISREG(st.st_mode))
        return (error_set(c->err, ERR_IMPORT, rs_name_offset(c, name),
                name.len, "'%.*s' is a module of the language: rename %s",
                (int)name.len, name.ptr, path));
    for (i = 0; i < r->nmodules; i++)
    {
        if (r->modules[i]->native == (int)which)
        {
            *out = r->modules[i];
            return (0);
        }
    }
    m = rs_module_new(r, g_native_module_names[which]);
    m->native = which;
    for (i = 0; i < g_native_count; i++)
        if (g_native_defs[i].module == which)
            add_native_symbol(m, g_native_defs[i].name, SYM_NATIVE, i,
                &g_native_defs[i]);
    for (i = 0; i < g_native_const_count; i++)
        if (g_native_consts[i].module == which)
            add_native_symbol(m, g_native_consts[i].name, SYM_GLOBAL,
                m->nglobals++, NULL);
    m->state = MOD_DONE;
    m->rank = r->ndone++;
    *out = m;
    return (0);
}

/*
 * Finds or loads the module "name" imported from c's source. A module is
 * loaded once (identity is the file, not the path); meeting one that is
 * still LOADING is a circular import.
 */
static int  load_module(t_ctx *c, t_name name, t_module **out)
{
    char            path[PATH_MAX];
    struct stat     st;
    t_resolver      *r;
    t_module        *m;
    const t_source  *src;
    t_node          *root;
    size_t          len;
    size_t          offset;
    int             i;

    if (native_module_named(name.ptr, name.len) != NMOD_NONE)
        return (load_native(c, name, native_module_named(name.ptr, name.len),
                out));
    r = c->r;
    offset = rs_name_offset(c, name);
    if (!module_path(c, name, path, sizeof(path)))
        return (error_set(c->err, ERR_IMPORT, offset, name.len,
                "the path of module '%.*s' is too long", (int)name.len,
                name.ptr));
    if (stat(path, &st) == -1 || !S_ISREG(st.st_mode))
        return (error_set(c->err, ERR_IMPORT, offset, name.len,
                "no module named '%.*s': %s does not exist", (int)name.len,
                name.ptr, path));
    for (i = 0; i < r->nmodules; i++)
    {
        m = r->modules[i];
        if (!m->has_file || m->dev != st.st_dev || m->ino != st.st_ino)
            continue ;
        if (m->state == MOD_LOADING)
            return (error_circular(c, name, offset));
        *out = m;
        return (0);
    }
    r->pending = read_file(path, &len);
    if (!r->pending)
        return (error_set(c->err, ERR_IMPORT, offset, name.len,
                "cannot read module '%.*s' (%s)", (int)name.len, name.ptr,
                path));
    src = sources_add(r->sources, r->arena, path, r->pending, len);
    free(r->pending);
    r->pending = NULL;
    root = parse_program(r->arena, src, c->err);
    if (!root)
        return (-1);
    m = rs_module_new(r, arena_strndup(r->arena, name.ptr, name.len));
    m->src = src;
    m->root = root;
    m->has_file = 1;
    m->dev = st.st_dev;
    m->ino = st.st_ino;
    if (rs_resolve_module(r, m, c->err) == -1)
        return (-1);
    m->state = MOD_DONE;
    m->rank = r->ndone++;
    *out = m;
    return (0);
}

/* The REPL has an exit() of its own (a REPL-only native). */
static int  repl_has_exit(const t_ctx *c)
{
    t_name  name;

    name.ptr = "exit";
    name.len = 4;
    return (symtab_find(&c->r->natives, name) != NULL);
}

/*
 * import m / import m as x: one name for the module (it is not a value:
 * only m.name is). from m import a, b as c: each name (or its alias)
 * refers to m's own declaration, same kind and slot (a variable stays
 * read-only).
 */
int  rs_resolve_import(t_ctx *c, t_node *n)
{
    t_module        *m;
    t_symbol        sym;
    const t_symbol  *target;
    t_node          *id;
    t_name          name;

    if (load_module(c, n->as.import.module, &m) == -1)
        return (-1);
    if (!n->as.import.is_from)
    {
        name = n->as.import.alias.ptr ? n->as.import.alias
            : n->as.import.module;
        sym = rs_make_symbol(c, name, SYM_MODULE, n, 0);
        sym.module = m->index;
        return (rs_declare(c, &c->mod->globals, &sym));
    }
    for (id = n->as.import.names; id; id = id->next)
    {
        name = id->as.ident.name;
        if (m->native == NMOD_sys && rs_is_named(name, "exit")
            && repl_has_exit(c))
            return (error_set(c->err, ERR_IMPORT, id->offset, name.len,
                    "cannot import 'exit' from 'sys' in the REPL: exit() is"
                    " already there"));
        target = rs_exported(m, name);
        if (!target)
            return (error_set(c->err, ERR_IMPORT, id->offset, name.len,
                    "cannot import name '%.*s' from '%s'", (int)name.len,
                    name.ptr, m->name));
        if (id->as.ident.alias.ptr)
            name = id->as.ident.alias;
        sym = *target;
        sym.name = name;
        sym.offset = rs_name_offset(c, name);
        sym.imported = 1;
        if (rs_declare(c, &c->mod->globals, &sym) == -1)
            return (-1);
        rs_set_ref(id, &sym);
    }
    return (0);
}
