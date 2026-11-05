#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "core/arena.h"
#include "core/buffer.h"
#include "core/errors.h"
#include "core/reader.h"
#include "core/source.h"
#include "core/stack.h"
#include "front/ast.h"
#include "front/lexer.h"
#include "front/parser.h"
#include "resolver/resolver.h"
#include "runtime/interp.h"

#define EXIT_LANG_ERROR 1
#define EXIT_USAGE      2

typedef enum e_mode
{
    MODE_RUN,
    MODE_TOKENS,
    MODE_AST,
    MODE_CHECK
}   t_mode;

/*
 * Everything that lives as long as the program: the arena (sources and
 * AST), the table of loaded sources and the resolver's global names. In
 * the REPL it spans the whole session, so later lines can use what
 * earlier lines declared. input is the buffer being read (the file text
 * until it is copied into the arena, or the REPL line) and pending the
 * REPL lines of an input not complete yet: they belong to the session so
 * that an out-of-memory exit frees them too. repl: the session is the
 * REPL (parse_repl, values echoed).
 */
typedef struct s_session
{
    t_mode      mode;
    t_arena     arena;
    t_sources   sources;
    t_resolver  resolver;
    t_interp    interp;
    int         repl;
    char        *input;
    t_buffer    pending;
}   t_session;

/*
 * Prints every token as "line:col  TYPE  'lexeme'". The line is tracked
 * incrementally between tokens, so the whole dump is a single pass.
 * Token offsets are global; i walks the local text.
 */
static int  dump_tokens(const t_source *src, t_error *err)
{
    t_lexer     lx;
    t_token     tok;
    size_t      line;
    size_t      line_start;
    size_t      local;
    size_t      i;

    lexer_init(&lx, src);
    line = 1;
    line_start = 0;
    i = 0;
    while (1)
    {
        if (lexer_next(&lx, &tok, err) == -1)
            return (-1);
        local = tok.offset - src->base;
        for (; i < local; i++)
        {
            if (src->text[i] == '\n')
            {
                line++;
                line_start = i + 1;
            }
        }
        printf("%zu:%-4zu %-20s", line, local - line_start + 1,
            token_type_name(tok.type));
        if (tok.len)
            printf(" '%.*s'", (int)tok.len, src->text + local);
        putchar('\n');
        if (tok.type == TOK_EOF)
            return (0);
    }
}

/*
 * Runs a registered source. --tokens and --ast dump; otherwise it parses
 * and resolves, --check stops there (analysis only), and the normal mode
 * runs it. Fills err on failure.
 */
static int  run_source(t_session *s, const t_source *src, t_error *err)
{
    t_node  *root;

    if (s->mode == MODE_TOKENS)
        return (dump_tokens(src, err));
    if (s->repl)
        root = parse_repl(&s->arena, src, err);
    else
        root = parse_program(&s->arena, src, err);
    if (!root)
        return (-1);
    if (s->mode == MODE_AST)
    {
        ast_print(root);
        return (0);
    }
    if (resolve_program(&s->resolver, src, root, err) == -1)
        return (-1);
    if (s->mode == MODE_CHECK)
        return (0);
    return (interp_run(&s->interp, root, s->repl, err));
}

static void session_free(t_session *s)
{
    fatal_oom_set_cleanup(NULL, NULL);
    free(s->input);
    s->input = NULL;
    buf_free(&s->pending);
    interp_free(&s->interp);
    resolver_free(&s->resolver);
    sources_free(&s->sources);
    arena_free(&s->arena);
}

static void session_cleanup(void *s)
{
    session_free(s);
}

/*
 * Everything that does not allocate is set up first, and only then is
 * the cleanup registered: fatal_oom may fire during resolver_init (its
 * first allocation), and session_free must find every member valid.
 */
static void session_init(t_session *s, t_mode mode, int repl)
{
    s->mode = mode;
    s->input = NULL;
    s->repl = repl;
    arena_init(&s->arena);
    sources_init(&s->sources);
    buf_init(&s->pending);
    interp_init(&s->interp, &s->resolver);
    fatal_oom_set_cleanup(session_cleanup, s);
    resolver_init(&s->resolver, &s->arena, &s->sources, repl);
}

