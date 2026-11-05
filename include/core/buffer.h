#ifndef BUFFER_H
# define BUFFER_H

#include <stdarg.h>
#include <stddef.h>

/*
 * A growable byte buffer (malloc'd; out of memory is fatal). It must
 * belong to something the session frees (the interpreter has one), so
 * that fatal_oom's cleanup also releases it: never keep one only in a C
 * local. data is always '\0'-terminated after len bytes.
 */
typedef struct s_buffer
{
    char    *data;
    size_t  len;
    size_t  cap;
}   t_buffer;

void    buf_init(t_buffer *b);

/* Empties it, keeping its memory for the next use. */
void    buf_clear(t_buffer *b);

void    buf_add(t_buffer *b, const char *s, size_t n);
void    buf_add_str(t_buffer *b, const char *s);
void    buf_add_char(t_buffer *b, char c);

/* Appends like printf. */
void    buf_printf(t_buffer *b, const char *fmt, ...)
            __attribute__((format(printf, 2, 3)));
void    buf_vprintf(t_buffer *b, const char *fmt, va_list ap)
            __attribute__((format(printf, 2, 0)));

void    buf_free(t_buffer *b);

#endif
