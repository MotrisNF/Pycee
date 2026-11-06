/*
 * Module signal: signals and their handlers. A handler is any function
 * taking the signal's number; it runs at the next safe point, never
 * inside the signal itself (runtime/signals.c).
 */

#include <signal.h>
#include "natives/modules.h"

t_value nconst_signal_SIGINT(t_interp *in)
{
    (void)in;
    return (val_int(SIGINT));
}

t_value nconst_signal_SIGTERM(t_interp *in)
{
    (void)in;
    return (val_int(SIGTERM));
}

t_value nconst_signal_SIGHUP(t_interp *in)
{
    (void)in;
    return (val_int(SIGHUP));
}

t_value nconst_signal_SIGQUIT(t_interp *in)
{
    (void)in;
    return (val_int(SIGQUIT));
}

t_value nconst_signal_SIGUSR1(t_interp *in)
{
    (void)in;
    return (val_int(SIGUSR1));
}

t_value nconst_signal_SIGUSR2(t_interp *in)
{
    (void)in;
    return (val_int(SIGUSR2));
}

t_value nconst_signal_SIGALRM(t_interp *in)
{
    (void)in;
    return (val_int(SIGALRM));
}

t_value nconst_signal_SIGPIPE(t_interp *in)
{
    (void)in;
    return (val_int(SIGPIPE));
}

t_value nconst_signal_SIGCHLD(t_interp *in)
{
    (void)in;
    return (val_int(SIGCHLD));
}

static int  allowed(int64_t sig)
{
    return (sig == SIGINT || sig == SIGTERM || sig == SIGHUP
        || sig == SIGQUIT || sig == SIGUSR1 || sig == SIGUSR2
        || sig == SIGALRM || sig == SIGPIPE || sig == SIGCHLD);
}

/*
 * ARG(0), one of the signals the module offers. SIGKILL and SIGSTOP
 * cannot be handled by anyone; SIGSEGV, SIGFPE, SIGBUS and SIGILL are
 * not allowed: going on after one would break memory safety.
 */
static int  sig_arg(t_interp *in, size_t base, const t_node *call,
                const char *fname, int *out, t_error *err)
{
    int64_t sig;

    if (rt_int_arg(call, ARG(0), fname, "signal", &sig, err) == -1)
        return (-1);
    if (sig == SIGKILL || sig == SIGSTOP)
        return (rt_error(err, ERR_VALUE, call, "signal %lld (%s) cannot be"
                " handled", (long long)sig, sig == SIGKILL ? "SIGKILL"
                : "SIGSTOP"));
    if (sig == SIGSEGV || sig == SIGFPE || sig == SIGBUS || sig == SIGILL)
        return (rt_error(err, ERR_VALUE, call, "signal %lld is not allowed:"
                " going on after it would not be memory safe",
                (long long)sig));
    if (!allowed(sig))
        return (rt_error(err, ERR_VALUE, call, "%lld is not one of the"
                " signals of the module (SIGINT, SIGTERM, SIGHUP, SIGQUIT,"
                " SIGUSR1, SIGUSR2, SIGALRM, SIGPIPE, SIGCHLD)",
                (long long)sig));
    *out = (int)sig;
    return (0);
}

/*
 * handle(sig, f): from now on f(sig) runs when sig comes. For SIGINT,
 * Ctrl+C then calls f instead of being a KeyboardInterrupt.
 */
int nat_signal_handle(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int sig;

    (void)nargs;
    if (sig_arg(in, base, call, "handle", &sig, err) == -1)
        return (-1);
    if (!val_is_obj(ARG(1), OBJ_FUNCTION) && !val_is_obj(ARG(1), OBJ_NATIVE)
        && !val_is_obj(ARG(1), OBJ_BOUND) && !val_is_obj(ARG(1), OBJ_METHOD))
        return (rt_error(err, ERR_TYPE, call, "handle() needs a function to"
                " call, not '%s'", val_type_name(ARG(1))));
    rt_signal_handle(in, sig, ARG(1));
    *out = val_none();
    return (0);
}

/* ignore(sig): sig does nothing from now on (Ctrl+C neither). */
int nat_signal_ignore(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int sig;

    (void)nargs;
    if (sig_arg(in, base, call, "ignore", &sig, err) == -1)
        return (-1);
    rt_signal_unhandle(in, sig, 1);
    *out = val_none();
    return (0);
}

/*
 * reset(sig): what it did at the start: SIGINT is a KeyboardInterrupt;
 * any other one does what the system does (most end the program).
 */
int nat_signal_reset(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int sig;

    (void)nargs;
    if (sig_arg(in, base, call, "reset", &sig, err) == -1)
        return (-1);
    rt_signal_unhandle(in, sig, 0);
    *out = val_none();
    return (0);
}

/*
 * send(sig): sends sig to the program itself; it is attended before
 * send returns (its handler runs, or SIGINT is a KeyboardInterrupt).
 */
int nat_signal_send(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int sig;

    (void)nargs;
    if (sig_arg(in, base, call, "send", &sig, err) == -1)
        return (-1);
    raise(sig);
    *out = val_none();
    return (rt_interrupted(in, call, err));
}
