/*
 * Pumpkin Demon Eyes  -  Blink / eyelid controller
 * ------------------------------------------------
 * Purpose  : Implementation, see BlinkController.h
 */

#include "BlinkController.h"

#include "Config.h"
#include "Random.h"

// ============================================================
// PUBLIC API
// ============================================================

void BlinkController::begin(Rng* rng, float openness)
{
    rng_ = rng;
    base_.set(openness);
    anger_.set(0.0f);
    phase_    = Phase::Idle;
    factor_   = 1.0f;
    openness_ = openness;
}

void BlinkController::blink(BlinkType type, uint32_t now, uint32_t delayMs,
                            uint8_t repeats, float depth)
{
    switch (type)
    {
        case BlinkType::Normal:
            closeMs_ = BLINK_CLOSE_MS;      holdMs_ = BLINK_HOLD_MS;      openMs_ = BLINK_OPEN_MS;
            break;
        case BlinkType::Fast:
            closeMs_ = BLINK_FAST_CLOSE_MS; holdMs_ = BLINK_FAST_HOLD_MS; openMs_ = BLINK_FAST_OPEN_MS;
            break;
        case BlinkType::Long:
            closeMs_ = BLINK_LONG_CLOSE_MS;
            holdMs_  = rng_->rangeU32(BLINK_LONG_HOLD_MS_MIN, BLINK_LONG_HOLD_MS_MAX);
            openMs_  = BLINK_LONG_OPEN_MS;
            break;
        case BlinkType::Slow:
            closeMs_ = BLINK_SLOW_CLOSE_MS; holdMs_ = BLINK_SLOW_HOLD_MS; openMs_ = BLINK_SLOW_OPEN_MS;
            break;
    }

    // Slight natural variation of every blink
    closeMs_ = closeMs_ * rng_->range(0.85f, 1.15f);
    openMs_  = openMs_  * rng_->range(0.85f, 1.2f);

    remaining_ = repeats ? repeats : 1;
    depth_     = clampf(depth, 0.0f, 1.0f);

    if (delayMs > 0) startPhase(Phase::Waiting, now, delayMs);
    else             startPhase(Phase::Closing, now, closeMs_);
}

void BlinkController::setOpenness(float level, uint32_t now, uint32_t durationMs, Ease ease)
{
    base_.to(clampf(level, 0.0f, 1.0f), now, durationMs, ease);
}

void BlinkController::setAnger(float level, uint32_t now, uint32_t durationMs, Ease ease)
{
    anger_.to(clampf(level, 0.0f, 1.0f), now, durationMs, ease);
}

void BlinkController::update(uint32_t now)
{
    base_.update(now);
    anger_.update(now);

    // Advance through as many phases as have elapsed (robust against slow frames)
    for (int guard = 0; guard < 8 && phase_ != Phase::Idle; ++guard)
    {
        uint32_t elapsed = now - phaseStart_;
        float    t       = phaseDur_ ? (float)elapsed / (float)phaseDur_ : 1.0f;
        float    closed  = 1.0f - depth_;               // factor when "closed"

        if (t < 1.0f)
        {
            switch (phase_)
            {
                case Phase::Closing: factor_ = lerpf(factorFrom_, closed, applyEase(Ease::InQuad, t));  break;
                case Phase::Opening: factor_ = lerpf(closed, 1.0f, applyEase(Ease::OutQuad, t));        break;
                case Phase::Closed:  factor_ = closed;                                                  break;
                default: break;
            }
            break;
        }

        // Phase finished -> next phase starts exactly where this one ended
        uint32_t phaseEnd = phaseStart_ + phaseDur_;
        switch (phase_)
        {
            case Phase::Waiting:
            case Phase::Gap:
                startPhase(Phase::Closing, phaseEnd, closeMs_);
                break;
            case Phase::Closing:
                factor_ = closed;
                startPhase(Phase::Closed, phaseEnd, holdMs_);
                break;
            case Phase::Closed:
                startPhase(Phase::Opening, phaseEnd, openMs_);
                break;
            case Phase::Opening:
                factor_ = 1.0f;
                if (--remaining_ > 0) startPhase(Phase::Gap, phaseEnd, DOUBLE_BLINK_GAP_MS);
                else                  phase_ = Phase::Idle;
                break;
            case Phase::Idle:
                break;
        }
    }

    openness_ = clampf(base_.value() * factor_, 0.0f, 1.0f);
}

// ============================================================
// INTERNALS
// ============================================================

void BlinkController::startPhase(Phase phase, uint32_t now, uint32_t durationMs)
{
    phase_      = phase;
    phaseStart_ = now;
    phaseDur_   = durationMs;
    factorFrom_ = factor_;
}
