/*
 * Pumpkin Demon Eyes  -  Easing and tweening
 * ------------------------------------------
 * Purpose  : Implementation of the easing curves and Tween (see Tween.h)
 */

#include "Tween.h"

#include <math.h>

// ============================================================
// EASING CURVES
// ============================================================

float applyEase(Ease ease, float t)
{
    t = clampf(t, 0.0f, 1.0f);
    switch (ease)
    {
        case Ease::Linear:     return t;
        case Ease::Smooth:     return t * t * (3.0f - 2.0f * t);
        case Ease::Smoother:   return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        case Ease::InQuad:     return t * t;
        case Ease::OutQuad:    return t * (2.0f - t);
        case Ease::OutCubic:   { float u = 1.0f - t; return 1.0f - u * u * u; }
        case Ease::InOutCubic: return t < 0.5f ? 4.0f * t * t * t
                                               : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
        case Ease::InOutSine:  return 0.5f - 0.5f * cosf(t * 3.14159265f);
        case Ease::OutBack:
        {
            const float c1 = 1.70158f;           // standard overshoot (~10 %)
            const float c3 = c1 + 1.0f;
            float u = t - 1.0f;
            return 1.0f + c3 * u * u * u + c1 * u * u;
        }
    }
    return t;
}

// ============================================================
// TWEEN
// ============================================================

void Tween::set(float value)
{
    from_ = to_ = value_ = value;
    active_ = false;
}

void Tween::to(float target, uint32_t now, uint32_t durationMs, Ease ease)
{
    if (durationMs == 0)
    {
        set(target);
        return;
    }
    from_     = value_;                         // continue smoothly from where we are
    to_       = target;
    start_    = now;
    duration_ = durationMs;
    ease_     = ease;
    active_   = true;
}

float Tween::update(uint32_t now)
{
    if (!active_) return value_;

    uint32_t elapsed = now - start_;            // wrap-safe millis() arithmetic
    if (elapsed >= duration_)
    {
        value_  = to_;
        active_ = false;
    }
    else
    {
        float t = (float)elapsed / (float)duration_;
        value_ = lerpf(from_, to_, applyEase(ease_, t));
    }
    return value_;
}
