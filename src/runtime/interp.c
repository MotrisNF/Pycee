/* Interpreter state: value stack, module globals and the top level. */

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "man/man.h"
#include "runtime/interp_internal.h"

/*
 * The roots of a collection: every value on the stack (frames, values in
 * flight), every global of every module, and the value a return is
 * carrying. Everything else alive hangs from these.
 */
static void mark_roots(t_heap *h, void *ctx)
{
    const t_interp  *in;
    size_t          i;
    int             m;
    int             g;

    in = ctx;
    for (i = 0; i < in->top; i++)
        heap_mark_value(h, in->stack[i]);
    for (m = 0; m < in->nmodules; m++)
        for (g = 0; g < in->modules[m].count; g++)
            heap_mark_value(h, in->modules[m].values[g]);
    heap_mark_value(h, in->ret);
    for (g = 0; g < RT_NSIG; g++)
        if (in->sig_handled[g])
            heap_mark_value(h, in->sig_handlers[g]);
}

void    interp_init(t_interp *in, t_resolver *r)
{
    int i;

    in->resolver = r;
    heap_init(&in->heap);
    heap_set_roots(&in->heap, mark_roots, in);
    in->stack = NULL;
    in->top = 0;
    in->cap = 0;
    in->frame = 0;
    in->cells = NULL;
    in->module = 0;
    in->jump_levels = 0;
    in->ret = val_none();
    in->ret_node = NULL;
    in->depth = 0;
    in->max_depth = 1000;
    in->func_name.ptr = "<module>";
    in->func_name.len = 8;
    in->argc = 0;
    in->argv = NULL;
    in->line = NULL;
    in->line_cap = 0;
    buf_init(&in->scratch);
    buf_init(&in->piece);
    in->pool = NULL;
    in->pool_len = 0;
    in->pool_cap = 0;
    in->pool_used = 0;
    in->modules = NULL;
    in->nmodules = 0;
    in->modules_cap = 0;
    buf_init(&in->text);
    in->rng_seeded = 0;
    in->dir = NULL;
    for (i = 0; i < RT_NSIG; i++)
    {
        in->sig_handlers[i] = val_none();
        in->sig_handled[i] = 0;
    }
}

void    interp_set_args(t_interp *in, int argc, char **argv)
{
    in->argc = argc;
    in->argv = argv;
}

void    interp_free(t_interp *in)
{
    int     i;
    int     argc;
    char    **argv;

    if (in->dir)
        closedir(in->dir);
    for (i = 0; i < in->nmodules; i++)
        free(in->modules[i].values);
    free(in->modules);
    free(in->stack);
    buf_free(&in->text);
    buf_free(&in->scratch);
    buf_free(&in->piece);
    for (i = 0; i < (int)in->pool_len; i++)
    {
        buf_free(in->pool[i]);
        free(in->pool[i]);
    }
    free(in->pool);
    free(in->line);
    heap_free(&in->heap);
    argc = in->argc;
    argv = in->argv;
    interp_init(in, in->resolver);
    interp_set_args(in, argc, argv);
}

/* ---------- buffers for showing values ---------- */

/*
 * A cleared buffer of its own for the caller, until rt_buf_give. Each
 * one is allocated apart (the array of them may move, they do not) and
 * counted before it is used, so the cleanup always finds every one.
 */
t_buffer    *rt_buf_take(t_interp *in)
{
    t_buffer    **grown;
    t_buffer    *b;
    size_t      cap;

    if (in->pool_used == in->pool_len)
    {
        if (in->pool_len == in->pool_cap)
        {
            cap = in->pool_cap ? in->pool_cap * 2 : 4;
            grown = realloc(in->pool, cap * sizeof(*in->pool));
            if (!grown)
                fatal_oom();
            in->pool = grown;
            in->pool_cap = cap;
        }
        b = malloc(sizeof(t_buffer));
        if (!b)
            fatal_oom();
        buf_init(b);
        in->pool[in->pool_len++] = b;
    }
    b = in->pool[in->pool_used++];
    buf_clear(b);
    return (b);
}

void    rt_buf_give(t_interp *in)
{
    in->pool_used--;
}

/* ---------- value stack ---------- */

/* cap only grows once realloc has succeeded (fatal_oom's cleanup). */
void    rt_push(t_interp *in, t_value v)
{
    t_value *grown;
    size_t  cap;

    if (in->top == in->cap)
    {
        cap = in->cap ? in->cap * 2 : 256;
        grown = realloc(in->stack, cap * sizeof(*in->stack));
        if (!grown)
            fatal_oom();
        in->stack = grown;
        in->cap = cap;
    }
    in->stack[in->top++] = v;
}

t_value rt_pop(t_interp *in)
{
    return (in->stack[--in->top]);
}

/* distance 0 is the top. */
t_value rt_peek(const t_interp *in, size_t distance)
{
    return (in->stack[in->top - 1 - distance]);
}

int rt_error(t_error *err, t_error_type type, const t_node *at,
        const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_vset(err, type, at->offset, at->len, fmt, ap);
    va_end(ap);
    return (-1);
}

