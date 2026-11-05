/* Unary and binary operators (docs/TYPES.md: number, string, bool). */

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "core/stack.h"
#include "runtime/interp_internal.h"

static int  is_number(t_value v)
{
    return (v.type == VAL_INT || v.type == VAL_FLOAT);
}

static double   as_double(t_value v)
{
    if (v.type == VAL_INT)
        return ((double)v.as.integer);
    return (v.as.number);
}

/*
 * Exact comparison of an int with a float: -1, 0 or 1, and 2 when f is
 * nan (unordered). Converting i to double would be wrong past 2^53.
 */
static int  cmp_int_float(int64_t i, double f)
{
    int64_t t;
    double  frac;

    if (f != f)
        return (2);
    if (f >= 9223372036854775808.0)
        return (-1);
    if (f < -9223372036854775808.0)
        return (1);
    t = (int64_t)f;
    if (i != t)
        return (i < t ? -1 : 1);
    frac = f - (double)t;
    if (frac == 0.0)
        return (0);
    return (frac > 0.0 ? -1 : 1);
}

/* -1, 0, 1, or 2 when unordered (a nan is involved). */
static int  cmp_numbers(t_value a, t_value b)
{
    int r;

    if (a.type == VAL_INT && b.type == VAL_INT)
        return (a.as.integer < b.as.integer ? -1
            : a.as.integer > b.as.integer);
    if (a.type == VAL_INT)
        return (cmp_int_float(a.as.integer, b.as.number));
    if (b.type == VAL_INT)
    {
        r = cmp_int_float(b.as.integer, a.as.number);
        return (r == 2 ? 2 : -r);
    }
    if (a.as.number != a.as.number || b.as.number != b.as.number)
        return (2);
    return (a.as.number < b.as.number ? -1 : a.as.number > b.as.number);
}

/*
 * Two ranges are equal when they give the same numbers, as in Python:
 * range(0) is range(5, 5), range(0, 3) is range(0, 3, 1).
 */
static int  range_equal(const t_range *a, const t_range *b)
{
    uint64_t    len;

    len = rt_range_len(a);
    if (len != rt_range_len(b))
        return (0);
    if (len == 0)
        return (1);
    return (a->start == b->start && (len == 1 || a->step == b->step));
}

/*
 * Two tuples, or two lists: item by item; -1 if the nesting is deeper
 * than the stack allows (a list that contains itself ends there too).
 */
static int  items_equal(t_value a, t_value b)
{
    const t_value   *x;
    const t_value   *y;
    size_t          lx;
    size_t          ly;
    size_t          i;
    int             r;

    x = rt_seq_items(a, &lx);
    y = rt_seq_items(b, &ly);
    if (lx != ly)
        return (0);
    if (stack_exhausted())
        return (-1);
    for (i = 0; i < lx; i++)
    {
        r = rt_equal(x[i], y[i]);
        if (r != 1)
            return (r);
    }
    return (1);
}

/*
 * Functions are the same when they would do the same: one declaration
 * with the same captured cells (each use of a func's name makes a new
 * value, but it is the same function), one native, one method on the
 * same value.
 */
static int  function_equal(t_value a, t_value b)
{
    const t_function    *f;
    const t_function    *g;
    size_t              i;

    if (a.as.obj->type != b.as.obj->type)
        return (0);
    if (val_is_obj(a, OBJ_NATIVE))
        return (((const t_native *)a.as.obj)->slot
            == ((const t_native *)b.as.obj)->slot);
    if (val_is_obj(a, OBJ_BOUND))
        return (((const t_bound *)a.as.obj)->method
            == ((const t_bound *)b.as.obj)->method
            && rt_equal(((const t_bound *)a.as.obj)->self,
                ((const t_bound *)b.as.obj)->self) == 1);
    f = (const t_function *)a.as.obj;
    g = (const t_function *)b.as.obj;
    if (f->decl != g->decl || f->ncells != g->ncells)
        return (0);
    for (i = 0; i < f->ncells; i++)
        if (f->cells[i] != g->cells[i])
            return (0);
    return (1);
}

