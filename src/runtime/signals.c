/*
 * Signals. The handler in C only notes that a signal came (it may arrive
 * in the middle of anything, even of malloc, so it touches nothing
 * else); the program's own handler runs later, at a safe point: before
 * each statement and each call (rt_poll), or when a wait that the signal
 * interrupted (input(), sleep(), a read) sees EINTR. SIGINT without a
 * handler of the program is a KeyboardInterrupt there.
 *
 * Handlers are installed without SA_RESTART, so a blocking read or wait
 * returns with EINTR and the signal is attended at once; every place
 * that waits retries after it (rt_interrupted).
 */

#include <signal.h>
#include <string.h>
#include "runtime/interp_internal.h"

volatile sig_atomic_t           g_signal_pending;
static volatile sig_atomic_t    g_pending[RT_NSIG];

static void on_signal(int sig)
{
    if (sig > 0 && sig < RT_NSIG)
        g_pending[sig] = 1;
    g_signal_pending = 1;
}

static void install(int sig, void (*fn)(int))
{
    struct sigaction    sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, NULL);
}

/* At the start: Ctrl+C becomes a KeyboardInterrupt instead of killing. */
void    interp_signals_init(void)
{
    int i;

    for (i = 0; i < RT_NSIG; i++)
        g_pending[i] = 0;
    g_signal_pending = 0;
    install(SIGINT, on_signal);
}

/* Forgets what came and was not attended (the REPL, at its prompt). */
void    interp_signals_clear(void)
{
    int i;

    for (i = 0; i < RT_NSIG; i++)
        g_pending[i] = 0;
    g_signal_pending = 0;
}

/* f runs when sig comes (signal.handle). */
void    rt_signal_handle(t_interp *in, int sig, t_value f)
{
    in->sig_handlers[sig] = f;
    in->sig_handled[sig] = 1;
    install(sig, on_signal);
}

/*
 * signal.ignore (how 1) or signal.reset (how 0): no handler of the
 * program any more. Reset is what the language does by default: SIGINT
 * is a KeyboardInterrupt again, any other one does what the system does.
 */
void    rt_signal_unhandle(t_interp *in, int sig, int ignore)
{
    in->sig_handlers[sig] = val_none();
    in->sig_handled[sig] = 0;
    g_pending[sig] = 0;
    if (ignore)
        install(sig, SIG_IGN);
    else if (sig == SIGINT)
        install(sig, on_signal);
    else
        install(sig, SIG_DFL);
}

static void still_pending(void)
{
    int i;

    for (i = 1; i < RT_NSIG; i++)
        if (g_pending[i])
            g_signal_pending = 1;
}

/*
 * Attends the signals that came: each one's handler is called with its
 * number (at at, for errors and the traceback); a SIGINT without one is
 * a KeyboardInterrupt. An error stops there, leaving the rest pending.
 */
int rt_poll_signals(t_interp *in, const t_node *at, t_error *err)
{
    size_t  base;
    t_value v;
    int     sig;

    g_signal_pending = 0;
    for (sig = 1; sig < RT_NSIG; sig++)
    {
        if (!g_pending[sig])
            continue ;
        g_pending[sig] = 0;
        if (in->sig_handled[sig])
        {
            base = in->top;
            rt_push(in, in->sig_handlers[sig]);
            rt_push(in, val_int(sig));
            if (rt_call(in, at, base, 1, &v, err) == -1)
            {
                in->top = base;
                still_pending();
                return (-1);
            }
            in->top = base;
        }
        else if (sig == SIGINT)
        {
            still_pending();
            return (rt_error(err, ERR_KEYBOARD_INTERRUPT, at, "%s", ""));
        }
    }
    return (0);
}

/*
 * A wait or a read stopped by a signal (EINTR): the signal is attended
 * now. -1 if that raised an error (a KeyboardInterrupt, or one of a
 * handler), 0 to go on waiting.
 */
int rt_interrupted(t_interp *in, const t_node *at, t_error *err)
{
    if (!g_signal_pending)
        return (0);
    return (rt_poll_signals(in, at, err));
}
