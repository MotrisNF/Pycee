#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/errors.h"
#include "core/utf8.h"

static void (*g_oom_cleanup)(void *);
static void *g_oom_ctx;

static const char   *g_error_names[ERR_TYPE_COUNT] = {
    [ERR_SYNTAX] = "SyntaxError",
    [ERR_NAME] = "NameError",
    [ERR_TYPE] = "TypeError",
    [ERR_ATTRIBUTE] = "AttributeError",
    [ERR_IMPORT] = "ImportError",
    [ERR_ZERO_DIVISION] = "ZeroDivisionError",
    [ERR_VALUE] = "ValueError",
    [ERR_OVERFLOW] = "OverflowError",
    [ERR_RECURSION] = "RecursionError",
    [ERR_NOT_IMPLEMENTED] = "NotImplementedError",
    [ERR_INDEX] = "IndexError",
    [ERR_ASSIGNMENT] = "AssignmentError",
    [ERR_KEY] = "KeyError",
    [ERR_RUNTIME] = "RuntimeError",
    [ERR_EOF] = "EOFError",
    [ERR_ASSERTION] = "AssertionError",
    [ERR_ERROR] = "Error",
    [ERR_EXIT] = "SystemExit",
};

/*
 * The parent of each type (-1: none). Everything descends from Error
 * except Error itself and SystemExit. A deeper tree (or the errors of
 * the user's obj, later) is only more entries here.
 */
static int  error_parent(t_error_type type)
{
    if (type == ERR_ERROR || type == ERR_EXIT)
        return (-1);
    return (ERR_ERROR);
}

int error_is_a(t_error_type type, t_error_type ancestor)
{
    int t;

    for (t = (int)type; t != -1; t = error_parent((t_error_type)t))
        if (t == (int)ancestor)
            return (1);
    return (0);
}

int error_type_named(const char *name, size_t len)
{
    int i;

    for (i = 0; i < ERR_TYPE_COUNT; i++)
        if (strlen(g_error_names[i]) == len
            && memcmp(g_error_names[i], name, len) == 0)
            return (i);
    return (-1);
}

const char  *error_type_name(t_error_type type)
{
    return (g_error_names[type]);
}

int error_set(
    t_error *err, t_error_type type, size_t offset,
    size_t len, const char *fmt, ...
)
{
    va_list ap;

    va_start(ap, fmt);
    error_vset(err, type, offset, len, fmt, ap);
    va_end(ap);
    return (-1);
}

int error_vset(
    t_error *err, t_error_type type, size_t offset,
    size_t len, const char *fmt, va_list ap
)
{
    err->type = type;
    err->offset = offset;
    err->len = len;
    err->incomplete = 0;
    err->code = 0;
    err->total = 0;
    err->top_name = "<module>";
    err->top_len = 8;
    err->context = NULL;
    err->self = NULL;
    err->instance = NULL;
    err->type_name = NULL;
    if (vsnprintf(err->msg, ERR_MSG_SIZE, fmt, ap) >= ERR_MSG_SIZE)
        err->msg[utf8_boundary(err->msg, strlen(err->msg))] = '\0';
    return (-1);
}

void    fatal_oom_set_cleanup(void (*fn)(void *), void *ctx)
{
    g_oom_cleanup = fn;
    g_oom_ctx = ctx;
}

/* The cleanup is unregistered before it runs, so it is never run twice. */
void    fatal_oom(void)
{
    void    (*cleanup)(void *);

    fputs("MemoryError: out of memory\n", stderr);
    cleanup = g_oom_cleanup;
    g_oom_cleanup = NULL;
    if (cleanup)
        cleanup(g_oom_ctx);
    exit(1);
}

void    error_add_frame(t_error *err, size_t offset, size_t len,
            const char *name, size_t name_len)
{
    t_frame_info    *f;

    if (err->total < TB_KEEP)
        f = &err->inner[err->total];
    else
        f = &err->outer[(err->total - TB_KEEP) % TB_KEEP];
    f->offset = offset;
    f->len = len;
    f->name = name;
    f->name_len = name_len;
    err->total++;
}

/* A position as its source, its line and that line's text [line, end). */
typedef struct s_where
{
    const t_source  *source;
    size_t          line_no;
    const char      *line;
    const char      *end;
    const char      *at;
}   t_where;

static void locate(const t_sources *sources, size_t offset, t_where *w)
{
    const char  *p;

    w->source = sources_find(sources, offset);
    w->at = w->source->text + (offset - w->source->base);
    w->line = w->source->text;
    w->line_no = 1;
    for (p = w->source->text; p < w->at; p++)
    {
        if (*p == '\n')
        {
            w->line = p + 1;
            w->line_no++;
        }
    }
    w->end = w->line;
    while (*w->end && *w->end != '\n')
        w->end++;
    if (w->end > w->line && w->end[-1] == '\r')
        w->end--;
}

/*
 * Where the text of an error goes: appended to b, or written to stderr
 * when b is NULL (error_print), so printing needs no memory.
 */
