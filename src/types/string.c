/*
 * Strings: characters (UTF-8 code points) and the methods of string.
 * Every string is valid UTF-8, so walking one never meets a bad byte;
 * indexes count characters, which on an ASCII string are bytes.
 */

#include <string.h>
#include "core/utf8.h"
#include "runtime/interp_internal.h"
#include "types/unicode.h"

#define SELF    (val_string(in->stack[base]))
#define ARG(i)  (in->stack[base + 1 + (i)])

size_t  rt_string_chars(t_string *s)
{
    if (s->nchars == SIZE_MAX)
        s->nchars = utf8_count(s->chars, s->len);
    return (s->nchars);
}

/* The byte where character i starts (i <= the number of characters). */
size_t  rt_string_offset(t_string *s, size_t i)
{
    size_t  pos;
    int32_t cp;

    if (rt_string_chars(s) == s->len)
        return (i);
    pos = 0;
    while (i-- > 0 && pos < s->len)
        pos += (size_t)utf8_decode(s->chars + pos, s->len - pos, &cp);
    return (pos);
}

/*
 * A new string from bytes of any origin (a file, the terminal, an error
 * message): if they are not valid UTF-8, each bad byte becomes U+FFFD.
 */
t_string    *rt_new_string(t_interp *in, const char *s, size_t len)
{
    if (utf8_valid(s, len))
        return (heap_string(&in->heap, s, len));
    buf_clear(&in->scratch);
    utf8_sanitize(s, len, &in->scratch);
    return (heap_string(&in->heap, in->scratch.data, in->scratch.len));
}

static int  string_out(t_interp *in, const char *s, size_t len, t_value *out)
{
    t_string    *str;

    str = heap_string(&in->heap, s, len);
    *out = val_obj(&str->obj);
    return (0);
}

typedef enum e_char_class
{
    CLASS_DIGIT,
    CLASS_ALPHA,
    CLASS_ALNUM,
    CLASS_SPACE
}   t_char_class;

/* Unicode's classes (types/unicode.h), as Python's isdigit / isalpha... */
static int  in_class(int32_t cp, t_char_class class)
{
    if (class == CLASS_DIGIT)
        return (uni_is_digit(cp));
    if (class == CLASS_ALPHA)
        return (uni_is_alpha(cp));
    if (class == CLASS_ALNUM)
        return (uni_is_alpha(cp) || uni_is_digit(cp));
    return (uni_is_space(cp));
}

static int  need_string(const t_node *call, const char *method, t_value v,
                t_error *err)
{
    if (val_is_string(v))
        return (0);
    return (rt_error(err, ERR_TYPE, call, "%s() needs a string, not '%s'",
            method, val_type_name(v)));
}

static int  m_len(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_int((int64_t)rt_string_chars(SELF));
    return (0);
}

/* mode: 0 lower, 1 upper, 2 capitalize (first upper, the rest lower). */
static int  recase(t_interp *in, size_t base, int mode, t_value *out)
{
    t_string    *s;
    char        enc[4];
    size_t      pos;
    int32_t     cp;
    int         len;

    s = SELF;
    buf_clear(&in->text);
    pos = 0;
    while (pos < s->len)
    {
        len = utf8_decode(s->chars + pos, s->len - pos, &cp);
        cp = (mode == 1 || (mode == 2 && pos == 0)) ? uni_upper(cp)
            : uni_lower(cp);
        buf_add(&in->text, enc, (size_t)utf8_encode(cp, enc));
        pos += (size_t)len;
    }
    return (string_out(in, in->text.data, in->text.len, out));
}

static int  m_upper(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (recase(in, base, 1, out));
}

static int  m_lower(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (recase(in, base, 0, out));
}

static int  m_capitalize(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (recase(in, base, 2, out));
}

/* Whether cp is to be stripped: whitespace, or one of chars. */
static int  strippable(int32_t cp, const t_string *chars)
{
    size_t  pos;
    int32_t c;

    if (!chars)
        return (in_class(cp, CLASS_SPACE));
    pos = 0;
    while (pos < chars->len)
    {
        pos += (size_t)utf8_decode(chars->chars + pos, chars->len - pos, &c);
        if (c == cp)
            return (1);
    }
    return (0);
}

/* The start of the character that ends at byte end. */
static size_t   char_before(const t_string *s, size_t end)
{
    size_t  start;

    start = end - 1;
    while (start > 0 && ((unsigned char)s->chars[start] & 0xC0) == 0x80)
        start--;
    return (start);
}

