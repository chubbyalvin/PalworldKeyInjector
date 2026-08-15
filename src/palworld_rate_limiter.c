#include "palworld_keyinjector_internal.h"

enum {
    PAL_RATE_BURST_TOKENS = 4,
    PAL_RATE_REFILL_MS = 75,
};

void pal_rate_limiter_initialize(pal_rate_limiter *limiter, uint64_t now_ms)
{
    if (limiter == NULL) {
        return;
    }
    limiter->last_refill_ms = now_ms;
    limiter->tokens = PAL_RATE_BURST_TOKENS;
}

int pal_rate_limiter_try_consume(pal_rate_limiter *limiter, uint64_t now_ms)
{
    uint64_t elapsed;
    uint64_t refill_count;

    if (limiter == NULL) {
        return 0;
    }

    if (now_ms < limiter->last_refill_ms) {
        limiter->last_refill_ms = now_ms;
    }

    elapsed = now_ms - limiter->last_refill_ms;
    refill_count = elapsed / PAL_RATE_REFILL_MS;
    if (refill_count > 0) {
        uint64_t tokens = (uint64_t)limiter->tokens + refill_count;
        limiter->tokens = (unsigned int)(tokens > PAL_RATE_BURST_TOKENS
            ? PAL_RATE_BURST_TOKENS : tokens);
        limiter->last_refill_ms += refill_count * PAL_RATE_REFILL_MS;
    }

    if (limiter->tokens == 0) {
        return 0;
    }
    --limiter->tokens;
    return 1;
}