/* ---------- globals ---------- */

/*
 * One t_globals per module of the resolver, each as long as its number of
 * global variables (new ones start as none). Called after every
 * successful resolve: a REPL line may have added modules and globals.
 */
static void sync_globals(t_interp *in)
{
    const t_resolver    *r;
    t_globals           *g;
    t_value             *values;
    int                 i;
    int                 need;

    r = in->resolver;
    if (r->nmodules > in->modules_cap)
    {
        g = realloc(in->modules, (size_t)r->modules_cap * sizeof(*g));
        if (!g)
            fatal_oom();
        in->modules = g;
        in->modules_cap = r->modules_cap;
    }
    for (; in->nmodules < r->nmodules; in->nmodules++)
    {
        g = &in->modules[in->nmodules];
        g->values = NULL;
        g->count = 0;
        g->cap = 0;
        g->initialized = 0;
    }
    for (i = 0; i < in->nmodules; i++)
    {
        g = &in->modules[i];
        need = r->modules[i]->nglobals;
        if (need > g->cap)
        {
            values = realloc(g->values, (size_t)need * 2 * sizeof(*values));
            if (!values)
                fatal_oom();
            g->values = values;
            g->cap = need * 2;
        }
        while (g->count < need)
            g->values[g->count++] = val_none();
    }
}

t_value *rt_global(t_interp *in, const t_ref *ref)
{
    return (&in->modules[ref->module].values[ref->slot]);
}

/*
 * A global, a local of the running frame or a capture. A local that an
 * inner function captures holds its cell: the variable is in there.
 */
t_value *rt_variable(t_interp *in, const t_ref *ref)
{
    t_value *v;

    if (ref->kind == REF_GLOBAL)
        return (rt_global(in, ref));
    if (ref->kind == REF_CAPTURE)
        return (&in->cells[ref->slot]->value);
    v = &in->stack[in->frame + ref->slot];
    if (val_is_obj(*v, OBJ_CELL))
        return (&((t_cell *)v->as.obj)->value);
    return (v);
}

/*
 * Moves the value of stack[index] into a new cell, which takes its place.
 * The value is rooted where it is while the cell is allocated.
 */
t_cell  *rt_box(t_interp *in, size_t index)
{
    t_cell  *cell;

    cell = (t_cell *)heap_alloc(&in->heap, OBJ_CELL, sizeof(t_cell));
    cell->value = in->stack[index];
    in->stack[index] = val_obj(&cell->obj);
    return (cell);
}

/* ---------- top level ---------- */

/* The REPL shows what an expression gives, except none. */
static int  run_expr_stmt(t_interp *in, const t_node *n, int echo,
                t_error *err)
{
    t_buffer    *b;
    t_value     v;
    int         status;

    if (rt_eval(in, n->as.expr_stmt.expr, &v, err) == -1)
        return (-1);
    if (!echo || v.type == VAL_NONE)
        return (0);
    rt_push(in, v);
    b = rt_buf_take(in);
    status = rt_show(in, n, b, v, 1, err);
    buf_add_char(b, '\n');
    if (status == 0)
        fwrite(b->data, 1, b->len, stdout);
    rt_buf_give(in);
    rt_pop(in);
    return (status);
}

/*
 * man(topic) at the REPL's prompt: the page, through the pager. Every
 * buffer is the session's (taken from the pool), so nothing leaks if
 * memory runs out. A topic with no page is not an error: the message
 * says what there is instead. Ctrl+C only leaves the pager.
 */
static int  run_man(t_interp *in, const t_node *n)
{
    t_buffer    *page;
    t_buffer    *title;
    t_buffer    *work;
    t_buffer    *lines;

    page = rt_buf_take(in);
    title = rt_buf_take(in);
    work = rt_buf_take(in);
    lines = rt_buf_take(in);
    if (man_lookup(n->as.man.topic.ptr, n->as.man.topic.len, page, title,
            work))
    {
        if (man_show(title->data, page->data, page->len, lines))
            interp_signals_clear();
    }
    else
    {
        fflush(stdout);
        fwrite(page->data, 1, page->len, stderr);
    }
    rt_buf_give(in);
    rt_buf_give(in);
    rt_buf_give(in);
    rt_buf_give(in);
    return (0);
}

/*
 * One item of a module's top level: the static vars of an obj are
 * globals too, given their values in its place. Only a REPL line holds
 * statements there; break, continue and return cannot reach it.
 */
static int  run_top(t_interp *in, t_node *n, int echo, t_error *err)
{
    const t_node    *m;

    if (n->type == NODE_OBJ)
    {
        for (m = n->as.obj.members; m; m = m->next)
            if (m->type == NODE_VAR && m->as.var.is_static
                && rt_run_var(in, m, err) == -1)
                return (-1);
        return (0);
    }
    if (n->type == NODE_FUNC || n->type == NODE_IMPORT)
        return (0);
    if (n->type == NODE_MAN)
        return (run_man(in, n));
    if (n->type == NODE_EXPR_STMT)
        return (run_expr_stmt(in, n, echo, err));
    return (rt_exec(in, n, err) == EXEC_ERROR ? -1 : 0);
}

