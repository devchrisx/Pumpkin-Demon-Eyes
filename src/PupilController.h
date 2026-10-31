/*
 * Pumpkin Demon Eyes  -  Pupil controller
 * ---------------------------------------
 * Purpose  : Pupil size and shape plus glow intensity of ONE eye.
 *              size     0 = constricted .. 1 = dilated
 *              slit     0 = round       .. 1 = narrow vertical slit
 *              glow     brightness of the iris / halo / lid rim (1.0 = nominal)
 *              coreGlow 0 = black pupil .. 1 = glowing pupil core
 *            Adds natural pupil unrest (hippus) and fire-like glow flicker.
 */

#pragma once

#include "Tween.h"

class Rng;

class PupilController
{
public:
    void  begin(Rng* rng);

    void  setSize(float size, uint32_t now, uint32_t durationMs, Ease ease);
    void  setSlit(float slit, uint32_t now, uint32_t durationMs, Ease ease);
    void  setGlow(float glow, uint32_t now, uint32_t durationMs, Ease ease);
    void  setCoreGlow(float core, uint32_t now, uint32_t durationMs, Ease ease);
    void  setUnrest(float hippusAmplitude, float flickerAmount);   // 0 = perfectly still
    void  update(uint32_t now);

    float size() const     { return size_; }
    float slit() const     { return slit_.value(); }
    float glow() const     { return glow_; }
    float coreGlow() const { return core_.value(); }

private:
    Rng*  rng_         = nullptr;
    Tween sizeT_, slit_, glowT_, core_;
    float hippusAmp_   = 0.0f;
    float flickerAmt_  = 0.0f;
    float noise_       = 0.0f;     // smoothed random part of the flicker
    float phaseOffset_ = 0.0f;     // de-synchronises the two eyes
    float size_        = 0.0f;
    float glow_        = 0.0f;
};
