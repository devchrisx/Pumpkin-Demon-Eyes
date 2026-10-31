/*
 * Pumpkin Demon Eyes  -  Blink / eyelid controller
 * ------------------------------------------------
 * Purpose  : Eye opening of ONE eye (0.0 = closed, 1.0 = fully open) and
 *            lid tension ("anger": slanted, lowered upper lid, squinting
 *            lower lid).
 *              openness = baseOpenness * blinkFactor
 *            baseOpenness changes slowly (sleep, wake up, squint), blink
 *            sequences (normal, fast, long, slow, double, twitch) modulate
 *            blinkFactor on top of it.
 */

#pragma once

#include "Tween.h"

enum class BlinkType : uint8_t
{
    Normal,
    Fast,
    Long,     // closes for a while
    Slow      // heavy, sleepy blink
};

class Rng;

class BlinkController
{
public:
    void  begin(Rng* rng, float openness);

    // Starts a blink after delayMs. repeats = 2 gives a double blink.
    // depth < 1 only partly closes the lid (twitch).
    void  blink(BlinkType type, uint32_t now, uint32_t delayMs = 0,
                uint8_t repeats = 1, float depth = 1.0f);

    void  setOpenness(float level, uint32_t now, uint32_t durationMs, Ease ease);
    void  setAnger(float level, uint32_t now, uint32_t durationMs, Ease ease);
    void  update(uint32_t now);

    float openness() const     { return openness_; }
    float baseOpenness() const { return base_.value(); }
    float anger() const        { return anger_.value(); }
    bool  blinking() const     { return phase_ != Phase::Idle; }

private:
    enum class Phase : uint8_t { Idle, Waiting, Closing, Closed, Opening, Gap };

    void  startPhase(Phase phase, uint32_t now, uint32_t durationMs);

    Rng*     rng_        = nullptr;
    Tween    base_;
    Tween    anger_;

    Phase    phase_      = Phase::Idle;
    uint32_t phaseStart_ = 0;
    uint32_t phaseDur_   = 0;
    uint32_t closeMs_    = 0;
    uint32_t holdMs_     = 0;
    uint32_t openMs_     = 0;
    uint8_t  remaining_  = 0;
    float    depth_      = 1.0f;
    float    factor_     = 1.0f;     // current blink multiplier
    float    factorFrom_ = 1.0f;     // multiplier when the current phase started
    float    openness_   = 1.0f;
};
