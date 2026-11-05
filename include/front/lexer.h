#ifndef LEXER_H
# define LEXER_H

#include <stddef.h>
#include "front/tokens.h"
#include "core/errors.h"
#include "core/source.h"

/*
 * Produces tokens on demand from a source. pos is local to the source
 * text; every offset it hands out (tokens, errors) is global: base + pos.
 */
typedef struct s_lexer
{
    const char  *src;
    size_t      len;
    size_t      base;
    size_t      pos;
}   t_lexer;

void    lexer_init(t_lexer *lx, const t_source *src);

/*
 * Stores the next token in tok and returns 0, or fills err and returns -1.
 * After the last token it keeps returning TOK_EOF (offset = base + len,
 * len = 0).
 */
int     lexer_next(t_lexer *lx, t_token *tok, t_error *err);

/*
 * Inside an f-string, s[start] is just after the '{' of a field: the
 * index of its closing '}' (at depth 0, past nested brackets and
 * strings), with *colon at the ':' that starts its format (== the '}'
 * index when there is none). A ':' that closes a '?' is the ternary's.
 * (size_t)-1 if the text ends first.
 */
size_t  lex_fstring_field(const char *s, size_t len, size_t start,
            size_t *colon);

#endif
