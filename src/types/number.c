/* Methods of number (int and float); see docs/TYPES.md. */

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "runtime/interp_internal.h"

#define SELF    (in->stack[base])
#define ARG(i)  (in->stack[base + 1 + (i)])

/*
 * A float made into an int (round, floor, ceil): nan has no int, and
 * anything out of the 64-bit range does not fit.
 */
static int  float_to_int(const t_node *call, double f, t_value *out,
                t_error *err)
{
    if (f != f)
        return (rt_error(err, ERR_VALUE, call, "cannot convert nan to an"
                " int"));
    if (f >= 9223372036854775808.0 || f < -9223372036854775808.0)
        return (rt_error(err, ERR_OVERFLOW, call, "%s is too large for an"
                " int", isinf(f) ? (f < 0 ? "-inf" : "inf") : "the number"));
    *out = val_int((int64_t)f);
    return (0);
}

/* b ** e for ints, e >= 0, by squaring: exact or OverflowError. */
static int  int_pow(const t_node *call, int64_t b, int64_t e, t_value *out,
                t_error *err)
{
    int64_t r;

    r = 1;
    while (e > 0)
    {
        if ((e & 1) && __builtin_mul_overflow(r, b, &r))
            return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
        e >>= 1;
        if (e > 0 && __builtin_mul_overflow(b, b, &b))
            return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
    }
    *out = val_int(r);
    return (0);
}

/*
 * x.pow(e): an int when both are ints and e >= 0; a float otherwise.
 * 0 to a negative power divides by zero; a negative base to a
 * fractional power has no real result; a float too large for a double
 * (from finite numbers) is an OverflowError, as an int too large.
 */
static int  m_pow(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    double  b;
    double  e;

    (void)nargs;
    if (ARG(0).type != VAL_INT && ARG(0).type != VAL_FLOAT)
        return (rt_error(err, ERR_TYPE, call, "pow() needs a number, not '%s'",
                val_type_name(ARG(0))));
    if (SELF.type == VAL_INT && ARG(0).type == VAL_INT && ARG(0).as.integer >= 0)
        return (int_pow(call, SELF.as.integer, ARG(0).as.integer, out, err));
    b = SELF.type == VAL_INT ? (double)SELF.as.integer : SELF.as.number;
    e = ARG(0).type == VAL_INT ? (double)ARG(0).as.integer : ARG(0).as.number;
    if (b == 0.0 && e < 0)
        return (rt_error(err, ERR_ZERO_DIVISION, call, "0 cannot be raised to"
                " a negative power"));
    if (b < 0 && e != floor(e))
        return (rt_error(err, ERR_VALUE, call, "a negative number cannot be"
                " raised to a fractional power"));
    *out = val_float(pow(b, e));
    if (isinf(out->as.number) && !isinf(b) && !isinf(e))
        return (rt_error(err, ERR_OVERFLOW, call, "pow(): the result is too"
                " large"));
    return (0);
}

static int  m_abs(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    if (SELF.type == VAL_FLOAT)
        *out = val_float(fabs(SELF.as.number));
    else if (SELF.as.integer == INT64_MIN)
        return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
    else
        *out = val_int(SELF.as.integer < 0 ? -SELF.as.integer
                : SELF.as.integer);
    return (0);
}

/*
 * round, floor, ceil and trunc give ints; round goes half away from zero
 * (2.5 -> 3, -2.5 -> -3).
 */
static int  rounded(t_interp *in, size_t base, const t_node *call,
                double (*fn)(double), t_value *out, t_error *err)
{
    if (SELF.type == VAL_INT)
    {
        *out = SELF;
        return (0);
    }
    return (float_to_int(call, fn(SELF.as.number), out, err));
}

/*
 * x rounded to digits decimals (negative: tens, hundreds...), half away
 * from zero, as a float. The scaling is done in long double, so a tie
 * that a float can hold (0.125) rounds up, while one it cannot hold
 * (2.675 is really 2.67499999...) goes where its true value says, as in
 * Python (in double, 2.675 * 100 would already be 267.5). From 2^52 on
 * a double has no decimals: x is already rounded.
 */
static double   round_digits(double x, int64_t digits)
{
    long double p;
    long double y;

    if (x != x || isinf(x) || digits > 308)
        return (x);
    if (digits < -308)
        return (0.0 * x);
    if (digits >= 0)
    {
        if (fabs(x) >= 4503599627370496.0)
            return (x);
        p = powl(10.0L, (long double)digits);
        y = (long double)x * p;
        return ((double)(roundl(y) / p));
    }
    p = powl(10.0L, (long double)-digits);
    return ((double)(roundl((long double)x / p) * p));
}

/* x.round(): an int; x.round(digits): a float with that many decimals. */
static int  m_round(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    double  x;

    if (nargs == 0)
        return (rounded(in, base, call, round, out, err));
    if (ARG(0).type != VAL_INT)
    {
        if (ARG(0).type == VAL_FLOAT)
            return (rt_error(err, ERR_TYPE, call, "round() needs an integer"
                    " number of decimals, not a float"));
        return (rt_error(err, ERR_TYPE, call, "round() needs an integer"
                " number of decimals, not '%s'", val_type_name(ARG(0))));
    }
    x = SELF.type == VAL_INT ? (double)SELF.as.integer : SELF.as.number;
    *out = val_float(round_digits(x, ARG(0).as.integer));
    return (0);
}

/* Toward zero: 2.7 -> 2, -2.7 -> -2. */
static int  m_trunc(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (rounded(in, base, call, trunc, out, err));
}

static int  m_floor(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (rounded(in, base, call, floor, out, err));
}

static int  m_ceil(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (rounded(in, base, call, ceil, out, err));
}

static int  m_sqrt(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    double  x;

    (void)nargs;
    x = SELF.type == VAL_INT ? (double)SELF.as.integer : SELF.as.number;
    if (x < 0)
        return (rt_error(err, ERR_VALUE, call, "the square root of a negative"
                " number is not a real number"));
    *out = val_float(sqrt(x));
    return (0);
}

static int  m_is_int(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_bool(SELF.type == VAL_INT);
    return (0);
}

const t_method  g_number_methods[] = {
    {"pow", 1, 1, m_pow, NULL},
    {"abs", 0, 0, m_abs, NULL},
    {"round", 0, 1, m_round, NULL},
    {"floor", 0, 0, m_floor, NULL},
    {"ceil", 0, 0, m_ceil, NULL},
    {"trunc", 0, 0, m_trunc, NULL},
    {"sqrt", 0, 0, m_sqrt, NULL},
    {"is_int", 0, 0, m_is_int, NULL},
    {NULL, 0, 0, NULL, NULL},
};

/*
 * The method name of number on x = stack[base], its other arguments
 * after it (nargs counts x): math.sqrt(x) is x.sqrt(). x must be a
 * number (math checks it).
 */
int rt_number_method(t_interp *in, const char *name, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int i;

    for (i = 0; g_number_methods[i].name; i++)
        if (strcmp(g_number_methods[i].name, name) == 0)
            return (g_number_methods[i].fn(in, base, nargs - 1, call, out,
                    err));
    return (rt_error(err, ERR_NOT_IMPLEMENTED, call, "%s() is missing", name));
}
