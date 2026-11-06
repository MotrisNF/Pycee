/* Module math: mathematics. */

#include <math.h>
#include <stdint.h>
#include "natives/modules.h"

/*
 * Every function takes numbers (int or float; a bool is not one). The
 * ones that are also methods of number (sqrt, abs, round, floor, ceil,
 * trunc, pow) are those methods: math.sqrt(x) is x.sqrt(). The rest give
 * floats, as Python's: a result out of the domain (a nan from numbers
 * that are not nan) is a ValueError, one too large (inf from finite
 * numbers) an OverflowError.
 */

t_value nconst_math_pi(t_interp *in)
{
    (void)in;
    return (val_float(M_PI));
}

t_value nconst_math_e(t_interp *in)
{
    (void)in;
    return (val_float(M_E));
}

t_value nconst_math_tau(t_interp *in)
{
    (void)in;
    return (val_float(2.0 * M_PI));
}

t_value nconst_math_inf(t_interp *in)
{
    (void)in;
    return (val_float(INFINITY));
}

t_value nconst_math_nan(t_interp *in)
{
    (void)in;
    return (val_float(NAN));
}

/* v as a double, or a TypeError naming fname. */
static int  num_arg(const t_node *call, t_value v, const char *fname,
                double *out, t_error *err)
{
    *out = 0.0;
    if (v.type == VAL_INT)
        *out = (double)v.as.integer;
    else if (v.type == VAL_FLOAT)
        *out = v.as.number;
    else
        return (rt_error(err, ERR_TYPE, call, "%s() needs a number, not '%s'",
                fname, val_type_name(v)));
    return (0);
}

/* x.name(...) once x is known to be a number (the method checks the rest). */
static int  as_method(t_interp *in, size_t base, int nargs,
                const t_node *call, const char *name, t_value *out,
                t_error *err)
{
    double  d;

    if (num_arg(call, ARG(0), name, &d, err) == -1)
        return (-1);
    return (rt_number_method(in, name, base, nargs, call, out, err));
}

/*
 * r, the result of fname on finite or not arguments (finite: all of
 * them were; nan_in: one was nan), checked as Python does.
 */
static int  checked(const t_node *call, const char *fname, double r,
                int finite, int nan_in, t_value *out, t_error *err)
{
    if (r != r && !nan_in)
        return (rt_error(err, ERR_VALUE, call, "%s(): math domain error",
                fname));
    if (isinf(r) && finite)
        return (rt_error(err, ERR_OVERFLOW, call, "%s(): the result is too"
                " large", fname));
    *out = val_float(r);
    return (0);
}

static int  apply1(t_interp *in, size_t base, const t_node *call,
                const char *fname, double (*fn)(double), t_value *out,
                t_error *err)
{
    double  x;

    if (num_arg(call, ARG(0), fname, &x, err) == -1)
        return (-1);
    return (checked(call, fname, fn(x), isfinite(x), x != x, out, err));
}

static int  apply2(t_interp *in, size_t base, const t_node *call,
                const char *fname, double (*fn)(double, double), t_value *out,
                t_error *err)
{
    double  x;
    double  y;

    if (num_arg(call, ARG(0), fname, &x, err) == -1
        || num_arg(call, ARG(1), fname, &y, err) == -1)
        return (-1);
    return (checked(call, fname, fn(x, y), isfinite(x) && isfinite(y),
            x != x || y != y, out, err));
}

