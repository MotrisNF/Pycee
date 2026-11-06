#ifndef NATIVES_H
# define NATIVES_H

# include <stddef.h>

/*
 * Every native function, global or in a module of the language, in slot
 * order: the resolver reserves the global names (no shadowing), knows
 * what each module offers and checks arities, and the runtime
 * implements each one (src/natives/). Being one list, the slots of both
 * can never disagree.
 *
 * NATIVE_LIST: the global natives, implemented as native_<name>.
 * X(name, min arguments, max arguments (-1 = any), repl (1 if it only
 * exists in the REPL, where a file does not see it), names, kw).
 * Each error type a program can raise has its constructor here, named
 * as the type: ValueError("bad input").
 *
 * names and kw say which arguments can be given by name (NULL: none):
 * names are the positional parameters, in order, which may also be
 * named (os.list_dir(p = ".")); kw are keyword-only parameters, which
 * come after every positional one (print(..., sep = ", ")). Both are
 * NULL-terminated arrays of g_native_defs's file (native_defs.c).
 */
# define NATIVE_LIST(X) \
    X(print,    0, -1,  0, NULL, g_kw_print) \
    X(input,    0, 1,   0, NULL, NULL) \
    X(str,      1, 1,   0, NULL, NULL) \
    X(num,      1, 1,   0, NULL, NULL) \
    X(int,      1, 1,   0, NULL, NULL) \
    X(float,    1, 1,   0, NULL, NULL) \
    X(range,    1, 3,   0, NULL, NULL) \
    X(tuple,    0, -1,  0, NULL, NULL) \
    X(set,      0, -1,  0, NULL, NULL) \
    X(dict,     0, -1,  0, NULL, NULL) \
    X(any,      1, 1,   0, NULL, NULL) \
    X(all,      1, 1,   0, NULL, NULL) \
    X(sum,      1, 1,   0, NULL, NULL) \
    X(min,      1, 1,   0, NULL, NULL) \
    X(max,      1, 1,   0, NULL, NULL) \
    X(exit,     0, 1,   1, NULL, NULL) \
    X(Error,                0, 1, 0, NULL, NULL) \
    X(TypeError,            0, 1, 0, NULL, NULL) \
    X(ValueError,           0, 1, 0, NULL, NULL) \
    X(ZeroDivisionError,    0, 1, 0, NULL, NULL) \
    X(OverflowError,        0, 1, 0, NULL, NULL) \
    X(IndexError,           0, 1, 0, NULL, NULL) \
    X(KeyError,             0, 1, 0, NULL, NULL) \
    X(AttributeError,       0, 1, 0, NULL, NULL) \
    X(AssignmentError,      0, 1, 0, NULL, NULL) \
    X(RuntimeError,         0, 1, 0, NULL, NULL) \
    X(RecursionError,       0, 1, 0, NULL, NULL) \
    X(NotImplementedError,  0, 1, 0, NULL, NULL) \
    X(EOFError,             0, 1, 0, NULL, NULL) \
    X(AssertionError,       0, 1, 0, NULL, NULL) \
    X(IOError,              0, 1, 0, NULL, NULL) \
    X(FileNotFoundError,    0, 1, 0, NULL, NULL) \
    X(PermissionError,      0, 1, 0, NULL, NULL) \
    X(KeyboardInterrupt,    0, 1, 0, NULL, NULL)

/*
 * The modules of the language (import math). A user's math.pce next to
 * the importing file is an ImportError: the name is taken.
 */
# define NATIVE_MODULES(M) \
    M(sys) M(math) M(io) M(random) M(time) M(os) M(signal) M(json)

/*
 * What each module offers. F(module, name, min, max, names, kw) is a
 * function, implemented as nat_<module>_<name> (src/natives/<module>.c);
 * C(module, name) a constant, whose value nconst_<module>_<name> makes
 * when the module is first imported (a global of the module, read-only).
 * Rule: no member is called like a reserved word or a global native, so
 * any of them can be imported alone (checked by tests/check_members.sh).
 * The one exception is sys.exit, which only clashes in the REPL, where
 * "from sys import exit" is an error.
 * Careful: name is only ever used with # and ## here, since some names
 * (SIGINT, stdin) are C macros too.
 */