/* args: the program's own arguments, args[0] being the file. */
static int  run_file(const char *path, t_mode mode, int nargs, char **args)
{
    t_session       s;
    const t_source  *src;
    size_t          len;
    t_error         err;
    int             status;

    session_init(&s, mode, 0);
    interp_set_args(&s.interp, nargs, args);
    s.input = read_file(path, &len);
    if (!s.input)
    {
        session_free(&s);
        return (EXIT_USAGE);
    }
    src = sources_add(&s.sources, &s.arena, path, s.input, len);
    free(s.input);
    s.input = NULL;
    status = 0;
    if (run_source(&s, src, &err) == -1)
    {
        fflush(stdout);
        if (err.type == ERR_EXIT)
            status = err.code;
        else
        {
            error_print(&err, &s.sources);
            status = EXIT_LANG_ERROR;
        }
    }
    session_free(&s);
    return (status);
}

static void prompt(int interactive, const char *text)
{
    if (!interactive)
        return ;
    fputs(text, stdout);
    fflush(stdout);
}

/*
 * Reads what is typed until EOF (Ctrl+D). An input that stops in the
 * middle of something (an open block, "2 +") goes on in the next line,
 * with the prompt "... ": the lines are joined in s.pending and parsed
 * again. Each complete input is registered as a source "<stdin:N>", N
 * being its first line. An error is printed and the session goes on.
 * The prompts are only shown when stdin is a terminal. getline also
 * returns -1 when it runs out of memory: that is a MemoryError. exit()
 * ends the session with its code, after everything is unwound.
 */
static int  run_repl(t_mode mode)
{
    t_session       s;
    const t_source  *src;
    char            path[32];
    size_t          line_no;
    size_t          first_line;
    size_t          cap;
    ssize_t         len;
    t_error         err;
    int             interactive;
    int             status;

    session_init(&s, mode, 1);
    status = -1;
    cap = 0;
    line_no = 0;
    first_line = 0;
    interactive = isatty(STDIN_FILENO);
    while (status == -1)
    {
        prompt(interactive, s.pending.len ? "... " : ">>> ");
        errno = 0;
        len = getline(&s.input, &cap, stdin);
        if (len == -1)
        {
            if (errno == ENOMEM)
                fatal_oom();
            if (s.pending.len)
                error_print(&err, &s.sources);
            if (interactive)
                fputc('\n', stdout);
            status = 0;
            break ;
        }
        if (!s.pending.len)
            first_line = line_no + 1;
        line_no++;
        buf_add(&s.pending, s.input, (size_t)len);
        snprintf(path, sizeof(path), "<stdin:%zu>", first_line);
        src = sources_add(&s.sources, &s.arena, path, s.pending.data,
                s.pending.len);
        if (run_source(&s, src, &err) == -1)
        {
            if (err.incomplete)
                continue ;
            fflush(stdout);
            if (err.type == ERR_EXIT)
                status = err.code;
            else
                error_print(&err, &s.sources);
        }
        buf_clear(&s.pending);
    }
    session_free(&s);
    return (status);
}

int main(int argc, char **argv)
{
    t_mode  mode;
    int     i;

    stack_init();
    mode = MODE_RUN;
    i = 1;
    if (i < argc && strcmp(argv[i], "--tokens") == 0)
    {
        mode = MODE_TOKENS;
        i++;
    }
    else if (i < argc && strcmp(argv[i], "--ast") == 0)
    {
        mode = MODE_AST;
        i++;
    }
    else if (i < argc && strcmp(argv[i], "--check") == 0)
    {
        mode = MODE_CHECK;
        i++;
    }
    if (i == argc)
        return (run_repl(mode));
    if (argv[i][0] != '-')
        return (run_file(argv[i], mode, argc - i, argv + i));
    fprintf(stderr, "usage: %s [--tokens | --ast | --check] [file.pce"
        " [arguments...]]\n", argv[0]);
    return (EXIT_USAGE);
}

