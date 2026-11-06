/*
 * Files (module io): opening, reading, writing and closing, and the
 * methods of a file value; see docs/TYPES.md. Text is UTF-8: what is
 * read is cleaned like input() (an invalid byte becomes U+FFFD).
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "runtime/interp_internal.h"

#define SELF    (in->stack[base])
#define ARG(i)  (in->stack[base + 1 + (i)])

/* A path argument of fname: a string with no '\0' inside. */
int rt_path_arg(const t_node *call, t_value v, const char *fname,
        t_error *err)
{
    const t_string  *s;

    if (!val_is_string(v))
        return (rt_error(err, ERR_TYPE, call, "%s() needs a path (a string),"
                " not '%s'", fname, val_type_name(v)));
    s = val_string(v);
    if (memchr(s->chars, '\0', s->len))
        return (rt_error(err, ERR_VALUE, call, "%s(): a path cannot contain"
                " the character \\0", fname));
    return (0);
}

/*
 * What the system said (errno) about path: FileNotFoundError when it (or
 * a directory on the way) does not exist, PermissionError when it is not
 * allowed, IOError otherwise (a file where a directory should be: "not a
 * directory"); the message is the system's, with the path.
 * No memory left is fatal, as everywhere.
 */
int rt_io_error(const t_node *call, const char *path, size_t len, int errnum,
        t_error *err)
{
    t_error_type    type;
    char            msg[128];

    if (errnum == ENOMEM)
        fatal_oom();
    type = ERR_IO;
    if (errnum == ENOENT)
        type = ERR_FILE_NOT_FOUND;
    else if (errnum == EACCES || errnum == EPERM || errnum == EROFS)
        type = ERR_PERMISSION;
    snprintf(msg, sizeof(msg), "%s", strerror(errnum));
    if (msg[0] >= 'A' && msg[0] <= 'Z')
        msg[0] = (char)(msg[0] - 'A' + 'a');
    return (rt_error(err, type, call, "%s: \"%.*s\"", msg, (int)len, path));
}

/* A file value for fp (NULL: not open yet), shown with that path. */
t_file  *rt_file_new(t_interp *in, FILE *fp, char mode, int standard,
            const char *path, size_t len)
{
    t_file  *f;

    f = (t_file *)heap_alloc(&in->heap, OBJ_FILE, sizeof(t_file) + len + 1);
    f->fp = fp;
    f->mode = mode;
    f->standard = standard;
    f->path_len = len;
    memcpy(f->path, path, len);
    f->path[len] = '\0';
    return (f);
}

/*
 * Opens path, a string the caller keeps rooted ("r", "w" or "a"). The
 * file value exists before fopen, so whatever happens after, the
 * collector or the session's end closes it (fopen itself allocates
 * nothing of the heap). Files left open are closed only when collected,
 * and a small file value never makes the collector run on its own: so
 * when the system has no descriptors left, a collection (which closes
 * the unreachable files) runs and fopen is tried once more. A directory
 * is not a file: opening one to read works in C, so it is checked.
 */
int rt_file_open(t_interp *in, const t_node *call, t_value path, char mode,
        t_value *out, t_error *err)
{
    const t_string  *p;
    t_file          *f;
    struct stat     st;
    char            m[2];

    p = val_string(path);
    f = rt_file_new(in, NULL, mode, 0, p->chars, p->len);
    m[0] = mode;
    m[1] = '\0';
    errno = 0;
    f->fp = fopen(p->chars, m);
    if (!f->fp && (errno == EMFILE || errno == ENFILE))
    {
        rt_push(in, val_obj(&f->obj));
        heap_collect(&in->heap);
        rt_pop(in);
        errno = 0;
        f->fp = fopen(p->chars, m);
    }
    if (!f->fp)
        return (rt_io_error(call, p->chars, p->len, errno, err));
    if (fstat(fileno(f->fp), &st) == 0 && S_ISDIR(st.st_mode))
    {
        fclose(f->fp);
        f->fp = NULL;
        return (rt_io_error(call, p->chars, p->len, EISDIR, err));
    }
    *out = val_obj(&f->obj);
    return (0);
}

