/*
 * The hash table under dict and set: entries in insertion order, an
 * index to find them. Keys are immutable values (numbers, strings,
 * bools, none, ranges, and tuples of those): one that could change
 * afterwards would leave the table corrupt. Equal values hash alike:
 * 1 and 1.0 are one key, as 1 is 1.0.
 */

#include <stdlib.h>
#include <string.h>
#include "core/stack.h"
#include "runtime/interp_internal.h"

static uint64_t mix(uint64_t h)
{
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return (h);
}

static uint64_t hash_bytes(const char *s, size_t len)
{
    uint64_t    h;
    size_t      i;

    h = 14695981039346656037ULL;
    for (i = 0; i < len; i++)
    {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return (mix(h));
}

static uint64_t hash_number(t_value v)
{
    double      f;
    uint64_t    bits;

    if (v.type == VAL_INT)
        return (mix((uint64_t)v.as.integer));
    f = v.as.number;
    if (f == f && f >= -9223372036854775808.0 && f < 9223372036854775808.0
        && f == (double)(int64_t)f)
        return (mix((uint64_t)(int64_t)f));
    memcpy(&bits, &f, sizeof(bits));
    return (mix(bits ^ 0x5bd1e995ULL));
}

/*
 * The hash of a key, or a TypeError for a value that can change (what
 * is the type of its error message: "a dict key", "a set element").
 */
int rt_hash(const t_node *n, t_value v, const char *what, uint64_t *out,
        t_error *err)
{
    const t_tuple   *t;
    const t_range   *r;
    uint64_t        h;
    size_t          i;

    if (v.type == VAL_NONE)
        *out = 0x9e3779b97f4a7c15ULL;
    else if (v.type == VAL_BOOL)
        *out = mix(0x1234567ULL + (uint64_t)v.as.boolean);
    else if (v.type == VAL_INT || v.type == VAL_FLOAT)
        *out = hash_number(v);
    else if (val_is_string(v))
        *out = hash_bytes(val_string(v)->chars, val_string(v)->len);
    else if (val_is_range(v))
    {
        r = val_range(v);
        h = rt_range_len(r);
        *out = mix(h * 31 + (h ? (uint64_t)r->start : 0) * 17
                + (h > 1 ? (uint64_t)r->step : 0));
    }
    else if (val_is_tuple(v))
    {
        if (stack_exhausted())
            return (rt_error(err, ERR_RECURSION, n, "stack exhausted"));
        t = val_tuple(v);
        h = 0x345678ULL;
        for (i = 0; i < t->len; i++)
        {
            if (rt_hash(n, t->items[i], what, out, err) == -1)
                return (-1);
            h = mix(h * 1000003ULL ^ *out);
        }
        *out = h;
    }
    else
        return (rt_error(err, ERR_TYPE, n, "a '%s' cannot be %s: it can"
                " change (only numbers, strings, bools, none, ranges and"
                " tuples of them can)", val_type_name(v), what));
    return (0);
}

/*
 * The entry of key in d: 1 found (*pos), 0 not there, -1 RecursionError
 * (comparing very deep tuples).
 */
int rt_table_find(const t_dict *d, t_value key, uint64_t hash, size_t *pos)
{
    size_t  i;
    size_t  e;
    int     r;

    if (d->index_cap == 0)
        return (0);
    i = (size_t)hash & (d->index_cap - 1);
    while ((e = d->index[i]) != 0)
    {
        if (d->entries[e - 1].used && d->entries[e - 1].hash == hash)
        {
            r = rt_equal(d->entries[e - 1].key, key);
            if (r != 0)
            {
                *pos = e - 1;
                return (r);
            }
        }
        i = (i + 1) & (d->index_cap - 1);
    }
    return (0);
}

/*
 * Makes room for one more entry: drops the removed ones and, if still
 * full, doubles; then the index is built again (never more than half
 * full). Sizes only change once their allocation has succeeded.
 */
static void make_room(t_heap *h, t_dict *d)
{
    t_entry *grown;
    size_t  *index;
    size_t  cap;
    size_t  icap;
    size_t  i;
    size_t  j;

    if (d->nentries < d->cap)
        return ;
    j = 0;
    for (i = 0; i < d->nentries; i++)
        if (d->entries[i].used)
            d->entries[j++] = d->entries[i];
    d->nentries = j;
    cap = d->cap ? d->cap : 4;
    while (cap < 2 * (d->len + 1))
    {
        if (cap > SIZE_MAX / 4 / sizeof(t_entry))
            fatal_oom();
        cap *= 2;
    }
    if (cap != d->cap)
    {
        grown = realloc(d->entries, cap * sizeof(t_entry));
        if (!grown)
            fatal_oom();
        h->bytes += (cap - d->cap) * sizeof(t_entry);
        d->entries = grown;
        d->cap = cap;
    }
    icap = 8;
    while (icap < 2 * d->cap)
        icap *= 2;
    index = calloc(icap, sizeof(size_t));
    if (!index)
        fatal_oom();
    free(d->index);
    h->bytes += (icap - d->index_cap) * sizeof(size_t);
    d->index = index;
    d->index_cap = icap;
    for (i = 0; i < d->nentries; i++)
    {
        j = (size_t)d->entries[i].hash & (icap - 1);
        while (d->index[j])
            j = (j + 1) & (icap - 1);
        d->index[j] = i + 1;
    }
}

/*
 * key -> value (a set passes none as the value): replaced if the key is
 * there, else added at the end. The caller keeps both rooted; nothing
 * here collects. -1 only for a RecursionError while comparing keys.
 */
int rt_table_set(t_heap *h, t_dict *d, t_value key, uint64_t hash,
        t_value value)
{
    size_t  pos;
    size_t  i;
    int     r;

    r = rt_table_find(d, key, hash, &pos);
    if (r == -1)
        return (-1);
    if (r == 1)
    {
        d->entries[pos].value = value;
        return (0);
    }
    make_room(h, d);
    pos = d->nentries++;
    d->entries[pos].key = key;
    d->entries[pos].value = value;
    d->entries[pos].hash = hash;
    d->entries[pos].used = 1;
    d->len++;
    i = (size_t)hash & (d->index_cap - 1);
    while (d->index[i])
        i = (i + 1) & (d->index_cap - 1);
    d->index[i] = pos + 1;
    return (0);
}

/* Takes the entry at pos out (it stays as a removed one until a rebuild). */
void    rt_table_remove(t_dict *d, size_t pos)
{
    d->entries[pos].used = 0;
    d->entries[pos].key = val_none();
    d->entries[pos].value = val_none();
    d->len--;
}

void    rt_table_clear(t_dict *d)
{
    size_t  i;

    for (i = 0; i < d->nentries; i++)
        rt_table_remove(d, i);
    d->len = 0;
    if (d->index)
        memset(d->index, 0, d->index_cap * sizeof(size_t));
    d->nentries = 0;
}

t_dict  *rt_table_new(t_heap *h, t_obj_type type)
{
    t_dict  *d;

    d = (t_dict *)heap_alloc(h, type, sizeof(t_dict));
    d->len = 0;
    d->nentries = 0;
    d->cap = 0;
    d->entries = NULL;
    d->index = NULL;
    d->index_cap = 0;
    return (d);
}
