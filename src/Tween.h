/*
 * Pumpkin Demon Eyes  -  Easing and tweening
 * ------------------------------------------
 * Purpose  : Easing curves and a time-based value animator (Tween) used by
 *            all eye controllers. Everything is driven by millis() time
 *            stamps, nothing blocks.
 */

#pragma once

#include <stdint.h>

// ============================================================
// EASING CURVES  (input and output 0..1)
// ============================================================

enum class Ease : uint8_t
{
    Linear,
    Smooth,       // smoothstep
    Smoother,     // smootherstep (softer start/end)
    InQuad,       // accelerate
    OutQuad,      // decelerate
    OutCubic,     // fast start, soft landing (saccades)
    InOutCubic,
    InOutSine,    // slow creeping glances
    OutBack       // overshoot, aggressive snaps
};

float applyEase(Ease ease, float t);

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t)    { return a + (b - a) * t; }

// ============================================================
// TWEEN  (animates one float from its current value to a target)
// ============================================================

class Tween
{
public:
    void  set(float value);                                          // jump immediately
    void  to(float target, uint32_t now, uint32_t durationMs, Ease ease);
    float update(uint32_t now);                                      // advance, returns value

    float value()  const { return value_; }
    float target() const { return to_; }
    bool  active() const { return active_; }

private:
    float    from_     = 0.0f;
    float    to_       = 0.0f;
    float    value_    = 0.0f;
    uint32_t start_    = 0;
    uint32_t duration_ = 0;
    Ease     ease_     = Ease::Linear;
    bool     active_   = false;
};
