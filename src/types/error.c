/* Methods of an error value: e.message(), e.line(), e.file(), e.traceback(). */

#include <string.h>
#include "runtime/interp_internal.h"

static const t_error    *self_error(const t_interp *in, size_t base)
{
    return (&((const t_error_value *)in->stack[base].as.obj)->err);
}

/* The text may come from outside (a path, a line of the source). */
static int  new_string(t_interp *in, const char *s, size_t len, t_value *out)
{
    t_string    *str;

    str = rt_new_string(in, s, len);
    *out = val_obj(&str->obj);
    return (0);
}

static int  m_message(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    const t_error   *e;

    (void)nargs;
    (void)call;
    (void)err;
    e = self_error(in, base);
    return (new_string(in, e->msg, strlen(e->msg), out));
}

/* Where it happened (where it was made, if it was never raised). */
static int  m_line(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_error   *e;
    const t_source  *src;

    (void)nargs;
    (void)call;
    (void)err;
    e = self_error(in, base);
    src = sources_find(in->resolver->sources, e->offset);
    *out = val_int((int64_t)source_line(src, e->offset));
    return (0);
}

static int  m_file(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_source  *src;

    (void)nargs;
    (void)call;
    (void)err;
    src = sources_find(in->resolver->sources, self_error(in, base)->offset);
    return (new_string(in, src->path, strlen(src->path), out));
}

/* The text that would be printed if nobody caught it. */
static int  m_traceback(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    buf_clear(&in->text);
    error_format(self_error(in, base), in->resolver->sources, &in->text);
    return (new_string(in, in->text.data, in->text.len, out));
}

const t_method  g_error_methods[] = {
    {"message", 0, 0, m_message, NULL},
    {"line", 0, 0, m_line, NULL},
    {"file", 0, 0, m_file, NULL},
    {"traceback", 0, 0, m_traceback, NULL},
    {NULL, 0, 0, NULL, NULL},
};
