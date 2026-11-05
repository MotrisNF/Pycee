#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/buffer.h"
#include "core/errors.h"

void    buf_init(t_buffer *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void    buf_clear(t_buffer *b)
{
    b->len = 0;
    if (b->data)
        b->data[0] = '\0';
}

/*
 * Room for n more bytes plus the '\0'. The fields only change once
 * realloc has succeeded, so the buffer stays valid for fatal_oom.
 */
static void reserve(t_buffer *b, size_t n)
{
    size_t  cap;
    char    *grown;

    if (n > SIZE_MAX - b->len - 1)
        fatal_oom();
    if (b->len + n + 1 <= b->cap)
        return ;
    cap = b->cap ? b->cap : 64;
    while (cap < b->len + n + 1)
    {
        if (cap > SIZE_MAX / 2)
        {
            cap = b->len + n + 1;
            break ;
        }
        cap *= 2;
    }
    grown = realloc(b->data, cap);
    if (!grown)
        fatal_oom();
    b->data = grown;
    b->cap = cap;
}

void    buf_add(t_buffer *b, const char *s, size_t n)
{
    reserve(b, n);
    if (n)
        memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void    buf_add_str(t_buffer *b, const char *s)
{
    buf_add(b, s, strlen(s));
}

void    buf_add_char(t_buffer *b, char c)
{
    buf_add(b, &c, 1);
}

void    buf_vprintf(t_buffer *b, const char *fmt, va_list ap)
{
    va_list copy;
    int     n;

    va_copy(copy, ap);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0)
        return ;
    reserve(b, (size_t)n);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    b->len += (size_t)n;
}

void    buf_printf(t_buffer *b, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    buf_vprintf(b, fmt, ap);
    va_end(ap);
}

void    buf_free(t_buffer *b)
{
    free(b->data);
    buf_init(b);
}
