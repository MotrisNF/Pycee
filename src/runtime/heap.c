#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/errors.h"
#include "front/ast.h"
#include "runtime/heap.h"

#define GC_MIN_BYTES    (1024 * 1024)

/* The settings of a fresh heap: no objects, no roots yet. */
static void heap_reset(t_heap *h)
{
    h->objects = NULL;
    h->bytes = 0;
    h->count = 0;
    h->next_gc = GC_MIN_BYTES;
    h->collections = 0;
    h->gray = NULL;
    h->ngray = 0;
    h->gray_cap = 0;
}

void    heap_init(t_heap *h)
{
    const char  *stress;

    heap_reset(h);
    h->last_id = 0;
    stress = getenv("PYCEE_GC_STRESS");
    h->stress = (stress && stress[0] && strcmp(stress, "0") != 0);
    h->roots_fn = NULL;
    h->roots_ctx = NULL;
}

void    heap_set_roots(t_heap *h, void (*fn)(t_heap *, void *), void *ctx)
{
    h->roots_fn = fn;
    h->roots_ctx = ctx;
}

/* What an object took when it was allocated (for bytes). */
static size_t   object_size(const t_obj *o)
{
    if (o->type == OBJ_STRING)
        return (sizeof(t_string) + ((const t_string *)o)->len + 1);
    if (o->type == OBJ_TUPLE)
        return (sizeof(t_tuple)
            + ((const t_tuple *)o)->len * sizeof(t_value));
    if (o->type == OBJ_FUNCTION)
        return (sizeof(t_function) + (size_t)((const t_function *)o)
            ->decl->as.func.ncaptures * sizeof(t_cell *));
    if (o->type == OBJ_RANGE)
        return (sizeof(t_range));
    if (o->type == OBJ_NATIVE)
        return (sizeof(t_native));
    if (o->type == OBJ_BOUND)
        return (sizeof(t_bound));
    if (o->type == OBJ_CELL)
        return (sizeof(t_cell));
    if (o->type == OBJ_LIST)
        return (sizeof(t_list) + ((const t_list *)o)->cap * sizeof(t_value));
    if (o->type == OBJ_DICT || o->type == OBJ_SET)
        return (sizeof(t_dict) + ((const t_dict *)o)->cap * sizeof(t_entry)
            + ((const t_dict *)o)->index_cap * sizeof(size_t));
    if (o->type == OBJ_INSTANCE)
        return (sizeof(t_instance)
            + ((const t_instance *)o)->nfields * sizeof(t_value));
    if (o->type == OBJ_CLASS)
        return (sizeof(t_class));
    if (o->type == OBJ_METHOD)
        return (sizeof(t_method_value));
    return (sizeof(t_error_value));
}

/* gray only grows once realloc has succeeded (fatal_oom's cleanup). */
void    heap_mark_object(t_heap *h, t_obj *o)
{
    t_obj   **grown;
    size_t  cap;

    if (!o || o->marked)
        return ;
    o->marked = 1;
    if (h->ngray == h->gray_cap)
    {
        cap = h->gray_cap ? h->gray_cap * 2 : 256;
        grown = realloc(h->gray, cap * sizeof(*h->gray));
        if (!grown)
            fatal_oom();
        h->gray = grown;
        h->gray_cap = cap;
    }
    h->gray[h->ngray++] = o;
}

void    heap_mark_value(t_heap *h, t_value v)
{
    if (v.type == VAL_OBJ)
        heap_mark_object(h, v.as.obj);
}

/* The error value whose err is e (a context always lives in one). */
static t_obj    *error_holder(const t_error *e)
{
    return ((t_obj *)((char *)e - offsetof(t_error_value, err)));
}

/* Marks what o points to. Strings, ranges and natives point to nothing. */
static void trace(t_heap *h, t_obj *o)
{
    const t_tuple       *t;
    const t_function    *f;
    const t_error       *e;
    size_t              i;

    if (o->type == OBJ_TUPLE)
    {
        t = (const t_tuple *)o;
        for (i = 0; i < t->len; i++)
            heap_mark_value(h, t->items[i]);
    }
    else if (o->type == OBJ_FUNCTION)
    {
        f = (const t_function *)o;
        for (i = 0; i < f->ncells; i++)
            heap_mark_object(h, &f->cells[i]->obj);
    }
    else if (o->type == OBJ_BOUND)
        heap_mark_value(h, ((const t_bound *)o)->self);
    else if (o->type == OBJ_CELL)
        heap_mark_value(h, ((const t_cell *)o)->value);
    else if (o->type == OBJ_LIST)
    {
        for (i = 0; i < ((const t_list *)o)->len; i++)
            heap_mark_value(h, ((const t_list *)o)->items[i]);
    }
    else if (o->type == OBJ_INSTANCE)
    {
        for (i = 0; i < ((const t_instance *)o)->nfields; i++)
            heap_mark_value(h, ((const t_instance *)o)->fields[i]);
        if (((const t_instance *)o)->error)
            heap_mark_object(h, &((t_instance *)o)->error->obj);
    }
    else if (o->type == OBJ_METHOD)
        heap_mark_value(h, ((const t_method_value *)o)->self);
    else if (o->type == OBJ_DICT || o->type == OBJ_SET)
    {
        for (i = 0; i < ((const t_dict *)o)->nentries; i++)
        {
            if (!((const t_dict *)o)->entries[i].used)
                continue ;
            heap_mark_value(h, ((const t_dict *)o)->entries[i].key);
            heap_mark_value(h, ((const t_dict *)o)->entries[i].value);
        }
    }
    else if (o->type == OBJ_ERROR)
    {
        e = &((const t_error_value *)o)->err;
        if (e->context)
            heap_mark_object(h, error_holder(e->context));
    }
}