static void out_add(t_buffer *b, const char *s, size_t len)
{
    if (b)
        buf_add(b, s, len);
    else
        fwrite(s, 1, len, stderr);
}

static void out_str(t_buffer *b, const char *s)
{
    out_add(b, s, strlen(s));
}

static void out_char(t_buffer *b, char c)
{
    out_add(b, &c, 1);
}

static void out_printf(t_buffer *b, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void out_printf(t_buffer *b, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (b)
        buf_vprintf(b, fmt, ap);
    else
        vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* The line of code, then a '^' under each character of [at, at + len). */
static void print_code(t_buffer *b, const t_where *w, size_t len,
                const char *indent)
{
    const char  *p;

    out_str(b, indent);
    out_add(b, w->line, (size_t)(w->end - w->line));
    out_char(b, '\n');
    out_str(b, indent);
    for (p = w->line; p < w->at; p++)
        out_char(b, *p == '\t' ? '\t' : ' ');
    if (len > (size_t)(w->end - p))
        len = w->end - p;
    if (len == 0)
        len = 1;
    while (len--)
        out_char(b, '^');
    out_char(b, '\n');
}

/* The k-th frame added, or NULL if it was not kept. */
static const t_frame_info   *frame_at(const t_error *err, size_t k)
{
    if (k >= err->total)
        return (NULL);
    if (k < TB_KEEP)
        return (&err->inner[k]);
    if (k + TB_KEEP < err->total)
        return (NULL);
    return (&err->outer[(k - TB_KEEP) % TB_KEEP]);
}

static void print_frame(t_buffer *b, const t_sources *sources,
                const t_frame_info *f, const char *name, size_t name_len)
{
    t_where w;

    locate(sources, f->offset, &w);
    out_printf(b, "  File \"%s\", line %zu, in %.*s\n", w.source->path,
        w.line_no, (int)name_len, name);
    print_code(b, &w, f->len, "    ");
}

/*
 * Frame k is a call made inside the function of frame k + 1 (the
 * outermost one, inside top_name). A run of the same call (a recursion)
 * shows three times and then how many more there were, as in Python;
 * the calls that were not kept count in the run when the same call is
 * on both sides of them.
 */
static void print_traceback(const t_error *err, const t_sources *sources,
                t_buffer *b)
{
    const t_frame_info  *f;
    const t_frame_info  *caller;
    const t_frame_info  *prev;
    const t_frame_info  *after;
    t_frame_info        here;
    size_t              k;
    size_t              repeats;

    out_str(b, "Traceback (most recent call last):\n");
    prev = NULL;
    repeats = 0;
    for (k = err->total; k-- > 0;)
    {
        f = frame_at(err, k);
        if (!f)
        {
            after = frame_at(err, TB_KEEP - 1);
            if (prev && after->offset == prev->offset)
                repeats++;
            else if (k == TB_KEEP)
                out_printf(b, "  [... %zu more calls ...]\n",
                    err->total - 2 * TB_KEEP);
            continue ;
        }
        if (prev && prev->offset == f->offset)
        {
            if (++repeats >= 3)
                continue ;
        }
        else
        {
            if (repeats >= 3)
                out_printf(b, "  [Previous line repeated %zu more"
                    " times]\n", repeats - 2);
            repeats = 0;
        }
        prev = f;
        caller = frame_at(err, k + 1);
        if (k + 1 == err->total)
            print_frame(b, sources, f, err->top_name, err->top_len);
        else
            print_frame(b, sources, f, caller ? caller->name : f->name,
                caller ? caller->name_len : f->name_len);
    }
    if (repeats >= 3)
        out_printf(b, "  [Previous line repeated %zu more times]\n",
            repeats - 2);
    here.offset = err->offset;
    here.len = err->len;
    print_frame(b, sources, &here, err->inner[0].name, err->inner[0].name_len);
    out_printf(b, "%s: %s\n", err->type_name ? err->type_name
        : error_type_name(err->type), err->msg);
}

static void format_one(const t_error *err, const t_sources *sources,
                t_buffer *b)
{
    t_where w;

    if (err->total > 0)
    {
        print_traceback(err, sources, b);
        return ;
    }
    locate(sources, err->offset, &w);
    out_printf(b, "%s: %s (%s, line %zu)\n", err->type_name ? err->type_name
        : error_type_name(err->type), err->msg, w.source->path, w.line_no);
    print_code(b, &w, err->len, "    ");
}

/*
 * The errors it was raised while handling come first, oldest first, as
 * in Python (a chain longer than 32 is cut there).
 */
static void format_chain(const t_error *err, const t_sources *sources,
                t_buffer *b, int depth)
{
    if (err->context && depth < 32)
    {
        format_chain(err->context, sources, b, depth + 1);
        out_str(b, "\nDuring handling of the above error, another error"
            " occurred:\n\n");
    }
    format_one(err, sources, b);
}

void    error_format(const t_error *err, const t_sources *sources,
            t_buffer *b)
{
    format_chain(err, sources, b, 0);
}

/* Straight to stderr: printing an error never allocates. */
void    error_print(const t_error *err, const t_sources *sources)
{
    error_format(err, sources, NULL);
}
