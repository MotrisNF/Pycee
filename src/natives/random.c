/*
 * Module random: pseudo-random numbers, from a generator of its own
 * (xoshiro256**, seeded through splitmix64) rather than the C library's,
 * so that random.seed(n) gives the same sequence on any machine. Without
 * a seed it starts from the system's randomness (getrandom), different
 * on every run. Not for cryptography.
 */

#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>
#include "core/utf8.h"
#include "natives/modules.h"

static uint64_t rotl(uint64_t x, int k)
{
    return ((x << k) | (x >> (64 - k)));
}

/* splitmix64: spreads one number into the four words of the state. */
static uint64_t splitmix(uint64_t *x)
{
    uint64_t    z;

    *x += 0x9e3779b97f4a7c15ULL;
    z = *x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return (z ^ (z >> 31));
}

static void seed_with(t_interp *in, uint64_t seed)
{
    int i;

    for (i = 0; i < 4; i++)
        in->rng[i] = splitmix(&seed);
    in->rng_seeded = 1;
}

/* From the system; should getrandom fail, the time and the pid. */
static void seed_from_system(t_interp *in)
{
    struct timespec ts;
    uint64_t        seed;

    if (getrandom(&seed, sizeof(seed), 0) != (ssize_t)sizeof(seed))
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        seed = (uint64_t)ts.tv_sec * 1000000007ULL ^ (uint64_t)ts.tv_nsec
            ^ ((uint64_t)getpid() << 32);
    }
    seed_with(in, seed);
}

/* xoshiro256**: the next 64 random bits. */
static uint64_t next(t_interp *in)
{
    uint64_t    *s;
    uint64_t    result;
    uint64_t    t;

    if (!in->rng_seeded)
        seed_from_system(in);
    s = in->rng;
    result = rotl(s[1] * 5, 7) * 9;
    t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return (result);
}

/* Uniform in [0, n), n > 0, without the bias of a plain modulo. */
static uint64_t below(t_interp *in, uint64_t n)
{
    uint64_t    threshold;
    uint64_t    r;

    threshold = (0 - n) % n;
    r = next(in);
    while (r < threshold)
        r = next(in);
    return (r % n);
}

/* seed(n): from now on, the sequence that n gives (n an integer). */
int nat_random_seed(t_interp *in, size_t base, int nargs, const t_node *call,
        t_value *out, t_error *err)
{
    int64_t n;

    (void)nargs;
    if (rt_int_arg(call, ARG(0), "seed", "seed", &n, err) == -1)
        return (-1);
    seed_with(in, (uint64_t)n);
    *out = val_none();
    return (0);
}

/* rand_int(a, b): an int from a to b, both included (a <= b). */
int nat_random_rand_int(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    int64_t     a;
    int64_t     b;
    uint64_t    span;

    (void)nargs;
    if (rt_int_arg(call, ARG(0), "rand_int", "bound", &a, err) == -1
        || rt_int_arg(call, ARG(1), "rand_int", "bound", &b, err) == -1)
        return (-1);
    if (a > b)
        return (rt_error(err, ERR_VALUE, call, "rand_int() needs a <= b, not"
                " %lld > %lld", (long long)a, (long long)b));
    span = (uint64_t)b - (uint64_t)a + 1;
    if (span == 0)
        *out = val_int((int64_t)next(in));
    else
        *out = val_int((int64_t)((uint64_t)a + below(in, span)));
    return (0);
}

/* rand_float(): a float in [0, 1), from 53 random bits. */
int nat_random_rand_float(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)base;
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_float((double)(next(in) >> 11) * 0x1.0p-53);
    return (0);
}

/*
 * How many items v has, for choice and sample: a list, a tuple, a string
 * (its characters) or a range; a set has no order to pick by position.
 */
static int  seq_len(const t_node *call, t_value v, const char *fname,
                uint64_t *out, t_error *err)
{
    if (val_is_list(v))
        *out = val_list(v)->len;
    else if (val_is_tuple(v))
        *out = val_tuple(v)->len;
    else if (val_is_string(v))
        *out = rt_string_chars(val_string(v));
    else if (val_is_range(v))
        *out = rt_range_len(val_range(v));
    else
        return (rt_error(err, ERR_TYPE, call, "%s() needs a list, a tuple, a"
                " string or a range, not '%s'", fname, val_type_name(v)));
    return (0);
}

