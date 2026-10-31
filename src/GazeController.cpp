/*
 * Pumpkin Demon Eyes  -  Gaze controller (eye movement)
 * -----------------------------------------------------
 * Purpose  : Implementation, see GazeController.h
 */

#include "GazeController.h"

#include <math.h>

#include "Config.h"
#include "Random.h"

// ============================================================
// HELPERS
// ============================================================

static void clampToUnitCircle(float& x, float& y)
{
    float d2 = x * x + y * y;
    if (d2 > 1.0f)
    {
        float k = 1.0f / sqrtf(d2);
        x *= k;
        y *= k;
    }
}

// ============================================================
// PUBLIC API
// ============================================================

void GazeController::begin(Rng* rng)
{
    rng_ = rng;
    mainX_.set(0.0f);
    mainY_.set(0.0f);
    microX_.set(0.0f);
    microY_.set(0.0f);
    microAmp_  = MICRO_SACCADE_AMPLITUDE;
    nextMicro_ = 0;
}

void GazeController::moveTo(float x, float y, uint32_t now, uint32_t durationMs, Ease ease)
{
    clampToUnitCircle(x, y);
    mainX_.to(x, now, durationMs, ease);
    mainY_.to(y, now, durationMs, ease);

    // Fixation jitter restarts after the movement has landed
    microX_.to(0.0f, now, durationMs, Ease::OutQuad);
    microY_.to(0.0f, now, durationMs, Ease::OutQuad);
    nextMicro_ = now + durationMs + rng_->rangeU32(MICRO_SACCADE_MS_MIN, MICRO_SACCADE_MS_MAX);
}

void GazeController::setMicroAmplitude(float amplitude)
{
    microAmp_ = amplitude;
}

void GazeController::update(uint32_t now)
{
    float mx = mainX_.update(now);
    float my = mainY_.update(now);

    // Micro-saccades: tiny, quick, irregular jumps around the fixation point
    if (!moving() && (int32_t)(now - nextMicro_) >= 0)
    {
        if (microAmp_ > 0.0f)
        {
            float ang = rng_->range(0.0f, 6.2831853f);
            float rad = microAmp_ * sqrtf(rng_->uniform());
            uint32_t dur = rng_->rangeU32(22, 45);
            microX_.to(cosf(ang) * rad, now, dur, Ease::OutCubic);
            microY_.to(sinf(ang) * rad, now, dur, Ease::OutCubic);
        }
        else
        {
            microX_.to(0.0f, now, 120, Ease::OutQuad);
            microY_.to(0.0f, now, 120, Ease::OutQuad);
        }
        nextMicro_ = now + rng_->rangeU32(MICRO_SACCADE_MS_MIN, MICRO_SACCADE_MS_MAX);
    }

    outX_ = mx + microX_.update(now);
    outY_ = my + microY_.update(now);
    clampToUnitCircle(outX_, outY_);
}