/* f must be open, and for reading (or writing). */
static int  check_open(const t_node *call, const t_file *f, int writing,
                t_error *err)
{
    if (!f->fp)
        return (rt_error(err, ERR_IO, call, "the file is closed: \"%.*s\"",
                (int)f->path_len, f->path));
    if (writing && f->mode == 'r')
        return (rt_error(err, ERR_IO, call, "the file is not open for"
                " writing: \"%.*s\"", (int)f->path_len, f->path));
    if (!writing && f->mode != 'r')
        return (rt_error(err, ERR_IO, call, "the file is not open for"
                " reading: \"%.*s\"", (int)f->path_len, f->path));
    return (0);
}

/* A read that stopped for an error, not at the end. */
static int  read_failed(const t_node *call, t_file *f, t_error *err)
{
    int errnum;

    errnum = errno ? errno : EIO;
    clearerr(f->fp);
    return (rt_io_error(call, f->path, f->path_len, errnum, err));
}

/*
 * The next line of the file at v, with its "\n" (the last one may have
 * none), or "" at the end. A standard input on a terminal can be read
 * again after Ctrl+D, as with input().
 */
int rt_file_read_line(t_interp *in, const t_node *call, t_value v,
        t_value *out, t_error *err)
{
    t_file  *f;
    ssize_t len;

    f = (t_file *)v.as.obj;
    if (check_open(call, f, 0, err) == -1)
        return (-1);
    while (1)
    {
        errno = 0;
        len = getline(&in->line, &in->line_cap, f->fp);
        if (len != -1 || errno != EINTR)
            break ;
        clearerr(f->fp);
        if (rt_interrupted(in, call, err) == -1)
            return (-1);
    }
    if (len == -1 && errno == ENOMEM)
        fatal_oom();
    if (len == -1 && ferror(f->fp))
        return (read_failed(call, f, err));
    if (len == -1)
    {
        if (f->standard)
            clearerr(f->fp);
        len = 0;
    }
    *out = val_obj(&rt_new_string(in, in->line ? in->line : "",
                (size_t)len)->obj);
    return (0);
}

/* Everything left in the file at v, as one string ("" at the end). */
int rt_file_read_all(t_interp *in, const t_node *call, t_value v,
        t_value *out, t_error *err)
{
    t_file      *f;
    t_buffer    *b;
    char        chunk[4096];
    size_t      n;

    f = (t_file *)v.as.obj;
    if (check_open(call, f, 0, err) == -1)
        return (-1);
    b = rt_buf_take(in);
    while (1)
    {
        errno = 0;
        while ((n = fread(chunk, 1, sizeof(chunk), f->fp)) > 0)
            buf_add(b, chunk, n);
        if (!ferror(f->fp) || errno != EINTR)
            break ;
        clearerr(f->fp);
        if (rt_interrupted(in, call, err) == -1)
        {
            rt_buf_give(in);
            return (-1);
        }
    }
    if (ferror(f->fp))
    {
        rt_buf_give(in);
        return (read_failed(call, f, err));
    }
    if (f->standard)
        clearerr(f->fp);
    *out = val_obj(&rt_new_string(in, b->data ? b->data : "", b->len)->obj);
    rt_buf_give(in);
    return (0);
}

/*
 * The lines left in the file at stack[slot], each with its "\n", as a
 * new list (kept at slot + 1 while it grows).
 */
int rt_file_read_lines(t_interp *in, const t_node *call, size_t slot,
        t_value *out, t_error *err)
{
    t_list  *l;
    t_value line;

    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    while (1)
    {
        if (rt_file_read_line(in, call, in->stack[slot], &line, err) == -1)
            return (-1);
        if (val_string(line)->len == 0)
            break ;
        l = val_list(in->stack[slot + 1]);
        rt_push(in, line);
        heap_list_reserve(&in->heap, l, l->len + 1);
        l->items[l->len++] = rt_pop(in);
    }
    *out = in->stack[slot + 1];
    in->top = slot + 1;
    return (0);
}

/*
 * Writes len bytes to f. Text for the terminal's error stream goes after
 * what is waiting for stdout, so the two never come out of order. A
 * signal that stops the write is attended, then the rest is written.
 */