/* Item i of v (seq_len accepted it); a character is a new string. */
static t_value  seq_item(t_interp *in, t_value v, uint64_t i)
{
    t_string    *s;
    t_string    *c;
    size_t      pos;
    int32_t     cp;
    int         len;

    if (val_is_list(v))
        return (val_list(v)->items[i]);
    if (val_is_tuple(v))
        return (val_tuple(v)->items[i]);
    if (val_is_range(v))
        return (val_int(rt_range_at(val_range(v), i)));
    s = val_string(v);
    pos = rt_string_offset(s, (size_t)i);
    len = utf8_decode(s->chars + pos, s->len - pos, &cp);
    c = heap_string(&in->heap, s->chars + pos, (size_t)len);
    c->nchars = 1;
    return (val_obj(&c->obj));
}

/* choice(seq): one item at random; IndexError if it is empty. */
int nat_random_choice(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    uint64_t    n;

    (void)nargs;
    if (seq_len(call, ARG(0), "choice", &n, err) == -1)
        return (-1);
    if (n == 0)
        return (rt_error(err, ERR_INDEX, call, "choice() from an empty %s",
                val_type_name(ARG(0))));
    *out = seq_item(in, ARG(0), below(in, n));
    return (0);
}

/* The items of l in a random order (Fisher-Yates), in place. */
static void shuffle_list(t_interp *in, t_list *l)
{
    t_value     tmp;
    uint64_t    j;
    size_t      i;

    i = l->len;
    while (i > 1)
    {
        j = below(in, i);
        i--;
        tmp = l->items[i];
        l->items[i] = l->items[j];
        l->items[j] = tmp;
    }
}

/* shuffle(l): in place; gives back the list (a tuple cannot change). */
int nat_random_shuffle(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    if (!val_is_list(ARG(0)))
        return (rt_error(err, ERR_TYPE, call, "shuffle() needs a list, not"
                " '%s'%s", val_type_name(ARG(0)), val_is_tuple(ARG(0))
                ? " (a tuple cannot change: shuffle(t.to_list()))" : ""));
    shuffle_list(in, val_list(ARG(0)));
    *out = ARG(0);
    return (0);
}

/*
 * Positions already taken, an open-addressing table of cap slots (a
 * power of two) in a buffer of the session: position + 1, 0 = free.
 * Adds i; 0 if it was there.
 */
static int  take(uint64_t *table, uint64_t cap, uint64_t i)
{
    uint64_t    h;

    h = (i * 0x9e3779b97f4a7c15ULL) & (cap - 1);
    while (table[h])
    {
        if (table[h] == i + 1)
            return (0);
        h = (h + 1) & (cap - 1);
    }
    table[h] = i + 1;
    return (1);
}

/*
 * k distinct positions below n (Floyd's algorithm: k draws, never a
 * retry), as the list at stack[slot] fills: then its order is shuffled,
 * so every ordered sample is as likely. A k whose table could not even
 * be counted in memory is out of memory, as the list would be.
 */
static int  sample_positions(t_interp *in, size_t base, size_t slot,
                uint64_t n, uint64_t k)
{
    t_buffer    *b;
    uint64_t    *table;
    uint64_t    cap;
    uint64_t    j;
    uint64_t    t;
    t_list      *l;
    t_value     v;

    if (k > SIZE_MAX / (4 * sizeof(uint64_t)))
        fatal_oom();
    cap = 1;
    while (cap < 2 * k)
        cap *= 2;
    b = rt_buf_take(in);
    buf_fill(b, 0, (size_t)cap * sizeof(uint64_t));
    table = (uint64_t *)b->data;
    for (j = n - k; j < n; j++)
    {
        t = below(in, j + 1);
        if (!take(table, cap, t))
        {
            t = j;
            take(table, cap, t);
        }
        v = seq_item(in, ARG(0), t);
        l = val_list(in->stack[slot]);
        rt_push(in, v);
        heap_list_reserve(&in->heap, l, l->len + 1);
        l->items[l->len++] = rt_pop(in);
    }
    rt_buf_give(in);
    shuffle_list(in, val_list(in->stack[slot]));
    return (0);
}

/* sample(seq, k): a new list of k items taken at random, none twice. */
int nat_random_sample(t_interp *in, size_t base, int nargs,
        const t_node *call, t_value *out, t_error *err)
{
    uint64_t    n;
    int64_t     k;
    size_t      slot;

    (void)nargs;
    if (seq_len(call, ARG(0), "sample", &n, err) == -1
        || rt_int_arg(call, ARG(1), "sample", "count", &k, err) == -1)
        return (-1);
    if (k < 0 || (uint64_t)k > n)
        return (rt_error(err, ERR_VALUE, call, "sample() needs a count from"
                " 0 to %llu (how many items there are), not %lld",
                (unsigned long long)n, (long long)k));
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    sample_positions(in, base, slot, n, (uint64_t)k);
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}
