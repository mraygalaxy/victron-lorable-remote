#include "../../stm32/companion_timing.h"

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

int main(void)
{
    const uint32_t intervals[] = { 5000, 10000, 15000, 30000, 60000, 210000 };
    for (unsigned i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i) {
        const uint32_t timeout = intervals[i];
        const uint32_t starts[] = { 0, 171000, UINT32_MAX - 100, UINT32_MAX };
        for (unsigned j = 0; j < sizeof(starts) / sizeof(starts[0]); ++j) {
            const uint32_t started = starts[j];
            /* Job activation / ACCEPTED callback is newer than caller's now. */
            CHECK(!companionTimeoutReached(started - 1, started, timeout));
            CHECK(!companionTimeoutReached(started - 200, started, timeout));
            CHECK(!companionTimeoutReached(started, started, timeout));
            CHECK(!companionTimeoutReached(started + timeout - 1, started, timeout));
            CHECK(companionTimeoutReached(started + timeout, started, timeout));
            CHECK(companionTimeoutReached(started + timeout + 1, started, timeout));
        }
    }
    /* Reproduce the old false immediate timeout, including wraparound. */
    CHECK((uint32_t)(171000u - 171001u) >= 60000u);
    CHECK((uint32_t)(UINT32_MAX - 0u) >= 210000u);
    CHECK(!companionTimeoutReached(171000, 171001, 60000));
    CHECK(!companionTimeoutReached(UINT32_MAX, 0, 210000));
    return 0;
}