/*
 * A module's top level runs in a frame of its own: the slots of the for
 * variables of a REPL line's statements (none for a file).
 */
static int  run_items(t_interp *in, int module, t_node *root, int echo,
                t_error *err)
{
    t_node  *n;
    int     i;

    in->module = module;
    in->frame = in->top;
    for (i = 0; i < root->as.list.nslots; i++)
        rt_push(in, val_none());
    for (n = root->as.list.items; n; n = n->next)
        if (run_top(in, n, echo, err) == -1)
            return (-1);
    return (0);
}

/*
 * Imported modules are initialised once, in the order they finished
 * loading (a module after everything it imports); a module of the
 * language only has its constants to set. One that fails stays
 * marked initialised, with the globals it did not reach still none.
 */
static int  init_modules(t_interp *in, t_error *err)
{
    const t_resolver    *r;
    size_t              top;
    int                 rank;
    int                 i;

    r = in->resolver;
    top = in->top;
    for (rank = 0; rank < r->ndone; rank++)
    {
        for (i = 1; i < r->nmodules; i++)
        {
            if (r->modules[i]->rank != rank || in->modules[i].initialized)
                continue ;
            in->modules[i].initialized = 1;
            if (r->modules[i]->native != -1)
                rt_init_native_module(in, i);
            else if (run_items(in, i, r->modules[i]->root, 0, err) == -1)
                return (-1);
            in->top = top;
        }
    }
    return (0);
}

/*
 * prog main(argc, argv): argc is how many arguments the program got and
 * argv a tuple of them as strings, argv[0] being its path. The tuple
 * stays in its slot (rooted) while each string is made.
 */
static void set_prog_args(t_interp *in, const t_node *prog)
{
    t_tuple *argv;
    t_string*s;
    int     i;

    in->stack[in->frame] = val_int(in->argc);
    argv = heap_tuple(&in->heap, (size_t)in->argc);
    in->stack[in->frame + 1] = val_obj(&argv->obj);
    for (i = 0; i < in->argc; i++)
    {
        s = rt_new_string(in, in->argv[i], strlen(in->argv[i]));
        argv->items[i] = val_obj(&s->obj);
    }
    i = 0;
    for (prog = prog->as.func.params; prog; prog = prog->next, i++)
        if (prog->as.param.captured)
            rt_box(in, in->frame + (size_t)i);
}

/*
 * What return gave back is the exit code, with the rules of exit():
 * none (or no return) is 0, otherwise an integer from 0 to 255. A code
 * other than 0 leaves as a SystemExit.
 */
static int  prog_exit_code(t_interp *in, t_error *err)
{
    const t_node    *at;
    t_value         v;

    v = in->ret;
    in->ret = val_none();
    if (v.type == VAL_NONE)
        return (0);
    at = in->ret_node->as.ret.value;
    if (v.type == VAL_FLOAT)
        return (rt_error(err, ERR_TYPE, at, "the exit code returned by prog"
                " must be an integer, not a float"));
    if (v.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, at, "the exit code returned by prog"
                " must be an integer, not '%s'", val_type_name(v)));
    if (v.as.integer < 0 || v.as.integer > 255)
        return (rt_error(err, ERR_VALUE, at, "exit code must be between 0"
                " and 255, not %lld", (long long)v.as.integer));
    if (v.as.integer == 0)
        return (0);
    rt_error(err, ERR_EXIT, at, "exit");
    err->code = (int)v.as.integer;
    return (-1);
}

/*
 * The prog of the main file, in a frame of its own (every slot starts
 * as none). A traceback from it ends in its name.
 */
static int  run_prog(t_interp *in, const t_node *root, t_error *err)
{
    const t_node    *n;
    t_exec          status;
    int             i;

    for (n = root->as.list.items; n; n = n->next)
        if (n->type == NODE_FUNC && n->as.func.is_prog)
            break ;
    if (!n)
        return (0);
    in->module = 0;
    in->frame = in->top;
    in->cells = NULL;
    in->func_name = n->as.func.name;
    for (i = 0; i < n->as.func.nslots; i++)
        rt_push(in, val_none());
    if (n->as.func.params)
        set_prog_args(in, n);
    status = rt_exec_block(in, n->as.func.body, err);
    in->func_name.ptr = "<module>";
    in->func_name.len = 8;
    if (status == EXEC_ERROR)
    {
        err->top_name = n->as.func.name.ptr;
        err->top_len = n->as.func.name.len;
        return (-1);
    }
    if (status == EXEC_RETURN)
        return (prog_exit_code(in, err));
    return (0);
}

int interp_run(t_interp *in, t_node *root, int echo, t_error *err)
{
    size_t  top;
    int     status;

    top = in->top;
    in->pool_used = 0;
    sync_globals(in);
    status = init_modules(in, err);
    if (status == 0)
        status = run_items(in, 0, root, echo, err);
    in->top = top;
    if (status == 0 && !echo)
        status = run_prog(in, root, err);
    in->top = top;
    in->frame = top;
    return (status);
}
