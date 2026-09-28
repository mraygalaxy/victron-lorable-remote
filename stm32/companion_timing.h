#pragma once

#include <stdbool.h>
#include <stdint.h>

/* All companion intervals are shorter than INT32_MAX milliseconds. Compare
 * their deadlines, not unsigned elapsed time: now may have been sampled before
 * a UART callback (or the caller) started a job in this same loop iteration.
 * Unsigned now - started would turn that few-millisecond skew into 49 days.
 */
static inline bool companionTimeoutReached(uint32_t now, uint32_t started,
                                           uint32_t timeout)
{
    return (int32_t)(now - (started + timeout)) >= 0;
}
