/*
 * Pumpkin Demon Eyes  -  Eye model
 * --------------------------------
 * Purpose  : Implementation, see Eye.h
 */

#include "Eye.h"

#include "Random.h"

// ============================================================
// PUBLIC API
// ============================================================

void Eye::begin(const EyeGeometryConfig& geometry, Rng* rng, uint32_t now)
{
    signX_ = geometry.mirrorX ? -1.0f : 1.0f;
    signY_ = geometry.mirrorY ? -1.0f : 1.0f;

    gaze_.begin(rng);
    lids_.begin(rng, 0.0f);                      // closed; WAKE_UP opens the eyes after power-on
    pupil_.begin(rng);
    pending_.active = false;
    update(now);
}

void Eye::lookAt(float headX, float headY, uint32_t now, uint32_t durationMs,
                 Ease ease, uint32_t delayMs)
{
    float lx = headX * signX_;
    float ly = headY * signY_;

    if (delayMs == 0)
    {
        pending_.active = false;
        gaze_.moveTo(lx, ly, now, durationMs, ease);
        return;
    }
    pending_.active   = true;
    pending_.startAt  = now + delayMs;
    pending_.x        = lx;
    pending_.y        = ly;
    pending_.duration = durationMs;
    pending_.ease     = ease;
}

void Eye::update(uint32_t now)
{
    if (pending_.active && (int32_t)(now - pending_.startAt) >= 0)
    {
        pending_.active = false;
        gaze_.moveTo(pending_.x, pending_.y, pending_.startAt, pending_.duration, pending_.ease);
    }

    gaze_.update(now);
    lids_.update(now);
    pupil_.update(now);

    state_.gazeX     = gaze_.x();
    state_.gazeY     = gaze_.y();
    state_.openness  = lids_.openness();
    state_.anger     = lids_.anger();
    state_.pupilSize = pupil_.size();
    state_.pupilSlit = pupil_.slit();
    state_.glow      = pupil_.glow() * GLOW_INTENSITY_SCALE;
    state_.coreGlow  = pupil_.coreGlow();

    float rings = (float)(now % 1000000u) * 0.001f * GLOW_FIRE_SPEED;
    state_.firePhase = rings - (float)(int32_t)rings;
}
