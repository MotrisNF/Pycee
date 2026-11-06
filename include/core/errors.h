#ifndef ERRORS_H
# define ERRORS_H

#include <stddef.h>
#include <stdarg.h>
#include "core/buffer.h"
#include "core/source.h"

# define ERR_MSG_SIZE 256

/*
 * Error types, named as in Python.
 * Analysis (lexer, parser, resolver): SyntaxError, NameError, TypeError
 * (wrong number of arguments), AttributeError (self.field not declared)
 * and ImportError (module not found, circular import).
 * Runtime: ZeroDivisionError, ValueError, OverflowError (an int out of
 * 64 bits), RecursionError (stack guard) and NotImplementedError (parts
 * of the language not built yet; it disappears when they are).
 * SystemExit is not an error but travels like one, so everything on the
 * way out is unwound and freed: exit() in the REPL (code in t_error).
 * IndexError (an index out of range), AssignmentError (unpacking a
 * value into a different number of targets), EOFError (input() at the
 * end of the input), AssertionError; KeyError and RuntimeError come with
 * dicts and lists. IOError (files, the file system: io, os) has two
 * children of its own, FileNotFoundError and PermissionError. Error is
 * the base of them all. KeyboardInterrupt (Ctrl+C) is not an Error
 * either, like SystemExit: only an except that names it catches it.
 */
typedef enum e_error_type
{
    ERR_SYNTAX,
    ERR_NAME,
    ERR_TYPE,
    ERR_ATTRIBUTE,
    ERR_IMPORT,
    ERR_ZERO_DIVISION,
    ERR_VALUE,
    ERR_OVERFLOW,
    ERR_RECURSION,
    ERR_NOT_IMPLEMENTED,
    ERR_INDEX,
    ERR_ASSIGNMENT,
    ERR_KEY,
    ERR_RUNTIME,
    ERR_EOF,
    ERR_ASSERTION,
    ERR_IO,
    ERR_FILE_NOT_FOUND,
    ERR_PERMISSION,
    ERR_ERROR,
    ERR_EXIT,
    ERR_KEYBOARD_INTERRUPT,
    ERR_TYPE_COUNT
}   t_error_type;

/*
 * The traceback: one frame per function call the error went out of,
 * innermost first. Only TB_KEEP of the innermost and TB_KEEP of the
 * outermost are kept (a deep recursion would need thousands); total
 * counts them all.
 */
# define TB_KEEP 16

typedef struct s_frame_info
{
    size_t      offset;     /* the call, in the caller */
    size_t      len;
    const char  *name;      /* the function called (not '\0'-terminated) */
    size_t      name_len;
}   t_frame_info;

/*
 * Filled where the error is detected, printed by main.
 * offset is global (see source.h); len is the length of the offending token.
 * incomplete marks an error found at the end of the input, which more
 * text could fix (an unclosed block, "2 +"): the REPL then reads another
 * line instead of reporting it. code is the exit status of a SystemExit.
 * context: an error raised while handling another one (in an except or
 * a finally) points to that one, which is printed first; it lives in an
 * error value on the heap. self: raise e records e, so re-raising the
 * error being handled does not chain it to itself. instance and
 * type_name: an error that is an object of an obj extending an error
 * type (raise ParseError("x")): the object, and its obj's name to show.
 */
typedef struct s_error
{
    t_error_type    type;
    size_t          offset;
    size_t          len;
    int             incomplete;
    int             code;
    char            msg[ERR_MSG_SIZE];
    t_frame_info    inner[TB_KEEP];
    t_frame_info    outer[TB_KEEP];     /* a ring, after the inner ones */
    size_t          total;
    const char      *top_name;          /* the outermost frame: "main"... */
    size_t          top_len;
    const struct s_error    *context;   /* the error being handled */
    const void      *self;              /* the error value raise e raised */
    void            *instance;          /* an obj's error: that object */
    const char      *type_name;         /* ... and the name of its obj */
}   t_error;

const char  *error_type_name(t_error_type type);

/*
 * The hierarchy: every error is an Error (the base, which except Error
 * catches); SystemExit is not, so no except stops it. error_is_a(t, a):
 * t is a or descends from it. error_type_named: the type with that name,
 * or -1.
 */
int         error_is_a(t_error_type type, t_error_type ancestor);
int         error_type_named(const char *name, size_t len);

/*
 * Formats the message like printf and always returns -1, so callers
 * can write: return (error_set(err, ERR_SYNTAX, off, len, "...", ...));
 */
int         error_set(t_error *err, t_error_type type, size_t offset,
                size_t len, const char *fmt, ...)
                __attribute__((format(printf, 5, 6)));

/* Same as error_set, for callers that are variadic themselves. */
int         error_vset(t_error *err, t_error_type type, size_t offset,
                size_t len, const char *fmt, va_list ap)
                __attribute__((format(printf, 5, 0)));

/* Adds the call (at offset, len) of the function name the error left. */
void        error_add_frame(t_error *err, size_t offset, size_t len,
                const char *name, size_t name_len);

/*
 * Out of memory is the only fatal error: prints "MemoryError", runs the
 * registered cleanup and exits. Used by every allocation that cannot
 * reasonably propagate a failure.
 */
void        fatal_oom(void) __attribute__((noreturn));

/*
 * Registers what fatal_oom frees before exiting (the session), so an
 * out-of-memory exit leaves nothing allocated. fn NULL unregisters it.
 * The cleanup must not allocate.
 */
void        fatal_oom_set_cleanup(void (*fn)(void *), void *ctx);

/*
 * Prints to stderr:
 *   SyntaxError: expected ';' (file.pce, line 12)
 *       print(x)
 *               ^
 * The file and line are found from the global offset (see source.h).
 * An error that went out of function calls prints, as Python does, the
 * traceback (most recent call last) and the message at the end.
 * error_format appends that same text to b.
 */
void        error_format(const t_error *err, const t_sources *sources,
                t_buffer *b);
void        error_print(const t_error *err, const t_sources *sources);

#endif