/* An object and what it owns apart from itself. */
static void free_object(t_obj *o)
{
    if (o->type == OBJ_LIST)
        free(((t_list *)o)->items);
    if (o->type == OBJ_DICT || o->type == OBJ_SET)
    {
        free(((t_dict *)o)->entries);
        free(((t_dict *)o)->index);
    }
    free(o);
}

/* Frees every object not marked and clears the marks of the rest. */
static void sweep(t_heap *h)
{
    t_obj   **link;
    t_obj   *o;

    link = &h->objects;
    while (*link)
    {
        o = *link;
        if (o->marked)
        {
            o->marked = 0;
            link = &o->next;
            continue ;
        }
        *link = o->next;
        h->bytes -= object_size(o);
        h->count--;
        free_object(o);
    }
}

void    heap_collect(t_heap *h)
{
    t_obj   *o;

    if (h->roots_fn)
        h->roots_fn(h, h->roots_ctx);
    while (h->ngray > 0)
    {
        o = h->gray[--h->ngray];
        trace(h, o);
    }
    sweep(h);
    h->collections++;
    h->next_gc = h->bytes * 2;
    if (h->next_gc < GC_MIN_BYTES)
        h->next_gc = GC_MIN_BYTES;
}

/*
 * A collection may run first (before the new object exists, so it is
 * never at risk). The object is linked as soon as it exists, so the
 * cleanup always reaches it; the caller fills it before allocating again.
 */
t_obj   *heap_alloc(t_heap *h, t_obj_type type, size_t size)
{
    t_obj   *o;

    if (h->roots_fn && (h->stress || h->bytes + size > h->next_gc))
        heap_collect(h);
    o = malloc(size);
    if (!o)
        fatal_oom();
    o->type = type;
    o->marked = 0;
    o->id = ++h->last_id;
    o->next = h->objects;
    h->objects = o;
    h->bytes += size;
    h->count++;
    return (o);
}

t_string    *heap_string_alloc(t_heap *h, size_t len)
{
    t_string    *s;

    if (len > SIZE_MAX - sizeof(t_string) - 1)
        fatal_oom();
    s = (t_string *)heap_alloc(h, OBJ_STRING, sizeof(t_string) + len + 1);
    s->len = len;
    s->nchars = SIZE_MAX;
    s->chars[len] = '\0';
    return (s);
}

t_string    *heap_string(t_heap *h, const char *chars, size_t len)
{
    t_string    *s;

    s = heap_string_alloc(h, len);
    if (len)
        memcpy(s->chars, chars, len);
    return (s);
}

t_tuple *heap_tuple(t_heap *h, size_t len)
{
    t_tuple *t;
    size_t  i;

    if (len > (SIZE_MAX - sizeof(t_tuple)) / sizeof(t_value))
        fatal_oom();
    t = (t_tuple *)heap_alloc(h, OBJ_TUPLE,
            sizeof(t_tuple) + len * sizeof(t_value));
    t->len = len;
    for (i = 0; i < len; i++)
        t->items[i] = val_none();
    return (t);
}

t_list  *heap_list(t_heap *h, size_t cap)
{
    t_list  *l;

    l = (t_list *)heap_alloc(h, OBJ_LIST, sizeof(t_list));
    l->len = 0;
    l->cap = 0;
    l->items = NULL;
    heap_list_reserve(h, l, cap);
    return (l);
}

/*
 * Grows items to hold need values (doubling, so appending is amortised
 * O(1)). Never collects: a realloc is not a heap_alloc, so the caller's
 * values in flight stay safe. cap only changes once realloc succeeded.
 */
void    heap_list_reserve(t_heap *h, t_list *l, size_t need)
{
    t_value *grown;
    size_t  cap;

    if (need <= l->cap)
        return ;
    cap = l->cap ? l->cap : 8;
    while (cap < need)
    {
        if (cap > SIZE_MAX / 2 / sizeof(t_value))
            fatal_oom();
        cap *= 2;
    }
    grown = realloc(l->items, cap * sizeof(t_value));
    if (!grown)
        fatal_oom();
    h->bytes += (cap - l->cap) * sizeof(t_value);
    l->items = grown;
    l->cap = cap;
}

/* Everything, reachable or not (the end of the session, or fatal_oom). */
void    heap_free(t_heap *h)
{
    t_obj   *o;
    t_obj   *next;

    for (o = h->objects; o; o = next)
    {
        next = o->next;
        free_object(o);
    }
    free(h->gray);
    heap_reset(h);
}
