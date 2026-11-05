#ifndef RESOLVER_H
# define RESOLVER_H

#include <stddef.h>
#include <sys/types.h>
#include "core/arena.h"
#include "front/ast.h"
#include "core/errors.h"
#include "core/source.h"

/* What a name refers to. */
typedef enum e_sym_kind
{
    SYM_NATIVE,     /* print, range, str, num, tuple, set */
    SYM_FUNC,
    SYM_PROG,       /* reserved name: prog cannot be called */
    SYM_OBJ,
    SYM_GLOBAL,     /* var at top level */
    SYM_MODULE,     /* import m / import m as x */
    SYM_PARAM,
    SYM_VAR,        /* var inside a function: lives until its end */
    SYM_VEMP        /* vemp and for variables: live until their block ends */
}   t_sym_kind;

/* Number of arguments accepted by a call; max -1 = any (print). */
typedef struct s_arity
{
    int min;
    int max;
}   t_arity;

/*
 * decl is NULL for natives. constant marks a const: it cannot be
 * assigned to. depth is how many functions enclose the declaration (0:
 * the top level), so a use from a deeper function is a capture. offset is where the name is declared
 * (global), used by "already declared (line N)" errors. slot and module
 * are copied into every t_ref that uses the name (see ast.h); slot is -1
 * when it has none. For SYM_MODULE, module is the module named.
 * imported marks a name brought in by "from m import x": it refers to
 * m's symbol (same kind, decl, slot and module) and, being m's, a
 * variable imported this way is read-only.
 */
typedef struct s_symbol
{
    t_name          name;
    t_sym_kind      kind;
    const t_node    *decl;
    size_t          offset;
    t_arity         arity;
    int             slot;
    int             module;
    int             imported;
    int             constant;
    int             depth;
}   t_symbol;

/*
 * Growable array of symbols; malloc'd, out of memory is fatal.
 * A hashed table also keeps an open-addressing index over items (each
 * slot holds an item position + 1, 0 = empty; never more than half
 * full), so a lookup does not scan every name. Only the globals are
 * hashed: they can be thousands, while a function has few locals.
 */
typedef struct s_symtab
{
    t_symbol    *items;
    size_t      count;
    size_t      cap;
    int         hashed;
    size_t      *index;
    size_t      index_cap;
}   t_symtab;

/*
 * A loaded file: the main one (index 0; in the REPL it has no file and
 * every line adds to it; it never leaves LOADING and its globals go
 * last) and each imported module, loaded once. A
 * module is LOADING while its own imports are being loaded: meeting a
 * LOADING one again is a circular import. Modules finish in dependency
 * order (rank), the order their globals must be initialised in.
 * Identity is the file (dev, ino), so two paths to one file are one module.
 */
typedef enum e_mod_state
{
    MOD_LOADING,
    MOD_DONE
}   t_mod_state;

typedef struct s_module
{
    const char      *name;      /* "utils", "__main__" for index 0 */
    const t_source  *src;       /* NULL until the main file is known */
    t_node          *root;
    t_symtab        globals;    /* its top-level names, hashed */
    int             nglobals;   /* its global variables: their slots */
    int             index;
    int             rank;       /* order of completion, -1 while loading */
    t_mod_state     state;
    int             has_file;
    dev_t           dev;
    ino_t           ino;
}   t_module;

/*
 * State that outlives a single resolve_program call. In the REPL every
 * line is its own tree, but a func declared on one line must be visible
 * on the next, so the modules (and the main one's names) live here, in
 * the session.
 *
 * Scopes, from outermost to innermost (no name may repeat a visible one,
 * so a lookup only has to find the name, never pick between several):
 *   natives  print, range...: shared by every module.
 *   globals  top-level declarations of the module being resolved.
 *   locals   params and var of the function being resolved: a var lives
 *            until the end of the function even if declared in a block.
 *   temps    vemp and for variables, a stack: each block pops what it
 *            pushed when it closes.
 * locals and temps are emptied after each function; they are kept as
 * members only to reuse their memory.
 *
 * arena and sources are borrowed from the session: modules are read,
 * registered and parsed through them. pending holds a module's text
 * between read_file and its copy into the arena, so fatal_oom's cleanup
 * (resolver_free) frees it too.
 */
typedef struct s_resolver
{
    t_arena         *arena;
    t_sources       *sources;
    t_symtab        natives;
    t_module        **modules;
    int             nmodules;
    int             modules_cap;
    int             ndone;      /* modules finished: the next rank */
    t_symtab        locals;
    t_symtab        temps;
    char            *pending;
}   t_resolver;

/* repl: the session is the REPL, which has natives of its own (exit). */
void    resolver_init(t_resolver *r, t_arena *arena, t_sources *sources,
            int repl);

/*
 * Checks the rules the grammar does not express (see docs/EBNF.md) on a tree
 * built by parse_program from src, the main module, loading and checking
 * every module it imports. Returns 0, or fills err and returns -1; on
 * error nothing the tree declared or loaded stays registered, so a
 * failed REPL line leaves the session as it was.
 */
int     resolve_program(t_resolver *r, const t_source *src, t_node *root,
            t_error *err);

void    resolver_free(t_resolver *r);

#endif