static int  is_function(t_value v)
{
    return (val_is_obj(v, OBJ_FUNCTION) || val_is_obj(v, OBJ_NATIVE)
        || val_is_obj(v, OBJ_BOUND));
}

/*
 * Two dicts (or two sets): the same keys, each with an equal value; the
 * order does not matter. -1 when it goes deeper than the stack allows.
 */
static int  tables_equal(const t_dict *a, const t_dict *b)
{
    size_t  i;
    size_t  pos;
    int     r;

    if (a->len != b->len)
        return (0);
    if (stack_exhausted())
        return (-1);
    for (i = 0; i < a->nentries; i++)
    {
        if (!a->entries[i].used)
            continue ;
        r = rt_table_find(b, a->entries[i].key, a->entries[i].hash, &pos);
        if (r != 1)
            return (r);
        r = rt_equal(a->entries[i].value, b->entries[pos].value);
        if (r != 1)
            return (r);
    }
    return (1);
}

/*
 * is: by value; different types are never equal; 1 is 1.0. Returns 1 or
 * 0, or -1 when tuples nest deeper than the stack allows (the caller
 * reports a RecursionError).
 */
int rt_equal(t_value a, t_value b)
{
    t_string    *x;
    t_string    *y;

    if (is_number(a) && is_number(b))
        return (cmp_numbers(a, b) == 0);
    if (a.type != b.type)
        return (0);
    if (a.type == VAL_NONE)
        return (1);
    if (a.type == VAL_BOOL)
        return (a.as.boolean == b.as.boolean);
    if (a.as.obj == b.as.obj)
        return (1);
    if (val_is_range(a) && val_is_range(b))
        return (range_equal(val_range(a), val_range(b)));
    if ((val_is_tuple(a) && val_is_tuple(b))
        || (val_is_list(a) && val_is_list(b)))
        return (items_equal(a, b));
    if (is_function(a) && is_function(b))
        return (function_equal(a, b));
    if ((val_is_dict(a) && val_is_dict(b)) || (val_is_set(a) && val_is_set(b)))
        return (tables_equal(val_dict(a), val_dict(b)));
    if (!val_is_string(a) || !val_is_string(b))
        return (0);
    x = val_string(a);
    y = val_string(b);
    return (x->len == y->len && memcmp(x->chars, y->chars, x->len) == 0);
}

/* Conditions, and/or/xor/not only take bools. what: "a condition"... */
int rt_truthy(const t_node *n, t_value v, const char *what, int *out,
        t_error *err)
{
    if (v.type != VAL_BOOL)
        return (rt_error(err, ERR_TYPE, n, "%s needs a bool, not '%s'", what,
                val_type_name(v)));
    *out = v.as.boolean;
    return (0);
}

int rt_unary(t_interp *in, const t_node *n, t_value v, t_value *out,
        t_error *err)
{
    t_token_type    op;
    int             b;

    (void)in;
    b = 0;
    op = n->as.unary.op;
    if (op == TOK_NOT)
    {
        if (rt_truthy(n, v, "'not'", &b, err) == -1)
            return (-1);
        *out = val_bool(!b);
        return (0);
    }
    if (op == TOK_MINUS && v.type == VAL_INT)
    {
        if (v.as.integer == INT64_MIN)
            return (rt_error(err, ERR_OVERFLOW, n, "integer overflow"));
        *out = val_int(-v.as.integer);
        return (0);
    }
    if (op == TOK_MINUS && v.type == VAL_FLOAT)
    {
        *out = val_float(-v.as.number);
        return (0);
    }
    if (op == TOK_TILDE && v.type == VAL_INT)
    {
        *out = val_int(~v.as.integer);
        return (0);
    }
    return (rt_error(err, ERR_TYPE, n, "bad operand type for unary %s: '%s'",
            token_type_desc(op), val_type_name(v)));
}

static int  type_error(const t_node *n, t_token_type op, t_value a,
                t_value b, t_error *err)
{
    return (rt_error(err, ERR_TYPE, n, "unsupported operand types for %s:"
            " '%s' and '%s'", token_type_desc(op), val_type_name(a),
            val_type_name(b)));
}

