#ifndef UTF8_H
# define UTF8_H

#include <stddef.h>
#include <stdint.h>
#include "core/buffer.h"

/* U+FFFD, what replaces every byte that is not part of valid UTF-8. */
# define UTF8_REPLACEMENT   "\xEF\xBF\xBD"

/*
 * The character at s, with n bytes available: its length (1 to 4) and
 * its code point in *cp. Anything that is not a valid, shortest encoding
 * of a scalar value (a stray byte, a truncated sequence, an overlong
 * form, a surrogate, past U+10FFFF) is 1 byte with *cp = -1.
 */
int     utf8_decode(const char *s, size_t n, int32_t *cp);

/* Writes cp (a valid code point) to out; returns how many bytes (1-4). */
int     utf8_encode(int32_t cp, char *out);

/* Whether [s, s + n) is valid UTF-8 from start to end. */
int     utf8_valid(const char *s, size_t n);

/* Appends [s, s + n) to b with each invalid byte replaced by U+FFFD. */
void    utf8_sanitize(const char *s, size_t n, t_buffer *b);

/* How many characters [s, s + n) holds (valid UTF-8). */
size_t  utf8_count(const char *s, size_t n);

/*
 * The longest prefix of [s, s + n) that does not cut a character in two
 * (to truncate text, as error messages are, without breaking UTF-8).
 */
size_t  utf8_boundary(const char *s, size_t n);

#endif