/* sides: 1 left, 2 right, 3 both. */
static int  strip(t_interp *in, size_t base, int nargs, int sides,
                const t_node *call, t_value *out, t_error *err)
{
    t_string    *s;
    t_string    *chars;
    size_t      start;
    size_t      end;
    int32_t     cp;
    int         len;

    chars = NULL;
    if (nargs == 1)
    {
        if (need_string(call, sides == 3 ? "strip" : sides == 1 ? "lstrip"
                : "rstrip", ARG(0), err) == -1)
            return (-1);
        chars = val_string(ARG(0));
    }
    s = SELF;
    start = 0;
    end = s->len;
    while ((sides & 1) && start < end)
    {
        len = utf8_decode(s->chars + start, end - start, &cp);
        if (!strippable(cp, chars))
            break ;
        start += (size_t)len;
    }
    while ((sides & 2) && end > start)
    {
        utf8_decode(s->chars + char_before(s, end), end - char_before(s, end),
            &cp);
        if (!strippable(cp, chars))
            break ;
        end = char_before(s, end);
    }
    return (string_out(in, s->chars + start, end - start, out));
}

static int  m_strip(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (strip(in, base, nargs, 3, call, out, err));
}

static int  m_lstrip(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (strip(in, base, nargs, 1, call, out, err));
}

static int  m_rstrip(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    return (strip(in, base, nargs, 2, call, out, err));
}

/* The first byte offset of sub in s at or after from, or -1. */
static int64_t  find_bytes(const t_string *s, const t_string *sub, size_t from)
{
    size_t  i;

    if (sub->len > s->len)
        return (-1);
    for (i = from; i + sub->len <= s->len; i++)
        if (memcmp(s->chars + i, sub->chars, sub->len) == 0)
            return ((int64_t)i);
    return (-1);
}

/* Whether sub is somewhere in s (x in s). */
int rt_string_contains(const t_string *s, const t_string *sub)
{
    return (find_bytes(s, sub, 0) != -1);
}

/* find(sub) / index(sub): the character index of the first match. */
static int  search(t_interp *in, size_t base, int must, const t_node *call,
                t_value *out, t_error *err)
{
    int64_t pos;

    if (need_string(call, must ? "index" : "find", ARG(0), err) == -1)
        return (-1);
    pos = find_bytes(SELF, val_string(ARG(0)), 0);
    if (pos == -1 && must)
        return (rt_error(err, ERR_VALUE, call, "index(): substring not"
                " found"));
    if (pos != -1)
        pos = (int64_t)utf8_count(SELF->chars, (size_t)pos);
    *out = val_int(pos);
    return (0);
}

static int  m_find(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (search(in, base, 0, call, out, err));
}

static int  m_index(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (search(in, base, 1, call, out, err));
}

/* count(sub): matches that do not overlap; "" is found between characters. */
static int  m_count(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_string  *sub;
    int64_t         pos;
    int64_t         n;

    (void)nargs;
    if (need_string(call, "count", ARG(0), err) == -1)
        return (-1);
    sub = val_string(ARG(0));
    if (sub->len == 0)
    {
        *out = val_int((int64_t)rt_string_chars(SELF) + 1);
        return (0);
    }
    n = 0;
    pos = find_bytes(SELF, sub, 0);
    while (pos != -1)
    {
        n++;
        pos = find_bytes(SELF, sub, (size_t)pos + sub->len);
    }
    *out = val_int(n);
    return (0);
}

/*
 * replace(old, new): every match, left to right. An empty old matches
 * between every two characters and at both ends, as in Python.
 */
static int  m_replace(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    const t_string  *s;
    const t_string  *old;
    const t_string  *new;
    size_t          pos;
    int64_t         at;
    int32_t         cp;

    (void)nargs;
    if (need_string(call, "replace", ARG(0), err) == -1
        || need_string(call, "replace", ARG(1), err) == -1)
        return (-1);
    s = SELF;
    old = val_string(ARG(0));
    new = val_string(ARG(1));
    buf_clear(&in->text);
    pos = 0;
    while (old->len == 0 && pos <= s->len)
    {
        buf_add(&in->text, new->chars, new->len);
        if (pos == s->len)
            break ;
        at = utf8_decode(s->chars + pos, s->len - pos, &cp);
        buf_add(&in->text, s->chars + pos, (size_t)at);
        pos += (size_t)at;
    }
    while (old->len > 0 && (at = find_bytes(s, old, pos)) != -1)
    {
        buf_add(&in->text, s->chars + pos, (size_t)at - pos);
        buf_add(&in->text, new->chars, new->len);
        pos = (size_t)at + old->len;
    }
    if (old->len > 0)
        buf_add(&in->text, s->chars + pos, s->len - pos);
    return (string_out(in, in->text.data, in->text.len, out));
}

