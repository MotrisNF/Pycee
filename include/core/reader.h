#ifndef READER_H
# define READER_H

#include <stddef.h>

/*
 * Reads a whole regular file into a malloc'd, '\0'-terminated buffer and
 * stores its length in len. On failure prints "path: reason" to stderr
 * and returns NULL.
 */
char    *read_file(const char *path, size_t *len);

#endif
