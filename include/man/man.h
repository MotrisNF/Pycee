#ifndef MAN_H
# define MAN_H

/*
 * The manual: man(topic) at the REPL's prompt. Its pages are written in
 * man/<topic>.txt and compressed into src/man/man_data.c by
 * tools/gen_man.py.
 * This layer uses only core: the runtime passes it the buffers it needs
 * (the session's, so running out of memory frees them).
 */

# include <stddef.h>
# include <stdint.h>
# include "core/buffer.h"

typedef struct s_man_entry
{
    const char  *topic;
    const char  *summary;
    uint32_t    offset;     /* in the decompressed text */
    uint32_t    len;
}   t_man_entry;

extern const unsigned char  g_man_blob[];
extern const size_t         g_man_blob_len;
extern const size_t         g_man_text_len;
extern const t_man_entry    g_man_entries[];
extern const int            g_man_count;

/*
 * What man(topic) shows, into page (title into title): the entry; for a
 * method several types have (len), the list of them; man() is
 * man(pycee). Returns 1 with a page, or 0 when there is none: page then
 * holds the message to give (with the closest topics). work is scratch.
 */
int     man_lookup(const char *topic, size_t len, t_buffer *page,
            t_buffer *title, t_buffer *work);

/*
 * Shows text: through the pager when both stdin and stdout are a
 * terminal (space/f, b, Enter, arrows, q), or all of it at once
 * otherwise. lines is scratch for the pager. Returns 1 if a signal
 * (Ctrl+C) ended it, 0 otherwise. The terminal is always left as found.
 */
int     man_show(const char *title, const char *text, size_t len,
            t_buffer *lines);

#endif
