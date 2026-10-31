/*
 * Pumpkin Demon Eyes  -  Random source
 * ------------------------------------
 * Purpose  : Small, fast xorshift32 generator with helpers for the
 *            animation (ranges, chances, weighted choice). One instance is
 *            shared by the whole animation so a fixed RANDOM_SEED gives a
 *            reproducible sequence.
 */

#pragma once

#include <stdint.h>

class Rng
{
public:
    void     seed(uint32_t seed);

    uint32_t next();                                  // raw 32-bit value
    float    uniform();                               // [0, 1)
    float    range(float lo, float hi);               // [lo, hi)
    uint32_t rangeU32(uint32_t lo, uint32_t hi);      // [lo, hi] inclusive
    bool     chance(float probability);               // true with given probability
    float    signedUnit();                            // [-1, 1)

    // Picks an index 0..count-1, proportional to the weights.
    // Returns -1 if all weights are zero.
    int      weighted(const uint8_t* weights, uint8_t count);

private:
    uint32_t state_ = 0x9E3779B9u;
};
