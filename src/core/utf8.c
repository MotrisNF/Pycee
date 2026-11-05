/* UTF-8, checked byte by byte: nothing here trusts its input. */

#include "core/utf8.h"

static int  continuation(unsigned char c)
{
    return ((c & 0xC0) == 0x80);
}

int utf8_decode(const char *s, size_t n, int32_t *cp)
{
    const unsigned char *u;
    int32_t             v;
    int                 len;
    int                 i;

    u = (const unsigned char *)s;
    *cp = -1;
    if (n == 0)
        return (0);
    if (u[0] < 0x80)
    {
        *cp = u[0];
        return (1);
    }
    if (u[0] >= 0xC2 && u[0] <= 0xDF)
        len = 2;
    else if (u[0] >= 0xE0 && u[0] <= 0xEF)
        len = 3;
    else if (u[0] >= 0xF0 && u[0] <= 0xF4)
        len = 4;
    else
        return (1);
    if ((size_t)len > n)
        return (1);
    v = u[0] & (0x7F >> len);
    for (i = 1; i < len; i++)
    {
        if (!continuation(u[i]))
            return (1);
        v = (v << 6) | (u[i] & 0x3F);
    }
    if ((len == 3 && v < 0x800) || (len == 4 && (v < 0x10000 || v > 0x10FFFF))
        || (v >= 0xD800 && v <= 0xDFFF))
        return (1);
    *cp = v;
    return (len);
}

int utf8_encode(int32_t cp, char *out)
{
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        return (1);
    }
    if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return (2);
    }
    if (cp < 0x10000)
    {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return (3);
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return (4);
}

int utf8_valid(const char *s, size_t n)
{
    size_t  i;
    int32_t cp;
    int     len;

    i = 0;
    while (i < n)
    {
        if ((unsigned char)s[i] < 0x80)
        {
            i++;
            continue ;
        }
        len = utf8_decode(s + i, n - i, &cp);
        if (cp == -1)
            return (0);
        i += (size_t)len;
    }
    return (1);
}

void    utf8_sanitize(const char *s, size_t n, t_buffer *b)
{
    size_t  i;
    int32_t cp;
    int     len;

    i = 0;
    while (i < n)
    {
        len = utf8_decode(s + i, n - i, &cp);
        if (cp == -1)
            buf_add_str(b, UTF8_REPLACEMENT);
        else
            buf_add(b, s + i, (size_t)len);
        i += (size_t)len;
    }
}

size_t  utf8_count(const char *s, size_t n)
{
    size_t  count;
    size_t  i;

    count = 0;
    for (i = 0; i < n; i++)
        count += !continuation((unsigned char)s[i]);
    return (count);
}

size_t  utf8_boundary(const char *s, size_t n)
{
    size_t  start;
    int32_t cp;

    if (n == 0)
        return (0);
    start = n - 1;
    while (start > 0 && n - start < 4 && continuation((unsigned char)s[start]))
        start--;
    if (utf8_decode(s + start, n - start, &cp) == (int)(n - start))
        return (n);
    return (start);
}
