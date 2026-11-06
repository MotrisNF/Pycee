/*
 * Module os: the file system and the environment. Paths are strings
 * relative to the current directory; what the system refuses is an
 * IOError (FileNotFoundError, PermissionError) with the path, as in io.
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "natives/modules.h"

/* ARG(i) as a C path (rt_path_arg has checked it: no '\0' inside). */
#define PATH(i) (val_string(ARG(i))->chars)
#define PATH_LEN(i) (val_string(ARG(i))->len)

static int  paths(t_interp *in, size_t base, int n, const t_node *call,
                const char *fname, t_error *err)
{
    int i;

    for (i = 0; i < n; i++)
        if (rt_path_arg(call, ARG(i), fname, err) == -1)
            return (-1);
    return (0);
}

static int  fail(t_interp *in, size_t base, int i, const t_node *call,
                t_error *err)
{
    return (rt_io_error(call, PATH(i), PATH_LEN(i), errno, err));
}

/* exists / is_file / is_dir: what stat says (a link is followed). */
static int  test_path(t_interp *in, size_t base, const t_node *call,
                const char *fname, int which, t_value *out, t_error *err)
{
    struct stat st;
    int         found;

    if (paths(in, base, 1, call, fname, err) == -1)
        return (-1);
    found = (stat(PATH(0), &st) == 0);
    if (which == 1)
        found = found && S_ISREG(st.st_mode);
    else if (which == 2)
        found = found && S_ISDIR(st.st_mode);
    *out = val_bool(found);
    return (0);
}

int nat_os_exists(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    return (test_path(in, base, call, "exists", 0, out, err));
}

int nat_os_is_file(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    return (test_path(in, base, call, "is_file", 1, out, err));
}

int nat_os_is_dir(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    return (test_path(in, base, call, "is_dir", 2, out, err));
}

/* Byte order of two strings, for sorting names. */
static int  by_bytes(const void *a, const void *b)
{
    const t_string  *x;
    const t_string  *y;
    size_t          n;
    int             c;

    x = val_string(*(const t_value *)a);
    y = val_string(*(const t_value *)b);
    n = x->len < y->len ? x->len : y->len;
    c = memcmp(x->chars, y->chars, n);
    if (c != 0)
        return (c);
    return ((x->len > y->len) - (x->len < y->len));
}

/*
 * list_dir() / list_dir(p): the names in the directory (not "." and
 * ".."), sorted. The open directory is the interpreter's (in->dir) while
 * the names are made, so running out of memory still closes it.
 */