static int  edge_match(t_interp *in, size_t base, int end, const t_node *call,
                t_value *out, t_error *err)
{
    const t_string  *s;
    const t_string  *x;

    if (need_string(call, end ? "ends_with" : "starts_with", ARG(0), err)
        == -1)
        return (-1);
    s = SELF;
    x = val_string(ARG(0));
    *out = val_bool(x->len <= s->len && memcmp(s->chars
                + (end ? s->len - x->len : 0), x->chars, x->len) == 0);
    return (0);
}

static int  m_starts_with(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (edge_match(in, base, 0, call, out, err));
}

static int  m_ends_with(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    return (edge_match(in, base, 1, call, out, err));
}

/* The characters in reverse order (each one stays whole). */
static int  m_reverse(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    const t_string  *s;
    size_t          end;
    size_t          start;

    (void)nargs;
    (void)call;
    (void)err;
    s = SELF;
    buf_clear(&in->text);
    end = s->len;
    while (end > 0)
    {
        start = char_before(s, end);
        buf_add(&in->text, s->chars + start, end - start);
        end = start;
    }
    return (string_out(in, in->text.data, in->text.len, out));
}

/* Whether every character is of class (and there is at least one). */
static int  every(t_interp *in, size_t base, t_char_class class, t_value *out)
{
    const t_string  *s;
    size_t          pos;
    int32_t         cp;

    s = SELF;
    pos = 0;
    while (pos < s->len)
    {
        pos += (size_t)utf8_decode(s->chars + pos, s->len - pos, &cp);
        if (!in_class(cp, class))
        {
            *out = val_bool(0);
            return (0);
        }
    }
    *out = val_bool(s->len > 0);
    return (0);
}

static int  m_is_digit(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (every(in, base, CLASS_DIGIT, out));
}

static int  m_is_alpha(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (every(in, base, CLASS_ALPHA, out));
}

static int  m_is_alnum(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (every(in, base, CLASS_ALNUM, out));
}

static int  m_is_space(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (every(in, base, CLASS_SPACE, out));
}

/* Every character below U+0080 (true for ""). */
static int  m_is_ascii(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    *out = val_bool(rt_string_chars(SELF) == SELF->len);
    return (0);
}

/*
 * sep.join(c): the strings of a list or a tuple, with sep between them.
 * Everything must be a string.
 */
int rt_join(t_interp *in, const t_node *call, const t_string *sep,
        t_value c, t_value *out, t_error *err)
{
    const t_value   *items;
    size_t          len;
    size_t          i;

    if (!val_is_tuple(c) && !val_is_list(c))
        return (rt_error(err, ERR_TYPE, call, "join() needs a list or a tuple"
                " of strings, not '%s'", val_type_name(c)));
    items = rt_seq_items(c, &len);
    buf_clear(&in->text);
    for (i = 0; i < len; i++)
    {
        if (!val_is_string(items[i]))
            return (rt_error(err, ERR_TYPE, call, "join() needs strings, but"
                    " item %zu is '%s'", i, val_type_name(items[i])));
        if (i > 0)
            buf_add(&in->text, sep->chars, sep->len);
        buf_add(&in->text, val_string(items[i])->chars,
            val_string(items[i])->len);
    }
    return (string_out(in, in->text.data, in->text.len, out));
}

static int  m_join(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    (void)nargs;
    return (rt_join(in, call, SELF, ARG(0), out, err));
}

/* The string at stack[slot] (a list) gets s[from, to) as a new string. */
static void add_piece(t_interp *in, size_t slot, const t_string *s,
                size_t from, size_t to)
{
    t_string    *piece;
    t_list      *l;

    piece = heap_string(&in->heap, s->chars + from, to - from);
    l = val_list(in->stack[slot]);
    heap_list_reserve(&in->heap, l, l->len + 1);
    l->items[l->len++] = val_obj(&piece->obj);
}