/* One line per function that is a method of number. */
#define AS_METHOD(name) \
    int nat_math_##name(t_interp *in, size_t base, int nargs, \
        const t_node *call, t_value *out, t_error *err) \
    { \
        return (as_method(in, base, nargs, call, #name, out, err)); \
    }

AS_METHOD(sqrt)
AS_METHOD(abs)
AS_METHOD(round)
AS_METHOD(floor)
AS_METHOD(ceil)
AS_METHOD(trunc)
AS_METHOD(pow)

/* One line per function of one or two floats, with the C function it is. */
#define APPLY1(name, fn) \
    int nat_math_##name(t_interp *in, size_t base, int nargs, \
        const t_node *call, t_value *out, t_error *err) \
    { \
        (void)nargs; \
        return (apply1(in, base, call, #name, fn, out, err)); \
    }
#define APPLY2(name, fn) \
    int nat_math_##name(t_interp *in, size_t base, int nargs, \
        const t_node *call, t_value *out, t_error *err) \
    { \
        (void)nargs; \
        return (apply2(in, base, call, #name, fn, out, err)); \
    }

static double   to_degrees(double x)
{
    return (x * (180.0 / M_PI));
}

static double   to_radians(double x)
{
    return (x * (M_PI / 180.0));
}

APPLY1(exp, exp)
APPLY1(log2, log2)
APPLY1(log10, log10)
APPLY1(sin, sin)
APPLY1(cos, cos)
APPLY1(tan, tan)
APPLY1(asin, asin)
APPLY1(acos, acos)
APPLY1(atan, atan)
APPLY1(sinh, sinh)
APPLY1(cosh, cosh)
APPLY1(tanh, tanh)
APPLY1(degrees, to_degrees)
APPLY1(radians, to_radians)
APPLY2(atan2, atan2)
APPLY2(hypot, hypot)

/*
 * log(x): natural logarithm; log(x, base). x must be > 0 (log(0) would
 * be -inf: a domain error, as in Python), base > 0 and not 1.
 */
int nat_math_log(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    double  x;
    double  b;

    if (num_arg(call, ARG(0), "log", &x, err) == -1)
        return (-1);
    if (x <= 0)
        return (rt_error(err, ERR_VALUE, call, "log() needs a number > 0"));
    if (nargs == 1)
        return (checked(call, "log", log(x), isfinite(x), x != x, out, err));
    if (num_arg(call, ARG(1), "log", &b, err) == -1)
        return (-1);
    if (b <= 0 || b == 1)
        return (rt_error(err, ERR_VALUE, call, "log() needs a base > 0 and"
                " other than 1"));
    return (checked(call, "log", log(x) / log(b), isfinite(x) && isfinite(b),
            x != x || b != b, out, err));
}

/* An int argument of gcd, lcm or factorial (a float is a TypeError). */
static int  int_arg(const t_node *call, t_value v, const char *fname,
                int64_t *out, t_error *err)
{
    return (rt_int_arg(call, v, fname, "argument", out, err));
}

/* |a| as an unsigned (|INT64_MIN| fits there). */
static uint64_t uabs(int64_t a)
{
    return (a < 0 ? (uint64_t)0 - (uint64_t)a : (uint64_t)a);
}

static uint64_t ugcd(uint64_t a, uint64_t b)
{
    uint64_t    t;

    while (b)
    {
        t = a % b;
        a = b;
        b = t;
    }
    return (a);
}

/* An unsigned result that must fit in an int. */
static int  fit(const t_node *call, uint64_t r, t_value *out, t_error *err)
{
    if (r > (uint64_t)INT64_MAX)
        return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
    *out = val_int((int64_t)r);
    return (0);
}

/* gcd(a, b, ...): the greatest common divisor, >= 0 (gcd(0, 0) is 0). */
int nat_math_gcd(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    uint64_t    r;
    int64_t     a;
    int         i;

    r = 0;
    for (i = 0; i < nargs; i++)
    {
        if (int_arg(call, ARG(i), "gcd", &a, err) == -1)
            return (-1);
        r = ugcd(r, uabs(a));
    }
    return (fit(call, r, out, err));
}

/* lcm(a, b, ...): the least common multiple, >= 0 (0 if any is 0). */
int nat_math_lcm(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    uint64_t    r;
    uint64_t    u;
    int64_t     a;
    int         i;

    r = 1;
    for (i = 0; i < nargs; i++)
    {
        if (int_arg(call, ARG(i), "lcm", &a, err) == -1)
            return (-1);
        u = uabs(a);
        if (u == 0 || r == 0)
            r = 0;
        else if (__builtin_mul_overflow(r / ugcd(r, u), u, &r)
            || r > (uint64_t)INT64_MAX)
            return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
    }
    return (fit(call, r, out, err));
}

/* n! for an int n >= 0; 21! no longer fits. */
int nat_math_factorial(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int64_t n;
    int64_t r;

    (void)nargs;
    if (int_arg(call, ARG(0), "factorial", &n, err) == -1)
        return (-1);
    if (n < 0)
        return (rt_error(err, ERR_VALUE, call, "factorial() needs an integer"
                " >= 0, not %lld", (long long)n));
    r = 1;
    while (n > 1)
        if (__builtin_mul_overflow(r, n--, &r))
            return (rt_error(err, ERR_OVERFLOW, call, "integer overflow"));
    *out = val_int(r);
    return (0);
}

/* is_nan, is_inf, is_finite: an int is never nan or inf. */
static int  classify(t_interp *in, size_t base, const t_node *call,
                const char *fname, int which, t_value *out, t_error *err)
{
    double  x;

    if (num_arg(call, ARG(0), fname, &x, err) == -1)
        return (-1);
    if (which == 0)
        *out = val_bool(x != x);
    else if (which == 1)
        *out = val_bool(isinf(x) != 0);
    else
        *out = val_bool(isfinite(x) != 0);
    return (0);
}

int nat_math_is_nan(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    return (classify(in, base, call, "is_nan", 0, out, err));
}

int nat_math_is_inf(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    (void)nargs;
    return (classify(in, base, call, "is_inf", 1, out, err));
}

int nat_math_is_finite(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (classify(in, base, call, "is_finite", 2, out, err));
}