int nat_os_list_dir(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    struct dirent   *e;
    const char      *p;
    t_list          *l;
    t_string        *s;
    size_t          slot;

    p = ".";
    if (nargs == 1 && ARG(0).type != VAL_NONE)
    {
        if (paths(in, base, 1, call, "list_dir", err) == -1)
            return (-1);
        p = PATH(0);
    }
    errno = 0;
    in->dir = opendir(p);
    if (!in->dir)
        return (rt_io_error(call, p, strlen(p), errno, err));
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    while ((e = readdir(in->dir)) != NULL)
    {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue ;
        s = rt_new_string(in, e->d_name, strlen(e->d_name));
        rt_push(in, val_obj(&s->obj));
        l = val_list(in->stack[slot]);
        heap_list_reserve(&in->heap, l, l->len + 1);
        l->items[l->len++] = rt_pop(in);
    }
    closedir(in->dir);
    in->dir = NULL;
    l = val_list(in->stack[slot]);
    if (l->len > 1)
        qsort(l->items, l->len, sizeof(t_value), by_bytes);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/* remove(p): a file (a directory is an IOError: remove_dir). */
int nat_os_remove(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    struct stat st;

    (void)nargs;
    if (paths(in, base, 1, call, "remove", err) == -1)
        return (-1);
    errno = 0;
    if (lstat(PATH(0), &st) == 0 && S_ISDIR(st.st_mode))
        return (rt_io_error(call, PATH(0), PATH_LEN(0), EISDIR, err));
    if (unlink(PATH(0)) == -1)
        return (fail(in, base, 0, call, err));
    *out = val_none();
    return (0);
}

/* rename(old, new): moves or renames; new is replaced if it exists. */
int nat_os_rename(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    if (paths(in, base, 2, call, "rename", err) == -1)
        return (-1);
    errno = 0;
    if (rename(PATH(0), PATH(1)) == -1)
        return (fail(in, base, errno == ENOENT ? 0 : 1, call, err));
    *out = val_none();
    return (0);
}

/* One directory; with parents, one that already exists is fine. */
static int  make_one(const char *p, int parents)
{
    struct stat st;

    if (mkdir(p, 0777) == 0)
        return (0);
    if (parents && errno == EEXIST && stat(p, &st) == 0 && S_ISDIR(st.st_mode))
        return (0);
    if (parents && errno == EEXIST)
        errno = ENOTDIR;
    return (-1);
}

/*
 * Every directory of p from the top, in a buffer of the session: "a/b/c"
 * makes "a", "a/b", then "a/b/c". On failure, *at is where the path that
 * failed ends.
 */
static int  make_parents(t_interp *in, const char *p, size_t len,
                size_t *at)
{
    t_buffer    *b;
    size_t      i;
    int         status;

    b = rt_buf_take(in);
    buf_add(b, p, len);
    status = 0;
    for (i = 1; i <= len && status == 0; i++)
    {
        if (i < len && (p[i] != '/' || p[i - 1] == '/'))
            continue ;
        b->data[i] = '\0';
        status = make_one(b->data, 1);
        *at = i;
        if (i < len)
            b->data[i] = '/';
    }
    rt_buf_give(in);
    return (status);
}

/*
 * make_dir(p): one directory (an error if it exists); make_dir(p, "r"):
 * also every one missing on the way, and one that exists is fine.
 */
int nat_os_make_dir(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    const t_string  *m;
    size_t          at;

    if (paths(in, base, 1, call, "make_dir", err) == -1)
        return (-1);
    if (nargs == 2)
    {
        if (!val_is_string(ARG(1)))
            return (rt_error(err, ERR_TYPE, call, "make_dir() takes \"r\" as"
                    " its second argument (make every directory on the way),"
                    " not '%s'", val_type_name(ARG(1))));
        m = val_string(ARG(1));
        if (m->len != 1 || m->chars[0] != 'r')
            return (rt_error(err, ERR_VALUE, call, "make_dir() takes \"r\" as"
                    " its second argument (make every directory on the way),"
                    " not \"%.*s\"", (int)m->len, m->chars));
    }
    errno = 0;
    at = PATH_LEN(0);
    if (nargs == 2 && make_parents(in, PATH(0), PATH_LEN(0), &at) == -1)
        return (rt_io_error(call, PATH(0), at, errno, err));
    if (nargs == 1 && make_one(PATH(0), 0) == -1)
        return (fail(in, base, 0, call, err));
    *out = val_none();
    return (0);
}

/* remove_dir(p): only an empty directory ("directory not empty" otherwise). */
int nat_os_remove_dir(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    if (paths(in, base, 1, call, "remove_dir", err) == -1)
        return (-1);
    errno = 0;
    if (rmdir(PATH(0)) == -1)
    {
        if (errno == EEXIST)
            errno = ENOTEMPTY;
        return (fail(in, base, 0, call, err));
    }
    *out = val_none();
    return (0);
}

/* cwd(): the current directory, as an absolute path. */
int nat_os_cwd(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    t_buffer    *b;
    size_t      size;

    (void)base;
    (void)nargs;
    b = rt_buf_take(in);
    size = PATH_MAX;
    while (1)
    {
        buf_clear(b);
        buf_fill(b, 0, size);
        errno = 0;
        if (getcwd(b->data, size))
            break ;
        if (errno != ERANGE)
        {
            rt_buf_give(in);
            return (rt_io_error(call, ".", 1, errno, err));
        }
        size *= 2;
    }
    *out = val_obj(&rt_new_string(in, b->data, strlen(b->data))->obj);
    rt_buf_give(in);
    return (0);
}

/* env(name): the environment variable, or none if it is not set. */
int nat_os_env(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    const char  *v;

    (void)nargs;
    if (!val_is_string(ARG(0)))
        return (rt_error(err, ERR_TYPE, call, "env() needs a name (a string),"
                " not '%s'", val_type_name(ARG(0))));
    if (memchr(PATH(0), '\0', PATH_LEN(0)))
    {
        *out = val_none();
        return (0);
    }
    v = getenv(PATH(0));
    *out = v ? val_obj(&rt_new_string(in, v, strlen(v))->obj) : val_none();
    return (0);
}

/* The text of a string argument of a path function (no system involved). */
static int  str_arg(t_interp *in, size_t base, int i, const t_node *call,
                const char *fname, t_error *err)
{
    if (!val_is_string(ARG(i)))
        return (rt_error(err, ERR_TYPE, call, "%s() needs strings, not '%s'",
                fname, val_type_name(ARG(i))));
    return (0);
}

/*
 * join(a, b, ...): the parts with one "/" between them, as Python's
 * os.path.join: a part that starts with "/" starts again from there, and
 * no "/" is added after one that ends with it.
 */
int nat_os_join(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    const t_string  *s;
    t_buffer        *b;
    int             i;

    for (i = 0; i < nargs; i++)
        if (str_arg(in, base, i, call, "join", err) == -1)
            return (-1);
    b = rt_buf_take(in);
    for (i = 0; i < nargs; i++)
    {
        s = val_string(ARG(i));
        if (s->len > 0 && s->chars[0] == '/')
            buf_clear(b);
        else if (b->len > 0 && b->data[b->len - 1] != '/')
            buf_add_char(b, '/');
        buf_add(b, s->chars, s->len);
    }
    *out = val_obj(&rt_new_string(in, b->data ? b->data : "", b->len)->obj);
    rt_buf_give(in);
    return (0);
}

/* Where the last part of s starts: after its last "/". */
static size_t   last_part(const t_string *s)
{
    size_t  i;

    i = s->len;
    while (i > 0 && s->chars[i - 1] != '/')
        i--;
    return (i);
}

/* base_name(p): what follows the last "/" ("a/b.txt" -> "b.txt"; "a/" -> ""). */
int nat_os_base_name(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    const t_string  *s;
    size_t          i;

    (void)nargs;
    if (str_arg(in, base, 0, call, "base_name", err) == -1)
        return (-1);
    s = val_string(ARG(0));
    i = last_part(s);
    *out = val_obj(&heap_string(&in->heap, s->chars + i, s->len - i)->obj);
    return (0);
}

/*
 * dir_name(p): what comes before the last "/", without the "/" at its
 * end ("a/b.txt" -> "a"; "b.txt" -> ""; "/a" -> "/").
 */
int nat_os_dir_name(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    const t_string  *s;
    size_t          i;
    size_t          j;

    (void)nargs;
    if (str_arg(in, base, 0, call, "dir_name", err) == -1)
        return (-1);
    s = val_string(ARG(0));
    i = last_part(s);
    j = i;
    while (j > 0 && s->chars[j - 1] == '/')
        j--;
    if (j == 0)
        j = i;
    *out = val_obj(&heap_string(&in->heap, s->chars, j)->obj);
    return (0);
}