/* + - * on two ints: exact, or OverflowError. */
static int  int_arith(const t_node *n, t_token_type op, int64_t a,
                int64_t b, t_value *out, t_error *err)
{
    int64_t r;
    int     overflow;

    if (op == TOK_PLUS)
        overflow = __builtin_add_overflow(a, b, &r);
    else if (op == TOK_MINUS)
        overflow = __builtin_sub_overflow(a, b, &r);
    else
        overflow = __builtin_mul_overflow(a, b, &r);
    if (overflow)
        return (rt_error(err, ERR_OVERFLOW, n, "integer overflow"));
    *out = val_int(r);
    return (0);
}

/*
 * // and % on two ints: truncated, as in C. INT64_MIN // -1 does not fit;
 * INT64_MIN % -1 is 0 (in C both are undefined behaviour).
 */
static int  int_divmod(const t_node *n, t_token_type op, int64_t a,
                int64_t b, t_value *out, t_error *err)
{
    if (b == 0)
        return (rt_error(err, ERR_ZERO_DIVISION, n, op == TOK_PERCENT
                ? "modulo by zero" : "division by zero"));
    if (op == TOK_SLASH_SLASH && a == INT64_MIN && b == -1)
        return (rt_error(err, ERR_OVERFLOW, n, "integer overflow"));
    if (op == TOK_PERCENT)
        *out = val_int(b == -1 ? 0 : a % b);
    else
        *out = val_int(a / b);
    return (0);
}

/* Shifts: the count must be >= 0; << that loses bits is an overflow. */
static int  int_shift(const t_node *n, t_token_type op, int64_t a,
                int64_t count, t_value *out, t_error *err)
{
    int64_t r;

    if (count < 0)
        return (rt_error(err, ERR_VALUE, n, "negative shift count"));
    if (op == TOK_SHR)
    {
        if (count >= 64)
            *out = val_int(a < 0 ? -1 : 0);
        else
            *out = val_int(a < 0 ? ~(~a >> count) : a >> count);
        return (0);
    }
    if (a == 0)
        r = 0;
    else if (count == 63 && a == -1)
        r = INT64_MIN;
    else if (count >= 63
        || __builtin_mul_overflow(a, (int64_t)1 << count, &r))
        return (rt_error(err, ERR_OVERFLOW, n, "integer overflow"));
    *out = val_int(r);
    return (0);
}

static int  int_bits(const t_node *n, t_token_type op, int64_t a, int64_t b,
                t_value *out, t_error *err)
{
    if (op == TOK_AMP)
        *out = val_int(a & b);
    else if (op == TOK_PIPE)
        *out = val_int(a | b);
    else if (op == TOK_CARET)
        *out = val_int(a ^ b);
    else
        return (int_shift(n, op, a, b, out, err));
    return (0);
}

/* Any float involved: the result is a float; / // % by 0 are errors. */
static int  float_arith(const t_node *n, t_token_type op, double a,
                double b, t_value *out, t_error *err)
{
    if ((op == TOK_SLASH || op == TOK_SLASH_SLASH || op == TOK_PERCENT)
        && b == 0.0)
        return (rt_error(err, ERR_ZERO_DIVISION, n, op == TOK_PERCENT
                ? "modulo by zero" : "division by zero"));
    if (op == TOK_PLUS)
        *out = val_float(a + b);
    else if (op == TOK_MINUS)
        *out = val_float(a - b);
    else if (op == TOK_STAR)
        *out = val_float(a * b);
    else if (op == TOK_SLASH)
        *out = val_float(a / b);
    else if (op == TOK_SLASH_SLASH)
        *out = val_float(trunc(a / b));
    else
        *out = val_float(fmod(a, b));
    return (0);
}

