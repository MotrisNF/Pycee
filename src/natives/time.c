/* Module time: clocks and waiting. */

#include <errno.h>
#include <math.h>
#include <time.h>
#include "natives/modules.h"

static double   seconds(clockid_t clock)
{
    struct timespec ts;

    clock_gettime(clock, &ts);
    return ((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
}

/* Seconds since 1970-01-01 00:00 UTC, as a float (the wall clock). */
int nat_time_now(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)in;
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_float(seconds(CLOCK_REALTIME));
    return (0);
}

/*
 * Seconds from some fixed moment, as a float: it never goes back (the
 * wall clock can), so the difference of two is a duration.
 */
int nat_time_clock(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)in;
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_float(seconds(CLOCK_MONOTONIC));
    return (0);
}

/*
 * sleep(s): waits s seconds (a number >= 0, fractions too). A signal
 * that comes meanwhile is attended at once (its handler runs, or Ctrl+C
 * is a KeyboardInterrupt), then the rest of the wait goes on.
 */
int nat_time_sleep(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    struct timespec req;
    struct timespec rem;
    double          s;

    (void)nargs;
    if (ARG(0).type != VAL_INT && ARG(0).type != VAL_FLOAT)
        return (rt_error(err, ERR_TYPE, call, "sleep() needs a number of"
                " seconds, not '%s'", val_type_name(ARG(0))));
    s = ARG(0).type == VAL_INT ? (double)ARG(0).as.integer : ARG(0).as.number;
    if (s != s || s < 0)
        return (rt_error(err, ERR_VALUE, call, "sleep() needs a number of"
                " seconds >= 0"));
    if (s > 1e15)
        s = 1e15;
    req.tv_sec = (time_t)s;
    req.tv_nsec = (long)((s - floor(s)) * 1e9);
    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
    {
        if (rt_interrupted(in, call, err) == -1)
            return (-1);
        req = rem;
    }
    *out = val_none();
    return (0);
}
