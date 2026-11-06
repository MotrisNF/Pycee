/*
 * Looking up the manual: the decompressor of the pages (see
 * tools/gen_man.py), the search by topic, the lists for a method of
 * several types, and the suggestions for a topic that does not exist.
 */

#include <string.h>
#include "man/man.h"

/*
 * Tokens: c < 0x80, c + 1 literal bytes follow; c >= 0x80, (c & 0x7f) + 4
 * bytes are copied from o bytes back (o: 2 bytes, little endian). Every
 * read and copy is checked, though the data is generated here: 0 if the
 * blob does not make sense.
 */
static int  decompress(t_buffer *out)
{
    const unsigned char *b;
    size_t              i;
    size_t              n;
    size_t              off;
    size_t              k;

    b = g_man_blob;
    buf_clear(out);
    i = 0;
    while (i < g_man_blob_len)
    {
        if (b[i] < 0x80)
        {
            n = (size_t)b[i] + 1;
            if (i + 1 + n > g_man_blob_len)
                return (0);
            buf_add(out, (const char *)b + i + 1, n);
            i += 1 + n;
            continue ;
        }
        if (i + 3 > g_man_blob_len)
            return (0);
        n = (size_t)(b[i] & 0x7F) + 4;
        off = (size_t)b[i + 1] | ((size_t)b[i + 2] << 8);
        if (off == 0 || off > out->len)
            return (0);
        for (k = 0; k < n; k++)
            buf_add_char(out, out->data[out->len - off]);
        i += 3;
    }
    return (out->len == g_man_text_len);
}

static int  topic_cmp(const char *topic, size_t len, const char *name)
{
    int c;

    c = strncmp(topic, name, len);
    if (c != 0)
        return (c);
    return (name[len] ? -1 : 0);
}

static const t_man_entry    *find(const char *topic, size_t len)
{
    int lo;
    int hi;
    int mid;
    int c;

    lo = 0;
    hi = g_man_count - 1;
    while (lo <= hi)
    {
        mid = lo + (hi - lo) / 2;
        c = topic_cmp(topic, len, g_man_entries[mid].topic);
        if (c == 0)
            return (&g_man_entries[mid]);
        if (c < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return (NULL);
}

/* Entries "type.topic" (a method, a member of a module), in order. */
static int  qualified(const char *topic, size_t len, const t_man_entry **one)
{
    const char  *dot;
    int         count;
    int         i;

    count = 0;
    for (i = 0; i < g_man_count; i++)
    {
        dot = strchr(g_man_entries[i].topic, '.');
        if (dot && strlen(dot + 1) == len && memcmp(dot + 1, topic, len) == 0)
        {
            if (count++ == 0)
                *one = &g_man_entries[i];
        }
    }
    return (count);
}

static int  min3(int a, int b, int c)
{
    if (b < a)
        a = b;
    return (c < a ? c : a);
}

/* Edit distance (letters added, removed or changed), up to 63 chars. */
static int  distance(const char *a, size_t na, const char *b)
{
    int     row[64];
    size_t  nb;
    size_t  i;
    size_t  j;
    int     prev;
    int     tmp;

    nb = strlen(b);
    if (na > 63 || nb > 63)
        return (99);
    for (j = 0; j <= nb; j++)
        row[j] = (int)j;
    for (i = 1; i <= na; i++)
    {
        prev = row[0];
        row[0] = (int)i;
        for (j = 1; j <= nb; j++)
        {
            tmp = row[j];
            row[j] = min3(row[j] + 1, row[j - 1] + 1,
                    prev + (a[i - 1] != b[j - 1]));
            prev = tmp;
        }
    }
    return (row[nb]);
}

/* The part of a topic after its dot, if it has one. */
static const char   *last_part(const char *topic)
{
    const char  *dot;

    dot = strchr(topic, '.');
    return (dot ? dot + 1 : topic);
}

/*
 * Whether name is close to the topic typed: at distance d (letters
 * added, removed or changed), or, for d 3, one starts with the other
 * (lenght -> len), on the whole topic or its part after the dot.
 */
static int  close_to(const char *topic, size_t len, const char *name, int d)
{
    const char  *part;
    size_t      n;

    part = last_part(name);
    if (d < 3)
        return (distance(topic, len, name) == d
            || distance(topic, len, part) == d);
    n = strlen(part);
    if (len < 3 || n < 3 || distance(topic, len, part) <= 2)
        return (0);
    return (strncmp(part, topic, n < len ? n : len) == 0);
}

/* "No manual entry for 'x'", with up to 5 of the closest topics. */
static void not_found(const char *topic, size_t len, t_buffer *page)
{
    int found;
    int d;
    int i;

    buf_printf(page, "No manual entry for '%.*s'.", (int)len, topic);
    found = 0;
    for (d = 1; d <= 3 && found < 5; d++)
    {
        for (i = 0; i < g_man_count && found < 5; i++)
        {
            if (!close_to(topic, len, g_man_entries[i].topic, d))
                continue ;
            buf_add_str(page, found++ == 0 ? " Did you mean: " : ", ");
            buf_add_str(page, g_man_entries[i].topic);
        }
    }
    buf_add_str(page, found ? "?\n" : "\nman() lists every entry.\n");
}

static int  is_name_char(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_' || c == '.');
}

/* The page for a method several types have: one line per type. */
static void several(const char *topic, size_t len, t_buffer *page)
{
    const char  *dot;
    int         i;

    buf_printf(page, "'%.*s' is in several types or modules:\n\n",
        (int)len, topic);
    for (i = 0; i < g_man_count; i++)
    {
        dot = strchr(g_man_entries[i].topic, '.');
        if (dot && strlen(dot + 1) == len && memcmp(dot + 1, topic, len) == 0)
            buf_printf(page, "    %-24s %s\n", g_man_entries[i].topic,
                g_man_entries[i].summary);
    }
    buf_printf(page, "\nType man(type.%.*s) to read one of them.\n",
        (int)len, topic);
}

int man_lookup(const char *topic, size_t len, t_buffer *page,
        t_buffer *title, t_buffer *work)
{
    const t_man_entry   *e;
    size_t              i;
    int                 count;

    buf_clear(page);
    buf_clear(title);
    if (len == 0)
    {
        topic = "pycee";
        len = 5;
    }
    for (i = 0; i < len; i++)
    {
        if (!is_name_char(topic[i]))
        {
            buf_printf(page, "No manual entry for '%.*s': every operator"
                " and symbol is explained in man(operators).\n", (int)len,
                topic);
            return (0);
        }
    }
    e = find(topic, len);
    count = 0;
    if (!e && !memchr(topic, '.', len))
        count = qualified(topic, len, &e);
    if (!e)
    {
        not_found(topic, len, page);
        return (0);
    }
    if (count > 1)
    {
        buf_printf(title, "man(%.*s)", (int)len, topic);
        several(topic, len, page);
        return (1);
    }
    buf_printf(title, "man(%s)", e->topic);
    if (!decompress(work))
    {
        buf_add_str(page, "The manual is damaged: rebuild pycee.\n");
        return (0);
    }
    buf_add(page, work->data + e->offset, e->len);
    return (1);
}
