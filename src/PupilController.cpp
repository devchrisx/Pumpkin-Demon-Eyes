/*
 * Pumpkin Demon Eyes  -  Pupil controller
 * ---------------------------------------
 * Purpose  : Implementation, see PupilController.h
 */

#include "PupilController.h"

#include <math.h>

#include "Config.h"
#include "Random.h"

// ============================================================
// PUBLIC API
// ============================================================

void PupilController::begin(Rng* rng)
{
    rng_ = rng;
    sizeT_.set(PUPIL_SIZE_DEFAULT);
    slit_.set(PUPIL_SLIT_DEFAULT);
    glowT_.set(GLOW_IDLE);
    core_.set(0.0f);
    hippusAmp_   = PUPIL_HIPPUS_AMPLITUDE;
    flickerAmt_  = GLOW_FLICKER_AMOUNT;
    phaseOffset_ = rng_->range(0.0f, 100.0f);
    size_        = PUPIL_SIZE_DEFAULT;
    glow_        = GLOW_IDLE;
}

void PupilController::setSize(float size, uint32_t now, uint32_t durationMs, Ease ease)
{
    sizeT_.to(clampf(size, 0.0f, 1.0f), now, durationMs * ANIM_PUPIL_SPEED, ease);
}

void PupilController::setSlit(float slit, uint32_t now, uint32_t durationMs, Ease ease)
{
    slit_.to(clampf(slit, 0.0f, 1.0f), now, durationMs * ANIM_PUPIL_SPEED, ease);
}

void PupilController::setGlow(float glow, uint32_t now, uint32_t durationMs, Ease ease)
{
    glowT_.to(glow < 0.0f ? 0.0f : glow, now, durationMs, ease);
}

void PupilController::setCoreGlow(float core, uint32_t now, uint32_t durationMs, Ease ease)
{
    core_.to(clampf(core, 0.0f, 1.0f), now, durationMs, ease);
}

void PupilController::setUnrest(float hippusAmplitude, float flickerAmount)
{
    hippusAmp_  = hippusAmplitude;
    flickerAmt_ = flickerAmount;
}

void PupilController::update(uint32_t now)
{
    sizeT_.update(now);
    slit_.update(now);
    glowT_.update(now);
    core_.update(now);

    // Seconds, kept small so float precision stays good for weeks of uptime
    float t = (float)(now % 600000u) * 0.001f + phaseOffset_;

    // Hippus: slow, irregular breathing of the pupil size
    float hippus = hippusAmp_ * (0.6f * sinf(t * 1.70f) + 0.4f * sinf(t * 3.31f + 1.3f));
    size_ = clampf(sizeT_.value() + hippus, 0.0f, 1.0f);

    // Flicker: two slow waves plus smoothed noise, like embers
    noise_ += (rng_->signedUnit() - noise_) * 0.15f;
    float flicker = 0.45f * sinf(t * 7.3f) + 0.25f * sinf(t * 17.9f + 2.0f) + 0.30f * noise_;
    glow_ = glowT_.value() * (1.0f + flickerAmt_ * flicker);
    if (glow_ < 0.0f) glow_ = 0.0f;
}
