/* Module sys: the interpreter and the program: arguments, exit, the collector. */

#include <ctype.h>
#include <string.h>
#include <sys/utsname.h>
#include "natives/modules.h"

/* The phase of the roadmap as the minor version, until the 1.0. */
#define PYCEE_VERSION   "0.10.0"

/* The program's arguments as strings, argv[0] its path (as prog's argv). */
t_value nconst_sys_argv(t_interp *in)
{
    t_tuple     *argv;
    t_string    *s;
    size_t      slot;
    int         i;

    argv = heap_tuple(&in->heap, (size_t)in->argc);
    slot = in->top;
    rt_push(in, val_obj(&argv->obj));
    for (i = 0; i < in->argc; i++)
    {
        s = rt_new_string(in, in->argv[i], strlen(in->argv[i]));
        argv->items[i] = val_obj(&s->obj);
    }
    in->top = slot;
    return (val_obj(&argv->obj));
}

/* The system's name in lower case, as uname gives it: "linux". */
t_value nconst_sys_platform(t_interp *in)
{
    struct utsname  u;
    t_string        *s;
    size_t          i;

    if (uname(&u) == -1)
        strcpy(u.sysname, "unknown");
    s = rt_new_string(in, u.sysname, strlen(u.sysname));
    for (i = 0; i < s->len; i++)
        s->chars[i] = (char)tolower((unsigned char)s->chars[i]);
    return (val_obj(&s->obj));
}

t_value nconst_sys_version(t_interp *in)
{
    return (val_obj(&rt_new_string(in, PYCEE_VERSION,
                strlen(PYCEE_VERSION))->obj));
}

/* sys.exit() / sys.exit(code): as exit() in the REPL (rt_exit). */
int nat_sys_exit(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)out;
    return (rt_exit(in, base, nargs, call, err));
}

/*
 * recursion_limit(): how many calls can be in progress at once (1000 at
 * the start); recursion_limit(n) changes it. The real stack guard keeps
 * protecting whatever the limit is.
 */
int nat_sys_recursion_limit(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int64_t n;

    if (nargs == 0)
    {
        *out = val_int(in->max_depth);
        return (0);
    }
    if (rt_int_arg(call, ARG(0), "recursion_limit", "limit", &n, err) == -1)
        return (-1);
    if (n < 1 || n > INT32_MAX)
        return (rt_error(err, ERR_VALUE, call, "the recursion limit must be"
                " from 1 to %d, not %lld", INT32_MAX, (long long)n));
    in->max_depth = (int)n;
    *out = val_none();
    return (0);
}

/* How many objects a collection run now frees. */
int nat_sys_gc_collect(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    size_t  before;

    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    before = in->heap.count;
    heap_collect(&in->heap);
    *out = val_int((int64_t)(before - in->heap.count));
    return (0);
}

/* The bytes the live objects take. */
int nat_sys_memory(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_int((int64_t)in->heap.bytes);
    return (0);
}

/* key (a new string): v, into the dict at stack[slot]. */
static int  put_stat(t_interp *in, const t_node *call, size_t slot,
                const char *key, size_t v, t_error *err)
{
    t_string    *s;
    int         status;

    s = rt_new_string(in, key, strlen(key));
    rt_push(in, val_obj(&s->obj));
    status = rt_table_put(in, call, slot, in->stack[in->top - 1],
            val_int((int64_t)v), err);
    rt_pop(in);
    return (status);
}

/*
 * dict("collections": ..., "objects": ..., "bytes": ..., "next_gc": ...),
 * read before the dict is made: making it allocates.
 */
int nat_sys_gc_stats(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    size_t  stats[4];
    size_t  slot;

    (void)base;
    (void)nargs;
    stats[0] = in->heap.collections;
    stats[1] = in->heap.count;
    stats[2] = in->heap.bytes;
    stats[3] = in->heap.next_gc;
    slot = in->top;
    rt_push(in, val_obj(&rt_table_new(&in->heap, OBJ_DICT)->obj));
    if (put_stat(in, call, slot, "collections", stats[0], err) == -1
        || put_stat(in, call, slot, "objects", stats[1], err) == -1
        || put_stat(in, call, slot, "bytes", stats[2], err) == -1
        || put_stat(in, call, slot, "next_gc", stats[3], err) == -1)
        return (-1);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

int nat_sys_gc_enable(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    heap_enable(&in->heap, 1);
    *out = val_none();
    return (0);
}

/*
 * No automatic collection until gc_enable (gc_set_max neither): memory
 * is then the program's business. Running out of it ends the program
 * cleanly, as always.
 */
int nat_sys_gc_disable(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    heap_enable(&in->heap, 0);
    *out = val_none();
    return (0);
}

/* A size in bytes for fname: an integer >= 0. */
static int  bytes_arg(t_interp *in, size_t base, const t_node *call,
                const char *fname, size_t *out, t_error *err)
{
    int64_t n;

    if (rt_int_arg(call, ARG(0), fname, "number of bytes", &n, err) == -1)
        return (-1);
    if (n < 0)
        return (rt_error(err, ERR_VALUE, call, "%s() needs a number of bytes"
                " >= 0, not %lld", fname, (long long)n));
    *out = (size_t)n;
    return (0);
}

/* Below this many bytes in use, no automatic collection (1 MiB at the start). */
int nat_sys_gc_set_min(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    size_t  bytes;

    (void)nargs;
    if (bytes_arg(in, base, call, "gc_set_min", &bytes, err) == -1)
        return (-1);
    heap_set_min(&in->heap, bytes);
    *out = val_none();
    return (0);
}

/* On reaching this many bytes in use, always a collection (0: no maximum). */
int nat_sys_gc_set_max(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    size_t  bytes;

    (void)nargs;
    if (bytes_arg(in, base, call, "gc_set_max", &bytes, err) == -1)
        return (-1);
    heap_set_max(&in->heap, bytes);
    *out = val_none();
    return (0);
}