int rt_file_write(t_interp *in, const t_node *call, t_file *f,
        const char *s, size_t len, t_error *err)
{
    size_t  done;

    if (check_open(call, f, 1, err) == -1)
        return (-1);
    if (f->fp == stderr)
        fflush(stdout);
    while (len > 0)
    {
        errno = 0;
        done = fwrite(s, 1, len, f->fp);
        s += done;
        len -= done;
        if (len == 0)
            break ;
        clearerr(f->fp);
        if (errno != EINTR)
            return (rt_io_error(call, f->path, f->path_len, errno ? errno
                    : EIO, err));
        if (rt_interrupted(in, call, err) == -1)
            return (-1);
    }
    return (0);
}

/*
 * Closes f; closing it again does nothing. What could not be written
 * shows up here (a full disk). The standard streams are never closed.
 */
int rt_file_close(const t_node *call, t_file *f, t_error *err)
{
    int status;

    if (f->standard)
        return (rt_error(err, ERR_IO, call, "the standard streams cannot be"
                " closed: \"%.*s\"", (int)f->path_len, f->path));
    if (!f->fp)
        return (0);
    errno = 0;
    status = fclose(f->fp);
    f->fp = NULL;
    if (status != 0)
        return (rt_io_error(call, f->path, f->path_len, errno ? errno : EIO,
                err));
    return (0);
}

/* v as an open file to write to, for print(file = v) (what names it). */
int rt_file_for_writing(const t_node *call, t_value v, const char *what,
        t_file **out, t_error *err)
{
    if (!val_is_obj(v, OBJ_FILE))
        return (rt_error(err, ERR_TYPE, call, "%s needs a file, not '%s'",
                what, val_type_name(v)));
    *out = (t_file *)v.as.obj;
    return (check_open(call, *out, 1, err));
}

/* ---------- methods ---------- */

static int  m_read(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (rt_file_read_all(in, call, SELF, out, err));
}

static int  m_read_line(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (rt_file_read_line(in, call, SELF, out, err));
}

static int  m_read_lines(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (rt_file_read_lines(in, call, base, out, err));
}

/* f.write(s): s must be a string; gives back the file, to chain writes. */
static int  m_write(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    if (!val_is_string(ARG(0)))
        return (rt_error(err, ERR_TYPE, call, "write() needs a string, not"
                " '%s' (str() converts it)", val_type_name(ARG(0))));
    if (rt_file_write(in, call, (t_file *)SELF.as.obj,
            val_string(ARG(0))->chars,
            val_string(ARG(0))->len, err) == -1)
        return (-1);
    *out = SELF;
    return (0);
}

/* Whether a read would give something: looks at the next byte. */
static int  m_has_next(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    t_file  *f;
    int     c;

    (void)nargs;
    f = (t_file *)SELF.as.obj;
    if (check_open(call, f, 0, err) == -1)
        return (-1);
    while (1)
    {
        errno = 0;
        c = fgetc(f->fp);
        if (c != EOF || !ferror(f->fp) || errno != EINTR)
            break ;
        clearerr(f->fp);
        if (rt_interrupted(in, call, err) == -1)
            return (-1);
    }
    if (c == EOF && ferror(f->fp))
        return (read_failed(call, f, err));
    if (c == EOF && f->standard)
        clearerr(f->fp);
    if (c != EOF)
        ungetc(c, f->fp);
    *out = val_bool(c != EOF);
    return (0);
}

static int  m_close(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    *out = val_none();
    return (rt_file_close(call, (t_file *)SELF.as.obj, err));
}

static int  m_is_closed(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_bool(((t_file *)SELF.as.obj)->fp == NULL);
    return (0);
}

/* What was written so far goes out now; gives back the file. */
static int  m_flush(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    t_file  *f;

    (void)nargs;
    f = (t_file *)SELF.as.obj;
    if (check_open(call, f, 1, err) == -1)
        return (-1);
    errno = 0;
    if (fflush(f->fp) != 0)
        return (rt_io_error(call, f->path, f->path_len, errno ? errno : EIO,
                err));
    *out = SELF;
    return (0);
}

const t_method  g_file_methods[] = {
    {"read", 0, 0, m_read, NULL},
    {"read_line", 0, 0, m_read_line, NULL},
    {"read_lines", 0, 0, m_read_lines, NULL},
    {"write", 1, 1, m_write, NULL},
    {"has_next", 0, 0, m_has_next, NULL},
    {"close", 0, 0, m_close, NULL},
    {"is_closed", 0, 0, m_is_closed, NULL},
    {"flush", 0, 0, m_flush, NULL},
    {NULL, 0, 0, NULL, NULL},
};