# define NATIVE_MEMBERS(F, C) \
    C(sys, argv) \
    C(sys, platform) \
    C(sys, version) \
    F(sys, exit,            0, 1,   NULL, NULL) \
    F(sys, recursion_limit, 0, 1,   NULL, NULL) \
    F(sys, gc_collect,      0, 0,   NULL, NULL) \
    F(sys, memory,          0, 0,   NULL, NULL) \
    F(sys, gc_stats,        0, 0,   NULL, NULL) \
    F(sys, gc_enable,       0, 0,   NULL, NULL) \
    F(sys, gc_disable,      0, 0,   NULL, NULL) \
    F(sys, gc_set_min,      1, 1,   NULL, NULL) \
    F(sys, gc_set_max,      1, 1,   NULL, NULL) \
    C(math, pi) \
    C(math, e) \
    C(math, tau) \
    C(math, inf) \
    C(math, nan) \
    F(math, sqrt,       1, 1,   NULL, NULL) \
    F(math, abs,        1, 1,   NULL, NULL) \
    F(math, round,      1, 2,   NULL, NULL) \
    F(math, floor,      1, 1,   NULL, NULL) \
    F(math, ceil,       1, 1,   NULL, NULL) \
    F(math, trunc,      1, 1,   NULL, NULL) \
    F(math, pow,        2, 2,   NULL, NULL) \
    F(math, exp,        1, 1,   NULL, NULL) \
    F(math, log,        1, 2,   NULL, NULL) \
    F(math, log2,       1, 1,   NULL, NULL) \
    F(math, log10,      1, 1,   NULL, NULL) \
    F(math, sin,        1, 1,   NULL, NULL) \
    F(math, cos,        1, 1,   NULL, NULL) \
    F(math, tan,        1, 1,   NULL, NULL) \
    F(math, asin,       1, 1,   NULL, NULL) \
    F(math, acos,       1, 1,   NULL, NULL) \
    F(math, atan,       1, 1,   NULL, NULL) \
    F(math, atan2,      2, 2,   NULL, NULL) \
    F(math, sinh,       1, 1,   NULL, NULL) \
    F(math, cosh,       1, 1,   NULL, NULL) \
    F(math, tanh,       1, 1,   NULL, NULL) \
    F(math, degrees,    1, 1,   NULL, NULL) \
    F(math, radians,    1, 1,   NULL, NULL) \
    F(math, hypot,      2, 2,   NULL, NULL) \
    F(math, gcd,        2, -1,  NULL, NULL) \
    F(math, lcm,        2, -1,  NULL, NULL) \
    F(math, factorial,  1, 1,   NULL, NULL) \
    F(math, is_nan,     1, 1,   NULL, NULL) \
    F(math, is_inf,     1, 1,   NULL, NULL) \
    F(math, is_finite,  1, 1,   NULL, NULL) \
    C(io, stdin) \
    C(io, stdout) \
    C(io, stderr) \
    F(io, read_file,    1, 1,   NULL, NULL) \
    F(io, read_lines,   1, 1,   NULL, NULL) \
    F(io, write_file,   2, 2,   NULL, NULL) \
    F(io, append_file,  2, 2,   NULL, NULL) \
    F(io, open,         1, 2,   NULL, NULL) \
    F(io, eprint,       0, -1,  NULL, g_kw_eprint) \
    F(random, rand_int,     2, 2,   NULL, NULL) \
    F(random, rand_float,   0, 0,   NULL, NULL) \
    F(random, choice,       1, 1,   NULL, NULL) \
    F(random, shuffle,      1, 1,   NULL, NULL) \
    F(random, sample,       2, 2,   NULL, NULL) \
    F(random, seed,         1, 1,   NULL, NULL) \
    F(time, now,    0, 0,   NULL, NULL) \
    F(time, clock,  0, 0,   NULL, NULL) \
    F(time, sleep,  1, 1,   NULL, NULL) \
    F(os, exists,       1, 1,   NULL, NULL) \
    F(os, is_file,      1, 1,   NULL, NULL) \
    F(os, is_dir,       1, 1,   NULL, NULL) \
    F(os, list_dir,     0, 1,   g_names_list_dir, NULL) \
    F(os, remove,       1, 1,   NULL, NULL) \
    F(os, rename,       2, 2,   NULL, NULL) \
    F(os, make_dir,     1, 2,   NULL, NULL) \
    F(os, remove_dir,   1, 1,   NULL, NULL) \
    F(os, cwd,          0, 0,   NULL, NULL) \
    F(os, env,          1, 1,   NULL, NULL) \
    F(os, join,         1, -1,  NULL, NULL) \
    F(os, base_name,    1, 1,   NULL, NULL) \
    F(os, dir_name,     1, 1,   NULL, NULL) \
    C(signal, SIGINT) \
    C(signal, SIGTERM) \
    C(signal, SIGHUP) \
    C(signal, SIGQUIT) \
    C(signal, SIGUSR1) \
    C(signal, SIGUSR2) \
    C(signal, SIGALRM) \
    C(signal, SIGPIPE) \
    C(signal, SIGCHLD) \
    F(signal, handle,   2, 2,   NULL, NULL) \
    F(signal, ignore,   1, 1,   NULL, NULL) \
    F(signal, reset,    1, 1,   NULL, NULL) \
    F(signal, send,     1, 1,   NULL, NULL) \
    F(json, parse,  1, 1,   NULL, NULL) \
    F(json, dump,   1, 1,   NULL, g_kw_dump)

# define NATIVE_MODULE_ENUM(mod) NMOD_##mod,

/* Which module a native or a constant belongs to; NMOD_NONE: global. */
typedef enum e_native_module
{
    NMOD_NONE = -1,
    NATIVE_MODULES(NATIVE_MODULE_ENUM)
    NMOD_COUNT
}   t_native_module;

/* One native function: its slot is its index in g_native_defs. */
typedef struct s_native_def
{
    t_native_module     module;
    const char          *name;
    const char          *qualified;     /* "math.sqrt", or "print" */
    int                 min;
    int                 max;
    int                 repl;
    const char *const   *names;
    const char *const   *kw;
}   t_native_def;

/* A constant of a module: its slot there is its order among that module's. */
typedef struct s_native_const
{
    t_native_module     module;
    const char          *name;
}   t_native_const;

extern const t_native_def   g_native_defs[];
extern const int            g_native_count;
extern const int            g_native_global_count;
extern const t_native_const g_native_consts[];
extern const int            g_native_const_count;
extern const char *const    g_native_module_names[];

/* The module of the language called name (len bytes), or NMOD_NONE. */
t_native_module native_module_named(const char *name, size_t len);

/* Position of name in a NULL-terminated list (names, kw), or -1. */
int             native_param(const char *const *list, const char *name,
                    size_t len);
int             native_param_count(const char *const *list);

/*
 * For a native with keyword-only parameters given too many positional
 * arguments: how to name them ("indent goes by name: ..."), or NULL.
 */
const char      *native_kw_hint(const t_native_def *def);

#endif
