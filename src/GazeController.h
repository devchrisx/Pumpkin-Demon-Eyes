/*
 * Pumpkin Demon Eyes  -  Gaze controller (eye movement)
 * -----------------------------------------------------
 * Purpose  : Smooth gaze movement of ONE eye in its local frame.
 *            Gaze is a normalised vector, (0,0) = straight ahead, the unit
 *            circle = maximum travel. Main movements are eased tweens
 *            (saccades, slow glides, snaps); small irregular micro-saccades
 *            are layered on top while the eye fixates.
 */

#pragma once

#include "Tween.h"

class Rng;

class GazeController
{
public:
    void  begin(Rng* rng);

    void  moveTo(float x, float y, uint32_t now, uint32_t durationMs, Ease ease);
    void  setMicroAmplitude(float amplitude);     // 0 = frozen stare
    void  update(uint32_t now);

    float x() const { return outX_; }             // main + micro, clamped to unit circle
    float y() const { return outY_; }
    float targetX() const { return mainX_.target(); }
    float targetY() const { return mainY_.target(); }
    bool  moving() const { return mainX_.active() || mainY_.active(); }

private:
    Rng*     rng_       = nullptr;
    Tween    mainX_, mainY_;
    Tween    microX_, microY_;
    float    microAmp_  = 0.0f;
    uint32_t nextMicro_ = 0;
    float    outX_      = 0.0f;
    float    outY_      = 0.0f;
};
