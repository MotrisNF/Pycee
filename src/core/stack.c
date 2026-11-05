#include <stddef.h>
#include <stdint.h>
#include <sys/resource.h>
#include "core/stack.h"

/*
 * Used when the limit is unknown or unlimited: the usual Linux default.
 * The margin is left for what runs after the check fails (building and
 * printing the error) and for frames bigger than usual (sanitizers).
 */
#define STACK_DEFAULT   ((size_t)8 * 1024 * 1024)
#define STACK_MARGIN    ((size_t)512 * 1024)

static uintptr_t    g_base;
static size_t       g_usable;

void    stack_init(void)
{
    struct rlimit   rl;
    size_t          size;

    g_base = (uintptr_t)__builtin_frame_address(0);
    size = STACK_DEFAULT;
    if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY
        && rl.rlim_cur < (rlim_t)SIZE_MAX)
        size = (size_t)rl.rlim_cur;
    g_usable = size > 2 * STACK_MARGIN ? size - STACK_MARGIN : size / 2;
}

/* The stack may grow down (x86, ARM) or up: the distance works both ways. */
int stack_exhausted(void)
{
    uintptr_t   here;
    size_t      used;

    here = (uintptr_t)__builtin_frame_address(0);
    used = here < g_base ? g_base - here : here - g_base;
    return (used > g_usable);
}
