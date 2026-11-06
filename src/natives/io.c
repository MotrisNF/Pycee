/* Module io: files and the standard streams. */

#include <string.h>
#include "natives/modules.h"

/* The standard streams: never closed (not even by close()). */
t_value nconst_io_stdin(t_interp *in)
{
    return (val_obj(&rt_file_new(in, stdin, 'r', 1, "<stdin>", 7)->obj));
}

t_value nconst_io_stdout(t_interp *in)
{
    return (val_obj(&rt_file_new(in, stdout, 'w', 1, "<stdout>", 8)->obj));
}

t_value nconst_io_stderr(t_interp *in)
{
    return (val_obj(&rt_file_new(in, stderr, 'w', 1, "<stderr>", 8)->obj));
}

/*
 * ARG(0), a path, opened with mode into stack[slot] (pushed: slot is
 * where it lands), for the functions that do everything in one call.
 */
static int  open_arg(t_interp *in, size_t base, const t_node *call,
                const char *fname, char mode, t_error *err)
{
    t_value f;

    if (rt_path_arg(call, ARG(0), fname, err) == -1
        || rt_file_open(in, call, ARG(0), mode, &f, err) == -1)
        return (-1);
    rt_push(in, f);
    return (0);
}

/* read_file(path): the whole file as one string. */
int nat_io_read_file(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    size_t  slot;

    (void)nargs;
    slot = in->top;
    if (open_arg(in, base, call, "read_file", 'r', err) == -1
        || rt_file_read_all(in, call, in->stack[slot], out, err) == -1)
        return (-1);
    rt_push(in, *out);
    if (rt_file_close(call, (t_file *)in->stack[slot].as.obj, err) == -1)
        return (-1);
    in->top = slot;
    return (0);
}

/* read_lines(path): a list of its lines, each with its "\n". */
int nat_io_read_lines(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    size_t  slot;

    (void)nargs;
    slot = in->top;
    if (open_arg(in, base, call, "read_lines", 'r', err) == -1
        || rt_file_read_lines(in, call, slot, out, err) == -1
        || rt_file_close(call, (t_file *)in->stack[slot].as.obj, err) == -1)
        return (-1);
    in->top = slot;
    return (0);
}

/* write_file / append_file(path, text): text must be a string. */
static int  put_file(t_interp *in, size_t base, const t_node *call,
                const char *fname, char mode, t_error *err)
{
    t_file  *f;
    size_t  slot;

    if (!val_is_string(ARG(1)))
        return (rt_error(err, ERR_TYPE, call, "%s() needs a string to write,"
                " not '%s' (str() converts it)", fname,
                val_type_name(ARG(1))));
    slot = in->top;
    if (open_arg(in, base, call, fname, mode, err) == -1)
        return (-1);
    f = (t_file *)in->stack[slot].as.obj;
    if (rt_file_write(in, call, f, val_string(ARG(1))->chars,
            val_string(ARG(1))->len, err) == -1
        || rt_file_close(call, f, err) == -1)
        return (-1);
    in->top = slot;
    return (0);
}

int nat_io_write_file(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    *out = val_none();
    return (put_file(in, base, call, "write_file", 'w', err));
}

int nat_io_append_file(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    *out = val_none();
    return (put_file(in, base, call, "append_file", 'a', err));
}

/* open(path) / open(path, mode): mode "r" (the default), "w" or "a". */
int nat_io_open(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    const t_string  *m;
    char            mode;

    mode = 'r';
    if (nargs == 2)
    {
        if (!val_is_string(ARG(1)))
            return (rt_error(err, ERR_TYPE, call, "open() needs a mode (a"
                    " string), not '%s'", val_type_name(ARG(1))));
        m = val_string(ARG(1));
        if (m->len != 1 || (m->chars[0] != 'r' && m->chars[0] != 'w'
                && m->chars[0] != 'a'))
            return (rt_error(err, ERR_VALUE, call, "open() mode must be \"r\","
                    " \"w\" or \"a\", not \"%.*s\"", (int)m->len, m->chars));
        mode = m->chars[0];
    }
    if (rt_path_arg(call, ARG(0), "open", err) == -1)
        return (-1);
    return (rt_file_open(in, call, ARG(0), mode, out, err));
}

/* eprint(a, b..., sep = " ", end = "\n"): print, to the error stream. */
int nat_io_eprint(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    t_buffer    *b;

    b = rt_buf_take(in);
    if (rt_print_args(in, base, nargs, call, b, err) == -1)
    {
        rt_buf_give(in);
        return (-1);
    }
    fflush(stdout);
    fwrite(b->data, 1, b->len, stderr);
    rt_buf_give(in);
    *out = val_none();
    return (0);
}
