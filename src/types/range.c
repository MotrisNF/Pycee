/*
 * range: start, stop and step, its numbers computed when asked for. The
 * arithmetic is unsigned so that no range can overflow, not even
 * range(-9223372036854775808, 9223372036854775807): the length and each
 * number always fit, only the steps in between might not.
 */

#include "runtime/interp_internal.h"

uint64_t    rt_range_len(const t_range *r)
{
    if (r->step > 0)
    {
        if (r->start >= r->stop)
            return (0);
        return (((uint64_t)r->stop - (uint64_t)r->start - 1)
            / (uint64_t)r->step + 1);
    }
    if (r->start <= r->stop)
        return (0);
    return (((uint64_t)r->start - (uint64_t)r->stop - 1)
        / (0 - (uint64_t)r->step) + 1);
}

/* The i-th number, i < rt_range_len(r). */
int64_t rt_range_at(const t_range *r, uint64_t i)
{
    return ((int64_t)((uint64_t)r->start + i * (uint64_t)r->step));
}
