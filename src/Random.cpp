/*
 * Pumpkin Demon Eyes  -  Random source
 * ------------------------------------
 * Purpose  : xorshift32 implementation (see Random.h)
 */

#include "Random.h"

// ============================================================
// SEEDING / RAW OUTPUT
// ============================================================

void Rng::seed(uint32_t seed)
{
    state_ = seed ? seed : 0x9E3779B9u;    // xorshift must never hold 0
}

uint32_t Rng::next()
{
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
}

// ============================================================
// HELPERS
// ============================================================

float Rng::uniform()
{
    return (next() >> 8) * (1.0f / 16777216.0f);   // 24 random bits
}

float Rng::range(float lo, float hi)
{
    return lo + (hi - lo) * uniform();
}

uint32_t Rng::rangeU32(uint32_t lo, uint32_t hi)
{
    if (hi <= lo) return lo;
    return lo + next() % (hi - lo + 1);
}

bool Rng::chance(float probability)
{
    return uniform() < probability;
}

float Rng::signedUnit()
{
    return uniform() * 2.0f - 1.0f;
}

int Rng::weighted(const uint8_t* weights, uint8_t count)
{
    uint32_t total = 0;
    for (uint8_t i = 0; i < count; ++i) total += weights[i];
    if (total == 0) return -1;

    uint32_t pick = next() % total;
    for (uint8_t i = 0; i < count; ++i)
    {
        if (pick < weights[i]) return i;
        pick -= weights[i];
    }
    return count - 1;
}
