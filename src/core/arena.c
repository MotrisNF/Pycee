#include <stdlib.h>
#include <string.h>
#include "core/arena.h"
#include "core/errors.h"

#define ARENA_BLOCK_SIZE    (64 * 1024)
#define ARENA_ALIGN         _Alignof(max_align_t)

static t_arena_block    *block_new(size_t size)
{
    t_arena_block   *b;

    b = malloc(sizeof(t_arena_block) + size);
    if (!b)
        fatal_oom();
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return (b);
}

void    arena_init(t_arena *a)
{
    a->head = NULL;
}

/*
 * The current block is always the head. A request larger than a whole
 * block gets its own block, inserted after the head so the space left
 * in the current block is not wasted.
 */
void    *arena_alloc(t_arena *a, size_t size)
{
    t_arena_block   *b;
    size_t          start;

    size = (size + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1);
    if (size > ARENA_BLOCK_SIZE)
    {
        b = block_new(size);
        if (a->head)
        {
            b->next = a->head->next;
            a->head->next = b;
        }
        else
            a->head = b;
        b->used = size;
        memset(b->data, 0, size);
        return (b->data);
    }
    if (!a->head || a->head->size - a->head->used < size)
    {
        b = block_new(ARENA_BLOCK_SIZE);
        b->next = a->head;
        a->head = b;
    }
    start = a->head->used;
    a->head->used += size;
    memset(a->head->data + start, 0, size);
    return (a->head->data + start);
}

char    *arena_strndup(t_arena *a, const char *s, size_t n)
{
    char    *copy;

    copy = arena_alloc(a, n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return (copy);
}

void    arena_free(t_arena *a)
{
    t_arena_block   *b;
    t_arena_block   *next;

    for (b = a->head; b; b = next)
    {
        next = b->next;
        free(b);
    }
    a->head = NULL;
}
