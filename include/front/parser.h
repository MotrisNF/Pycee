#ifndef PARSER_H
# define PARSER_H

#include <stddef.h>
#include "core/arena.h"
#include "front/ast.h"
#include "core/errors.h"
#include "front/lexer.h"
#include "core/source.h"

typedef struct s_parser
{
    t_lexer     lx;
    t_token     cur;
    t_token     prev;
    size_t      last_end;   /* end of the last non-EOF token consumed */
    const t_source  *src;
    t_arena     *arena;
    t_error     *err;
    int         repl;       /* parse_repl: a final ';' may be left out */
}   t_parser;

/*
 * Parses a registered source. Names in the AST point into src->text,
 * which lives in the arena. Returns the root, or NULL with err filled.
 * All offsets (nodes, err) are global (see source.h).
 *
 * program = { import } { func | prog | obj | var } EOF (see docs/EBNF.md).
 */
t_node  *parse_program(t_arena *arena, const t_source *src, t_error *err);

/*
 * Parses what was typed in the REPL: any mix of imports, declarations and
 * simple statements (expressions, assignments), with the final ';'
 * optional. An error at the end of the input sets err->incomplete.
 */
t_node  *parse_repl(t_arena *arena, const t_source *src, t_error *err);

#endif
