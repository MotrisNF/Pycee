/* The position an index stands for in a tuple, a list or a string. */

#include "runtime/interp_internal.h"

/*
 * The position i stands for in a sequence of len items: negative counts
 * from the end. -1 (with an IndexError) when it is out of range.
 */
int rt_seq_index(const t_node *n, const char *type, size_t len, int64_t i,
        size_t *out, t_error *err)
{
    if (i < 0 && (uint64_t)(0 - (uint64_t)i) <= len)
        *out = len - (size_t)(0 - (uint64_t)i);
    else if (i >= 0 && (uint64_t)i < len)
        *out = (size_t)i;
    else
        return (rt_error(err, ERR_INDEX, n, "%s index %lld out of range"
                " (length %zu)", type, (long long)i, len));
    return (0);
}