static int  number_op(const t_node *n, t_token_type op, t_value a, t_value b,
                t_value *out, t_error *err)
{
    int both_int;

    both_int = (a.type == VAL_INT && b.type == VAL_INT);
    if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET || op == TOK_SHL
        || op == TOK_SHR)
    {
        if (!both_int)
            return (rt_error(err, ERR_TYPE, n, "%s needs integers, not a"
                    " float", token_type_desc(op)));
        return (int_bits(n, op, a.as.integer, b.as.integer, out, err));
    }
    if (both_int && (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR))
        return (int_arith(n, op, a.as.integer, b.as.integer, out, err));
    if (both_int && (op == TOK_SLASH_SLASH || op == TOK_PERCENT))
        return (int_divmod(n, op, a.as.integer, b.as.integer, out, err));
    return (float_arith(n, op, as_double(a), as_double(b), out, err));
}

/* "ab" * 3 and 3 * "ab": the count is an int >= 0. */
static int  repeat(t_interp *in, const t_node *n, t_string *s, t_value count,
                t_value *out, t_error *err)
{
    t_string    *r;
    size_t      times;
    size_t      i;

    if (count.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, n, "a string can only be repeated an"
                " integer number of times"));
    if (count.as.integer < 0)
        return (rt_error(err, ERR_VALUE, n, "cannot repeat a string a"
                " negative number of times"));
    times = (size_t)count.as.integer;
    if (s->len && times > SIZE_MAX / s->len)
        fatal_oom();
    r = heap_string_alloc(&in->heap, s->len * times);
    for (i = 0; i < times; i++)
        memcpy(r->chars + i * s->len, s->chars, s->len);
    *out = val_obj(&r->obj);
    return (0);
}

/* [1, 2] * 3: the items again and again, in a new list. */
static int  repeat_list(t_interp *in, const t_node *n, t_value lv,
                t_value count, t_value *out, t_error *err)
{
    t_list  *r;
    size_t  times;
    size_t  len;
    size_t  i;

    if (count.type != VAL_INT)
        return (rt_error(err, ERR_TYPE, n, "a list can only be repeated an"
                " integer number of times"));
    if (count.as.integer < 0)
        return (rt_error(err, ERR_VALUE, n, "cannot repeat a list a"
                " negative number of times"));
    times = (size_t)count.as.integer;
    len = val_list(lv)->len;
    if (len && times > SIZE_MAX / sizeof(t_value) / len)
        fatal_oom();
    r = heap_list(&in->heap, len * times);
    for (i = 0; i < len * times; i++)
        r->items[i] = val_list(lv)->items[i % len];
    r->len = len * times;
    *out = val_obj(&r->obj);
    return (0);
}

/* a + b: a new list with the items of both (the operands stay rooted). */
static int  concat_lists(t_interp *in, t_value a, t_value b, t_value *out)
{
    t_list  *r;
    size_t  i;

    r = heap_list(&in->heap, val_list(a)->len + val_list(b)->len);
    for (i = 0; i < val_list(a)->len; i++)
        r->items[r->len++] = val_list(a)->items[i];
    for (i = 0; i < val_list(b)->len; i++)
        r->items[r->len++] = val_list(b)->items[i];
    *out = val_obj(&r->obj);
    return (0);
}

static int  concat(t_interp *in, t_string *a, t_string *b, t_value *out)
{
    t_string    *r;

    if (a->len > SIZE_MAX - b->len)
        fatal_oom();
    r = heap_string_alloc(&in->heap, a->len + b->len);
    memcpy(r->chars, a->chars, a->len);
    memcpy(r->chars + a->len, b->chars, b->len);
    *out = val_obj(&r->obj);
    return (0);
}

/*
 * a.compare(b), a an object whose obj has that method: the sign of the
 * number it gives back (a and b stay rooted on the stack meanwhile).
 */
