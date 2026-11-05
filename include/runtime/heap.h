#ifndef HEAP_H
# define HEAP_H

#include <stddef.h>
#include "runtime/value.h"

/*
 * Every heap object of the session, linked through their headers, and
 * the mark & sweep collector that frees the ones nothing can reach.
 *
 * A collection may run inside any heap_alloc: it marks from the roots
 * (roots_fn marks them: the interpreter's value stack, globals...), then
 * frees whatever was not marked. So a value that must survive an
 * allocation has to be reachable from a root at that moment (see the
 * rule in interp.h). Objects never move: a pointer to a live one stays
 * valid. Marking walks gray (objects marked but not traced yet) instead
 * of recursing, so no nesting can exhaust the C stack.
 *
 * bytes is what live objects take; a collection runs when it passes
 * next_gc, which is then set to twice what survived. stress collects in
 * every allocation (PYCEE_GC_STRESS=1), to catch a value left out of the
 * roots as soon as it happens.
 */
typedef struct s_heap
{
    t_obj   *objects;
    size_t  bytes;
    size_t  count;
    size_t  next_gc;
    size_t  collections;
    uint64_t    last_id;
    int     stress;
    t_obj   **gray;
    size_t  ngray;
    size_t  gray_cap;
    void    (*roots_fn)(struct s_heap *h, void *ctx);
    void    *roots_ctx;
}   t_heap;

void        heap_init(t_heap *h);

/* Who marks the roots, called at the start of every collection. */
void        heap_set_roots(t_heap *h, void (*fn)(t_heap *, void *),
                void *ctx);

/* size includes the header. Out of memory is fatal (fatal_oom). */
t_obj       *heap_alloc(t_heap *h, t_obj_type type, size_t size);

/* A new string of len bytes left for the caller to fill (plus its '\0'). */
t_string    *heap_string_alloc(t_heap *h, size_t len);

/* A new string with a copy of len bytes. */
t_string    *heap_string(t_heap *h, const char *chars, size_t len);

/* A new tuple of len values, all none until the caller sets them. */
t_tuple     *heap_tuple(t_heap *h, size_t len);

/*
 * A new empty list with room for cap values. heap_list_reserve makes room
 * for at least need values (the heap counts the bytes of items too).
 */
t_list      *heap_list(t_heap *h, size_t cap);
void        heap_list_reserve(t_heap *h, t_list *l, size_t need);

/* For roots_fn: marks what v reaches. */
void        heap_mark_value(t_heap *h, t_value v);
void        heap_mark_object(t_heap *h, t_obj *o);

/* Runs a collection now. */
void        heap_collect(t_heap *h);

void        heap_free(t_heap *h);

#endif
