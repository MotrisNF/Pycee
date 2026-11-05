#ifndef ARENA_H
# define ARENA_H

#include <stddef.h>

/*
 * Bump allocator for memory that lives until a single arena_free():
 * the AST and the copy of the source it points into. It is not the GC
 * (runtime values are collected separately, phase 8).
 */
typedef struct s_arena_block
{
    struct s_arena_block    *next;
    size_t                  size;
    size_t                  used;
    _Alignas(max_align_t) unsigned char data[];
}   t_arena_block;

typedef struct s_arena
{
    t_arena_block   *head;
}   t_arena;

void    arena_init(t_arena *a);

/* Zero-filled, aligned for any type. Never returns NULL: out of memory is fatal. */
void    *arena_alloc(t_arena *a, size_t size);

/* Copies n bytes and adds a '\0'. */
char    *arena_strndup(t_arena *a, const char *s, size_t n);

void    arena_free(t_arena *a);

#endif
