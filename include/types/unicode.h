#ifndef UNICODE_H
# define UNICODE_H

#include <stdint.h>

/*
 * Character classes and simple case mappings of Unicode, from tables
 * compiled in (src/types/unicode_data.c, made by tools/gen_unicode.py):
 * no locale, no allocation, the same answer on every system.
 */
int     uni_is_alpha(int32_t cp);
int     uni_is_space(int32_t cp);
int     uni_is_digit(int32_t cp);
int32_t uni_upper(int32_t cp);
int32_t uni_lower(int32_t cp);

#endif