static int  user_compare(t_interp *in, const t_node *n, t_value a,
                t_value b, const t_node *f, int *out, t_error *err)
{
    t_value r;
    size_t  top;
    int     status;

    top = in->top;
    rt_push(in, a);
    rt_push(in, b);
    status = rt_call_method_func(in, n, f, &a, top + 1, 1, NULL, &r, err);
    in->top = top;
    if (status == -1)
        return (-1);
    if (r.type == VAL_INT)
        *out = (r.as.integer > 0) - (r.as.integer < 0);
    else if (r.type == VAL_FLOAT)
        *out = (r.as.number > 0) - (r.as.number < 0);
    else
        return (rt_error(err, ERR_TYPE, n, "compare() must give back a"
                " number, not '%s'", val_type_name(r)));
    return (0);
}

/* The compare method of an object's obj, or NULL. */
static const t_node *compare_method(t_value v)
{
    const t_node    *f;
    t_name          name;

    if (!val_is_obj(v, OBJ_INSTANCE))
        return (NULL);
    name.ptr = "compare";
    name.len = 7;
    f = rt_obj_find(((const t_instance *)v.as.obj)->cls, name);
    if (f && f->type == NODE_FUNC && !f->as.func.is_static)
        return (f);
    return (NULL);
}

/*
 * a against b for < <= > >=, sort, min and max: -1, 0 or 1 in *out.
 * Numbers with numbers (a nan counts as equal to anything), strings with
 * strings in byte order, an object with its compare(other) (on either
 * side); anything else is a TypeError.
 */
int rt_compare(t_interp *in, const t_node *n, t_value a, t_value b,
        int *out, t_error *err)
{
    t_string    *x;
    t_string    *y;
    size_t      len;
    int         c;

    if (compare_method(a))
        return (user_compare(in, n, a, b, compare_method(a), out, err));
    if (compare_method(b))
    {
        if (user_compare(in, n, b, a, compare_method(b), out, err) == -1)
            return (-1);
        *out = -*out;
        return (0);
    }
    if (is_number(a) && is_number(b))
    {
        c = cmp_numbers(a, b);
        *out = c == 2 ? 0 : c;
        return (0);
    }
    if (!val_is_string(a) || !val_is_string(b))
        return (rt_error(err, ERR_TYPE, n, "cannot compare '%s' with '%s'",
                val_type_name(a), val_type_name(b)));
    x = val_string(a);
    y = val_string(b);
    len = x->len < y->len ? x->len : y->len;
    c = memcmp(x->chars, y->chars, len);
    if (c == 0)
        c = (x->len > y->len) - (x->len < y->len);
    *out = (c > 0) - (c < 0);
    return (0);
}

static int  compare(const t_node *n, t_token_type op, t_value a, t_value b,
                t_value *out, t_error *err)
{
    t_string    *x;
    t_string    *y;
    size_t      len;
    int         c;

    if (is_number(a) && is_number(b))
        c = cmp_numbers(a, b);
    else if (val_is_string(a) && val_is_string(b))
    {
        x = val_string(a);
        y = val_string(b);
        len = x->len < y->len ? x->len : y->len;
        c = memcmp(x->chars, y->chars, len);
        if (c == 0)
            c = (x->len > y->len) - (x->len < y->len);
        c = (c > 0) - (c < 0);
    }
    else
        return (rt_error(err, ERR_TYPE, n, "%s is not supported between '%s'"
                " and '%s'", token_type_desc(op), val_type_name(a),
                val_type_name(b)));
    if (c == 2)
        *out = val_bool(0);
    else if (op == TOK_LT)
        *out = val_bool(c < 0);
    else if (op == TOK_LE)
        *out = val_bool(c <= 0);
    else if (op == TOK_GT)
        *out = val_bool(c > 0);
    else
        *out = val_bool(c >= 0);
    return (0);
}

/* v in r: a number is in a range when one of its steps lands on it. */
static int  in_range(t_value v, const t_range *r)
{
    int64_t     i;
    uint64_t    dist;

    if (v.type == VAL_FLOAT)
    {
        if (v.as.number != v.as.number || v.as.number < -9223372036854775808.0
            || v.as.number >= 9223372036854775808.0
            || v.as.number != (double)(int64_t)v.as.number)
            return (0);
        v = val_int((int64_t)v.as.number);
    }
    if (v.type != VAL_INT || rt_range_len(r) == 0)
        return (0);
    i = v.as.integer;
    if (r->step > 0 && (i < r->start || i >= r->stop))
        return (0);
    if (r->step < 0 && (i > r->start || i <= r->stop))
        return (0);
    if (r->step > 0)
        dist = (uint64_t)i - (uint64_t)r->start;
    else
        dist = (uint64_t)r->start - (uint64_t)i;
    return (dist % (r->step > 0 ? (uint64_t)r->step
            : 0 - (uint64_t)r->step) == 0);
}

