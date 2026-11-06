/*
 * The manual's pager, so pycee needs no less: the page on the terminal's
 * alternate screen (what was on the screen comes back when it ends), a
 * status line at the bottom, and the keys
 *     space, f, Page Down     next page        b, Page Up       previous
 *     Enter, j, Down          one line down    k, Up            one up
 *     g, Home                 the top          G, End           the end
 *     q                       back to the REPL
 * The terminal is put in raw mode and always restored: on q, on a
 * signal (Ctrl+C makes read fail with EINTR), on a read error. Nothing
 * is allocated while it runs: the start of every screen line is worked
 * out before.
 */

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "man/man.h"

typedef struct s_pager
{
    const char  *title;
    const char  *text;
    size_t      *starts;    /* nlines + 1 offsets: line i is [i, i + 1) */
    size_t      nlines;
    size_t      top;
    int         rows;
    int         cols;
}   t_pager;

static void out(const char *s, size_t n)
{
    ssize_t w;

    while (n > 0)
    {
        w = write(STDOUT_FILENO, s, n);
        if (w <= 0 && errno != EINTR)
            return ;
        if (w > 0)
        {
            s += w;
            n -= (size_t)w;
        }
    }
}

static void outs(const char *s)
{
    out(s, strlen(s));
}

/* Bytes of the UTF-8 character that starts with c. */
static size_t   char_len(unsigned char c)
{
    if (c >= 0xF0)
        return (4);
    if (c >= 0xE0)
        return (3);
    if (c >= 0xC0)
        return (2);
    return (1);
}

/*
 * Splits text into screen lines of at most cols characters (a longer
 * line goes on in the next one), into lines as offsets.
 */
static void split_lines(const char *text, size_t len, int cols,
                t_buffer *lines)
{
    size_t  pos;
    size_t  start;
    int     width;

    buf_clear(lines);
    pos = 0;
    while (pos < len)
    {
        start = pos;
        buf_add(lines, (const char *)&start, sizeof(start));
        width = 0;
        while (pos < len && text[pos] != '\n' && width < cols)
        {
            pos += char_len((unsigned char)text[pos]);
            width++;
        }
        if (pos < len && text[pos] == '\n')
            pos++;
    }
    buf_add(lines, (const char *)&len, sizeof(len));
}

static size_t   page_rows(const t_pager *p)
{
    return (p->rows > 1 ? (size_t)p->rows - 1 : 1);
}

static size_t   last_top(const t_pager *p)
{
    return (p->nlines > page_rows(p) ? p->nlines - page_rows(p) : 0);
}

static void draw(const t_pager *p)
{
    char    status[160];
    size_t  i;
    size_t  line;
    size_t  end;
    size_t  shown;

    outs("\033[H");
    for (i = 0; i < page_rows(p); i++)
    {
        line = p->top + i;
        if (line < p->nlines)
        {
            end = p->starts[line + 1];
            while (end > p->starts[line] && p->text[end - 1] == '\n')
                end--;
            out(p->text + p->starts[line], end - p->starts[line]);
        }
        outs("\033[K\n");
    }
    shown = p->top + page_rows(p);
    if (shown > p->nlines)
        shown = p->nlines;
    snprintf(status, sizeof(status), " %s  lines %zu-%zu of %zu%s   "
        "space: next  b: back  q: quit ", p->title, p->nlines ? p->top + 1 : 0,
        shown, p->nlines, shown >= p->nlines ? " (end)" : "");
    outs("\033[7m");
    out(status, strlen(status) < (size_t)p->cols ? strlen(status)
        : (size_t)p->cols);
    outs("\033[0m\033[K");
}

/*
 * The next key, escape sequences read whole (a lone ESC is waited for
 * 50 ms at most). -1 when read fails: a signal (Ctrl+C) or the end.
 */
static int  read_key(void)
{
    struct pollfd   pfd;
    unsigned char   c[4];

    if (read(STDIN_FILENO, c, 1) != 1)
        return (-1);
    if (c[0] != 033)
        return (c[0]);
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 50) <= 0 || read(STDIN_FILENO, c + 1, 1) != 1
        || c[1] != '[' || read(STDIN_FILENO, c + 2, 1) != 1)
        return (033);
    if (c[2] == 'A' || c[2] == 'B' || c[2] == 'H' || c[2] == 'F')
        return (c[2] == 'A' ? 'k' : c[2] == 'B' ? 'j' : c[2] == 'H' ? 'g'
            : 'G');
    if ((c[2] == '5' || c[2] == '6') && read(STDIN_FILENO, c + 3, 1) == 1
        && c[3] == '~')
        return (c[2] == '5' ? 'b' : ' ');
    return (033);
}

/* Moves the view for key; 0 when it is q. */
static int  step(t_pager *p, int key)
{
    if (key == 'q' || key == 'Q')
        return (0);
    if (key == ' ' || key == 'f')
        p->top += page_rows(p);
    else if (key == 'b')
        p->top = p->top > page_rows(p) ? p->top - page_rows(p) : 0;
    else if (key == '\r' || key == '\n' || key == 'j')
        p->top++;
    else if (key == 'k' && p->top > 0)
        p->top--;
    else if (key == 'g')
        p->top = 0;
    else if (key == 'G')
        p->top = last_top(p);
    if (p->top > last_top(p))
        p->top = last_top(p);
    return (1);
}

static int  run(t_pager *p)
{
    int key;

    while (1)
    {
        draw(p);
        key = read_key();
        if (key == -1)
            return (errno == EINTR);
        if (!step(p, key))
            return (0);
    }
}

int man_show(const char *title, const char *text, size_t len,
        t_buffer *lines)
{
    struct termios  saved;
    struct termios  raw;
    struct winsize  ws;
    t_pager         p;
    int             interrupted;

    fflush(stdout);
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)
        || tcgetattr(STDIN_FILENO, &saved) == -1)
    {
        fwrite(text, 1, len, stdout);
        fflush(stdout);
        return (0);
    }
    p.rows = 24;
    p.cols = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 1
        && ws.ws_col > 10)
    {
        p.rows = ws.ws_row;
        p.cols = ws.ws_col;
    }
    split_lines(text, len, p.cols, lines);
    p.title = title;
    p.text = text;
    p.starts = (size_t *)lines->data;
    p.nlines = lines->len / sizeof(size_t) - 1;
    p.top = 0;
    raw = saved;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    outs("\033[?1049h\033[?25l");
    interrupted = run(&p);
    outs("\033[?25h\033[?1049l");
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    return (interrupted);
}
