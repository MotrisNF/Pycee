#ifndef INTERP_H
# define INTERP_H

#include <stddef.h>
#include "core/buffer.h"
#include "core/errors.h"
#include "front/ast.h"
#include "resolver/resolver.h"
#include "runtime/heap.h"
#include "runtime/value.h"

/* The global variables of one module, indexed by their slot. */
typedef struct s_globals
{
    t_value *values;
    int     count;
    int     cap;
    int     initialized;
}   t_globals;

/*
 * The interpreter walks trees the resolver has checked and annotated
 * (slots, refs): it never looks a name up.
 *
 * stack is the value stack, and with the globals it is the set of roots
 * the collector (phase 8) will mark. Rule: a t_value that has to survive
 * another evaluation or an allocation is pushed there, never kept only
 * in a C local, so nothing in use can ever be collected. It is reached by
 * index, since pushing may move it.
 *
 * frame is where the slots of the running frame start in stack (a
 * call, the prog's body, or a REPL line's top level): local slot i is
 * stack[frame + i]. cells are the captures of the running closure (NULL
 * outside one; the closure itself is on the stack, so they stay alive).
 * module is the module whose top level is running. jump_levels is how
 * many loops a break or continue still has to leave. ret is the value a
 * return gives back (a root too) and ret_node that return. depth counts
 * the calls in progress, up to max_depth (RecursionError); func_name
 * is the running function's ("<module>" at the top level). argv: the
 * program's arguments, for prog main(argc, argv). line: what input()
 * read last (getline's buffer). scratch is a second buffer, for work that
 * needs one while text is in use; piece is where an f-string field is
 * formatted (text and scratch are busy then). pool holds buffers for
 * showing values (rt_buf_take / rt_buf_give, one per level), because
 * showing an object runs its to_str, which may show things too.
 *
 * modules[i] holds the globals of the resolver's module i. text is a
 * scratch buffer for turning values into text (str, print, the REPL echo).
 * Everything here is freed by interp_free, which the session (and
 * fatal_oom's cleanup) always calls.
 */
typedef struct s_interp
{
    t_resolver  *resolver;
    t_heap      heap;
    t_value     *stack;
    size_t      top;
    size_t      cap;
    size_t      frame;
    t_cell      **cells;
    int         module;
    int         jump_levels;
    t_value     ret;
    const t_node *ret_node;
    int         depth;
    int         max_depth;
    t_name      func_name;
    int         argc;
    char        **argv;
    char        *line;
    size_t      line_cap;
    t_buffer    scratch;
    t_buffer    piece;
    t_buffer    **pool;
    size_t      pool_len;
    size_t      pool_cap;
    size_t      pool_used;
    t_globals   *modules;
    int         nmodules;
    int         modules_cap;
    t_buffer    text;
}   t_interp;

void    interp_init(t_interp *in, t_resolver *r);

/* The arguments of the program (argv[0] its path), for prog main(argc, argv). */
void    interp_set_args(t_interp *in, int argc, char **argv);

/*
 * Runs a tree that resolve_program accepted: first initialises the
 * globals of every module it loaded (in dependency order), then runs the
 * top level of root: global var values and, in the REPL, statements;
 * then, for a file, its prog. echo (the REPL) shows the value of each
 * expression statement at the top level. Returns 0, or fills err and
 * returns -1 (also for exit(): ERR_EXIT).
 */
int     interp_run(t_interp *in, t_node *root, int echo, t_error *err);

void    interp_free(t_interp *in);

#endif
