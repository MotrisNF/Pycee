/* Symbol tables: a growable array, hashed for the globals. */

#include <stdlib.h>
#include <string.h>
#include "resolver/resolver_internal.h"

void symtab_init(t_symtab *t, int hashed)
{
    t->items = NULL;
    t->count = 0;
    t->cap = 0;
    t->hashed = hashed;
    t->index = NULL;
    t->index_cap = 0;
}

int  rs_name_eq(t_name a, t_name b)
{
    return (a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0);
}

/* FNV-1a. */
size_t  rs_name_hash(t_name name)
{
    size_t  h;
    size_t  i;

    h = 14695981039346656037ULL;
    for (i = 0; i < name.len; i++)
    {
        h ^= (unsigned char)name.ptr[i];
        h *= 1099511628211ULL;
    }
    return (h);
}

/* index_cap is a power of two and the index is never full: probing ends. */
static void index_insert(t_symtab *t, size_t pos)
{
    size_t  i;

    i = rs_name_hash(t->items[pos].name) & (t->index_cap - 1);
    while (t->index[i])
        i = (i + 1) & (t->index_cap - 1);
    t->index[i] = pos + 1;
}

/* Rebuilds the index for items[0..count) with cap slots. */
static void index_rebuild(t_symtab *t, size_t cap)
{
    size_t  *index;
    size_t  pos;

    if (cap != t->index_cap)
    {
        index = calloc(cap, sizeof(*index));
        if (!index)
            fatal_oom();
        free(t->index);
        t->index = index;
        t->index_cap = cap;
    }
    else
        memset(t->index, 0, cap * sizeof(*t->index));
    for (pos = 0; pos < t->count; pos++)
        index_insert(t, pos);
}

/*
 * Sizes only change once the allocation has succeeded, so the table stays
 * valid for the cleanup fatal_oom runs.
 */
void symtab_push(t_symtab *t, const t_symbol *sym)
{
    t_symbol    *grown;
    size_t      cap;

    if (t->count == t->cap)
    {
        cap = t->cap ? t->cap * 2 : 16;
        grown = realloc(t->items, cap * sizeof(*t->items));
        if (!grown)
            fatal_oom();
        t->items = grown;
        t->cap = cap;
    }
    t->items[t->count++] = *sym;
    if (!t->hashed)
        return ;
    if (2 * t->count > t->index_cap)
        index_rebuild(t, t->index_cap ? t->index_cap * 2 : 64);
    else
        index_insert(t, t->count - 1);
}

/* Drops every symbol from position count on (a failed REPL line). */
void symtab_truncate(t_symtab *t, size_t count)
{
    if (count == t->count)
        return ;
    t->count = count;
    if (t->hashed)
        index_rebuild(t, t->index_cap);
}

void symtab_free(t_symtab *t)
{
    free(t->items);
    free(t->index);
    symtab_init(t, t->hashed);
}

t_symbol *symtab_find(const t_symtab *t, t_name name)
{
    size_t  i;
    size_t  pos;

    if (t->hashed && t->index_cap)
    {
        i = rs_name_hash(name) & (t->index_cap - 1);
        while ((pos = t->index[i]))
        {
            if (rs_name_eq(t->items[pos - 1].name, name))
                return (&t->items[pos - 1]);
            i = (i + 1) & (t->index_cap - 1);
        }
        return (NULL);
    }
    for (i = 0; i < t->count; i++)
        if (rs_name_eq(t->items[i].name, name))
            return (&t->items[i]);
    return (NULL);
}

/* The last symbol pushed with that name (unhashed tables only). */
t_symbol *symtab_find_last(const t_symtab *t, t_name name)
{
    size_t  i;

    for (i = t->count; i-- > 0;)
        if (rs_name_eq(t->items[i].name, name))
            return (&t->items[i]);
    return (NULL);
}
