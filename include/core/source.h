#ifndef SOURCE_H
# define SOURCE_H

#include <stddef.h>
#include "core/arena.h"

/*
 * Global positions: every loaded source (a file, or a REPL line) gets its
 * own range of offsets [base, base + len]. A single size_t offset in a
 * token, node or error therefore identifies both the source and the
 * position inside it. The ranges never touch (+1 gap), so the end-of-file
 * offset of a source is never the first offset of the next one.
 */
typedef struct s_source
{
    const char  *path;      /* "examples/a.pce" or "<stdin:N>" */
    const char  *text;      /* '\0'-terminated copy, lives in the arena */
    size_t      len;
    size_t      base;
}   t_source;

typedef struct s_sources
{
    t_source    **items;    /* sorted by base (sources are only appended) */
    size_t      count;
    size_t      cap;
    size_t      next_base;
}   t_sources;

void            sources_init(t_sources *s);

/*
 * Copies path and text into the arena and registers them. The caller may
 * free its own buffers afterwards. The returned pointer stays valid until
 * sources_free (pointers are stable even when the table grows).
 */
const t_source  *sources_add(t_sources *s, t_arena *arena, const char *path,
                    const char *text, size_t len);

/* Source containing a global offset (binary search). */
const t_source  *sources_find(const t_sources *s, size_t offset);

/* Global offset of a pointer into src->text (a t_name, for instance). */
size_t          source_offset(const t_source *src, const char *p);

/* 1-based line of a global offset inside src. */
size_t          source_line(const t_source *src, size_t offset);

/* Frees the table only: path and text belong to the arena. */
void            sources_free(t_sources *s);

#endif
