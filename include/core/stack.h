#ifndef STACK_H
# define STACK_H

/*
 * Guard against overflowing the C stack. Every recursive walk (parser,
 * resolver, ast_print, interpreter) asks stack_exhausted() before going
 * one level deeper and fails with an error instead of crashing: no
 * nesting or recursion depth can ever end in a segfault.
 *
 * It measures the real stack in use (the distance from the frame of
 * main to the current one), not a count of levels, so it adapts to the
 * size of each frame and to the system's stack limit.
 */

/* Call once, at the very start of main. */
void    stack_init(void);

/* 1 when less than the safety margin of the stack is left. */
int     stack_exhausted(void);

#endif