/* x in c: walks c item by item, comparing with is. */
static int  op_in(const t_node *n, t_value a, t_value b, t_value *out,
                t_error *err)
{
    const t_value   *items;
    size_t          len;
    size_t          i;
    int             r;

    if (val_is_range(b))
    {
        *out = val_bool(in_range(a, val_range(b)));
        return (0);
    }
    if (val_is_dict(b) || val_is_set(b))
    {
        if (rt_table_has(n, b, a, &r, err) == -1)
            return (-1);
        *out = val_bool(r);
        return (0);
    }
    if (val_is_string(b) && !val_is_string(a))
        return (rt_error(err, ERR_TYPE, n, "'in' a string needs a string on"
                " the left, not '%s'", val_type_name(a)));
    if (val_is_string(b))
    {
        *out = val_bool(rt_string_contains(val_string(b), val_string(a)));
        return (0);
    }
    if (!val_is_tuple(b) && !val_is_list(b))
        return (rt_error(err, ERR_TYPE, n, "'%s' object is not iterable",
                val_type_name(b)));
    items = rt_seq_items(b, &len);
    r = 0;
    for (i = 0; i < len && r == 0; i++)
        r = rt_equal(a, items[i]);
    if (r == -1)
        return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
    *out = val_bool(r);
    return (0);
}

/*
 * The operands are the two top values of the stack (left below right):
 * they stay rooted while the result is built. The caller pops them.
 */
int rt_binary(t_interp *in, const t_node *n, t_token_type op, t_value *out,
        t_error *err)
{
    t_value a;
    t_value b;
    int     c;

    a = rt_peek(in, 1);
    b = rt_peek(in, 0);
    if (op == TOK_IS)
    {
        c = rt_equal(a, b);
        if (c == -1)
            return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
        *out = val_bool(c);
        return (0);
    }
    if (op == TOK_IN)
        return (op_in(n, a, b, out, err));
    if ((op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE)
        && (compare_method(a) || compare_method(b)))
    {
        if (rt_compare(in, n, a, b, &c, err) == -1)
            return (-1);
        *out = val_bool(op == TOK_LT ? c < 0 : op == TOK_LE ? c <= 0
                : op == TOK_GT ? c > 0 : c >= 0);
        return (0);
    }
    if (op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE)
        return (compare(n, op, a, b, out, err));
    if (op == TOK_XOR && a.type == VAL_BOOL && b.type == VAL_BOOL)
    {
        *out = val_bool(a.as.boolean != b.as.boolean);
        return (0);
    }
    if (op == TOK_XOR)
        return (rt_error(err, ERR_TYPE, n, "'xor' needs bools, not '%s' and"
                " '%s'", val_type_name(a), val_type_name(b)));
    if (is_number(a) && is_number(b))
        return (number_op(n, op, a, b, out, err));
    if (op == TOK_PLUS && val_is_string(a) && val_is_string(b))
        return (concat(in, val_string(a), val_string(b), out));
    if (op == TOK_PLUS && val_is_list(a) && val_is_list(b))
        return (concat_lists(in, a, b, out));
    if (op == TOK_STAR && val_is_list(a))
        return (repeat_list(in, n, a, b, out, err));
    if (op == TOK_STAR && val_is_list(b))
        return (repeat_list(in, n, b, a, out, err));
    if (op == TOK_STAR && val_is_string(a))
        return (repeat(in, n, val_string(a), b, out, err));
    if (op == TOK_STAR && val_is_string(b))
        return (repeat(in, n, val_string(b), a, out, err));
    return (type_error(n, op, a, b, err));
}