/* split(): on runs of whitespace, no empty pieces. */
static void split_space(t_interp *in, size_t slot, const t_string *s)
{
    size_t  pos;
    size_t  start;
    int32_t cp;
    int     len;

    pos = 0;
    while (pos < s->len)
    {
        len = utf8_decode(s->chars + pos, s->len - pos, &cp);
        if (in_class(cp, CLASS_SPACE))
        {
            pos += (size_t)len;
            continue ;
        }
        start = pos;
        while (pos < s->len)
        {
            len = utf8_decode(s->chars + pos, s->len - pos, &cp);
            if (in_class(cp, CLASS_SPACE))
                break ;
            pos += (size_t)len;
        }
        add_piece(in, slot, s, start, pos);
    }
}

/*
 * split() / split(sep): a list of the pieces. With sep, every piece
 * between two of them, empty ones too ("a,,b" gives three); an empty sep
 * is a ValueError.
 */
static int  m_split(t_interp *in, size_t base, int nargs, const t_node *call,
                t_value *out, t_error *err)
{
    const t_string  *sep;
    size_t          slot;
    size_t          pos;
    int64_t         at;

    if (nargs == 1 && need_string(call, "split", ARG(0), err) == -1)
        return (-1);
    if (nargs == 1 && val_string(ARG(0))->len == 0)
        return (rt_error(err, ERR_VALUE, call, "split(): empty separator"));
    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, 0)->obj));
    if (nargs == 0)
        split_space(in, slot, SELF);
    else
    {
        sep = val_string(ARG(0));
        pos = 0;
        while ((at = find_bytes(SELF, sep, pos)) != -1)
        {
            add_piece(in, slot, SELF, pos, (size_t)at);
            pos = (size_t)at + sep->len;
        }
        add_piece(in, slot, SELF, pos, SELF->len);
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

/* to_list() (its characters) or enumerate() (tuple(index, character)). */
static int  chars_list(t_interp *in, size_t base, int numbered,
                t_value *out)
{
    t_string    *c;
    t_tuple     *t;
    t_list      *l;
    size_t      slot;
    size_t      pos;
    int32_t     cp;
    int         len;

    slot = in->top;
    rt_push(in, val_obj(&heap_list(&in->heap, rt_string_chars(SELF))->obj));
    pos = 0;
    while (pos < SELF->len)
    {
        len = utf8_decode(SELF->chars + pos, SELF->len - pos, &cp);
        c = heap_string(&in->heap, SELF->chars + pos, (size_t)len);
        c->nchars = 1;
        pos += (size_t)len;
        rt_push(in, val_obj(&c->obj));
        if (numbered)
        {
            t = heap_tuple(&in->heap, 2);
            t->items[0] = val_int((int64_t)val_list(in->stack[slot])->len);
            t->items[1] = in->stack[in->top - 1];
            in->stack[in->top - 1] = val_obj(&t->obj);
        }
        l = val_list(in->stack[slot]);
        heap_list_reserve(&in->heap, l, l->len + 1);
        l->items[l->len++] = rt_pop(in);
    }
    *out = in->stack[slot];
    in->top = slot;
    return (0);
}

static int  m_to_list(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (chars_list(in, base, 0, out));
}

static int  m_enumerate(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err)
{
    (void)nargs;
    (void)call;
    (void)err;
    return (chars_list(in, base, 1, out));
}

const t_method  g_string_methods[] = {
    {"split", 0, 1, m_split, NULL},
    {"to_list", 0, 0, m_to_list, NULL},
    {"enumerate", 0, 0, m_enumerate, NULL},
    {"len", 0, 0, m_len, NULL},
    {"upper", 0, 0, m_upper, NULL},
    {"lower", 0, 0, m_lower, NULL},
    {"capitalize", 0, 0, m_capitalize, NULL},
    {"strip", 0, 1, m_strip, NULL},
    {"lstrip", 0, 1, m_lstrip, NULL},
    {"rstrip", 0, 1, m_rstrip, NULL},
    {"find", 1, 1, m_find, NULL},
    {"index", 1, 1, m_index, NULL},
    {"count", 1, 1, m_count, NULL},
    {"replace", 2, 2, m_replace, NULL},
    {"starts_with", 1, 1, m_starts_with, NULL},
    {"ends_with", 1, 1, m_ends_with, NULL},
    {"reverse", 0, 0, m_reverse, NULL},
    {"is_digit", 0, 0, m_is_digit, NULL},
    {"is_alpha", 0, 0, m_is_alpha, NULL},
    {"is_alnum", 0, 0, m_is_alnum, NULL},
    {"is_space", 0, 0, m_is_space, NULL},
    {"is_ascii", 0, 0, m_is_ascii, NULL},
    {"join", 1, 1, m_join, NULL},
    {NULL, 0, 0, NULL, NULL},
};
