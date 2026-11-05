#include <stdlib.h>
#include <string.h>
#include "core/source.h"
#include "core/errors.h"

void    sources_init(t_sources *s)
{
    s->items = NULL;
    s->count = 0;
    s->cap = 0;
    s->next_base = 0;
}

const t_source  *sources_add(t_sources *s, t_arena *arena, const char *path,
                    const char *text, size_t len)
{
    t_source    *src;
    t_source    **grown;

    if (s->count == s->cap)
    {
        s->cap = s->cap ? s->cap * 2 : 8;
        grown = realloc(s->items, s->cap * sizeof(*s->items));
        if (!grown)
            fatal_oom();
        s->items = grown;
    }
    src = arena_alloc(arena, sizeof(*src));
    src->path = arena_strndup(arena, path, strlen(path));
    src->text = arena_strndup(arena, text, len);
    src->len = len;
    src->base = s->next_base;
    s->next_base += len + 1;
    s->items[s->count++] = src;
    return (src);
}

/* Last source whose base is <= offset. */
const t_source  *sources_find(const t_sources *s, size_t offset)
{
    size_t  lo;
    size_t  hi;
    size_t  mid;

    lo = 0;
    hi = s->count;
    while (hi - lo > 1)
    {
        mid = lo + (hi - lo) / 2;
        if (s->items[mid]->base <= offset)
            lo = mid;
        else
            hi = mid;
    }
    return (s->count ? s->items[lo] : NULL);
}

size_t  source_offset(const t_source *src, const char *p)
{
    return (src->base + (size_t)(p - src->text));
}

size_t  source_line(const t_source *src, size_t offset)
{
    const char  *p;
    const char  *end;
    size_t      line;

    line = 1;
    end = src->text + (offset - src->base);
    for (p = src->text; p < end; p++)
        if (*p == '\n')
            line++;
    return (line);
}

void    sources_free(t_sources *s)
{
    free(s->items);
    sources_init(s);
}
