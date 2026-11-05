/*
 * f-string fields with a format specifier (the parser checked its shape):
 * Python's mini-language for text, integers and floats. Widths count
 * characters, so text that is not ASCII lines up as well.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core/utf8.h"
#include "runtime/interp_internal.h"

/* The fill character, n times. */
static void add_fill(t_buffer *b, int32_t fill, size_t n)
{
    char    enc[4];
    int     len;

    len = utf8_encode(fill, enc);
    while (n-- > 0)
        buf_add(b, enc, (size_t)len);
}

/*
 * Puts sign + prefix + body (digits or text) in b, padded to width with
 * the fill: '<' left, '>' right, '^' centred (the extra one on the
 * right, as Python), '=' between the sign/prefix and the digits.
 */
static void pad(t_buffer *b, const t_fmt_spec *f, char align,
                const char *head, const char *body, size_t body_len)
{
    size_t  len;
    size_t  gap;

    len = strlen(head) + utf8_count(body, body_len);
    gap = (size_t)f->width > len ? (size_t)f->width - len : 0;
    if (align == '>')
        add_fill(b, f->fill, gap);
    else if (align == '^')
        add_fill(b, f->fill, gap / 2);
    buf_add_str(b, head);
    if (align == '=')
        add_fill(b, f->fill, gap);
    buf_add(b, body, body_len);
    if (align == '<')
        add_fill(b, f->fill, gap);
    else if (align == '^')
        add_fill(b, f->fill, gap - gap / 2);
}

/*
 * Digits with a separator every group of them (3 in decimal, 4 in the
 * other bases), up to the '.' or the exponent.
 */
static void group(t_buffer *out, const char *d, size_t len, char sep,
                int every)
{
    size_t  digits;
    size_t  i;

    digits = 0;
    while (digits < len && d[digits] >= '0' && d[digits] <= '9')
        digits++;
    if (every == 4)
        while (digits < len && strchr("0123456789abcdefABCDEF", d[digits]))
            digits++;
    for (i = 0; i < digits; i++)
    {
        if (i > 0 && (digits - i) % (size_t)every == 0)
            buf_add_char(out, sep);
        buf_add_char(out, d[i]);
    }
    buf_add(out, d + digits, len - digits);
}

/* The sign to show before a number (negative: always '-'). */
static const char   *sign_of(const t_fmt_spec *f, int negative)
{
    if (negative)
        return ("-");
    if (f->sign == '+')
        return ("+");
    if (f->sign == ' ')
        return (" ");
    return ("");
}

/* An int in base 10, 2, 8 or 16: its magnitude, so INT64_MIN fits too. */
static void int_digits(t_buffer *b, int64_t v, char type)
{
    char        tmp[72];
    uint64_t    m;
    int         base;
    int         n;
    const char  *dig;

    base = type == 'b' ? 2 : type == 'o' ? 8 : (type == 'x' || type == 'X')
        ? 16 : 10;
    dig = type == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
    m = v < 0 ? 0 - (uint64_t)v : (uint64_t)v;
    n = 0;
    do
    {
        tmp[n++] = dig[m % (uint64_t)base];
        m /= (uint64_t)base;
    } while (m);
    while (n > 0)
        buf_add_char(b, tmp[--n]);
}

/* A float with f / e / g / % (or none with a precision: g), or its repr. */
static void float_digits(t_buffer *b, double v, const t_fmt_spec *f)
{
    char    conv;
    int     prec;

    prec = f->precision >= 0 ? f->precision : 6;
    conv = f->type ? f->type : 'g';
    if (!f->type && f->precision < 0)
    {
        rt_format_float(b, fabs(v));
        return ;
    }
    if (conv == '%')
    {
        v *= 100;
        conv = 'f';
    }
    if (conv == 'f' || conv == 'F')
        buf_printf(b, conv == 'f' ? "%.*f" : "%.*F", prec, fabs(v));
    else if (conv == 'e' || conv == 'E')
        buf_printf(b, conv == 'e' ? "%.*e" : "%.*E", prec, fabs(v));
    else
        buf_printf(b, conv == 'g' ? "%.*g" : "%.*G", prec, fabs(v));
}

static int  format_number(t_interp *in, t_buffer *out, t_value v,
                const t_fmt_spec *f, const t_node *n, t_error *err)
{
    char        head[8];
    t_buffer    *d;
    int         negative;
    int         is_int;
    double      x;

    is_int = (v.type == VAL_INT && (!f->type || strchr("dboxX", f->type)));
    if (v.type == VAL_FLOAT && f->type && strchr("dboxX", f->type))
        return (rt_error(err, ERR_TYPE, n, "format code '%c' needs an"
                " integer, not a float", f->type));
    d = &in->scratch;
    buf_clear(d);
    x = v.type == VAL_INT ? (double)v.as.integer : v.as.number;
    negative = is_int ? v.as.integer < 0 : signbit(x) != 0;
    if (is_int)
        int_digits(d, v.as.integer, f->type);
    else
        float_digits(d, x, f);
    if (f->type == '%')
        buf_add_char(d, '%');
    snprintf(head, sizeof(head), "%s%s", sign_of(f, negative),
        !f->alt ? "" : f->type == 'b' ? "0b" : f->type == 'o' ? "0o"
        : f->type == 'x' ? "0x" : "0X");
    buf_clear(&in->text);
    if (f->grouping)
        group(&in->text, d->data, d->len, f->grouping,
            f->type && strchr("boxX", f->type) ? 4 : 3);
    else
        buf_add(&in->text, d->data, d->len);
    buf_clear(d);
    buf_add(d, in->text.data, in->text.len);
    pad(out, f, f->align ? f->align : '>', head, d->data, d->len);
    return (0);
}

/*
 * v formatted by f, appended to out (which must not be in->text or
 * in->scratch: both are used here). Text (anything but a number, or a
 * number with 's') takes a precision as its maximum length.
 */
int rt_format_spec(t_interp *in, t_buffer *out, t_value v,
        const t_fmt_spec *spec, const t_node *n, t_error *err)
{
    t_fmt_spec  f;
    t_buffer    *b;
    size_t      len;
    int32_t     cp;
    int         k;

    f = *spec;
    if (f.zero && !f.align)
    {
        f.fill = '0';
        f.align = '=';
    }
    if ((v.type == VAL_INT || v.type == VAL_FLOAT) && f.type != 's')
        return (format_number(in, out, v, &f, n, err));
    if (f.type && f.type != 's')
        return (rt_error(err, ERR_TYPE, n, "format code '%c' needs a number,"
                " not '%s'", f.type, val_type_name(v)));
    b = rt_buf_take(in);
    if (rt_show(in, n, b, v, 0, err) == -1)
    {
        rt_buf_give(in);
        return (-1);
    }
    buf_clear(&in->scratch);
    buf_add(&in->scratch, b->data ? b->data : "", b->len);
    rt_buf_give(in);
    len = in->scratch.len;
    if (f.precision >= 0)
    {
        len = 0;
        for (k = 0; k < f.precision && len < in->scratch.len; k++)
            len += (size_t)utf8_decode(in->scratch.data + len,
                    in->scratch.len - len, &cp);
    }
    pad(out, &f, f.align == '=' ? '<' : f.align ? f.align : '<', "",
        in->scratch.data ? in->scratch.data : "", len);
    return (0);
}
