#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "core/reader.h"

char    *read_file(const char *path, size_t *len)
{
    int         fd;
    struct stat st;
    size_t      size;
    char        *buf;
    size_t      bytes_read;
    ssize_t     ret;

    fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return (NULL);
    }
    if (fstat(fd, &st) == -1)
    {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        goto err_close;
    }
    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr, "%s: not a regular file\n", path);
        goto err_close;
    }
    size = (size_t)st.st_size;
    buf = malloc(size + 1);
    if (!buf)
    {
        fprintf(stderr, "%s: out of memory\n", path);
        goto err_close;
    }

    bytes_read = 0;
    ret = 0;
    while (bytes_read < size)
    {
        ret = read(fd, buf + bytes_read, size - bytes_read);
        if (ret == 0)
        {
            fprintf(stderr, "%s: unexpected end of file\n", path);
            goto err_free;
        }
        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "%s: %s\n", path, strerror(errno));
            goto err_free;
        }
        bytes_read += ret;
    }

    buf[size] = '\0';
    close(fd);
    *len = size;
    return (buf);

err_free:
    free(buf);
err_close:
    close(fd);
    return (NULL);
}
