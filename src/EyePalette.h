/*
 * Pumpkin Demon Eyes  -  Colour palette / gradient tables
 * -------------------------------------------------------
 * Purpose  : Turns the colours from Config.h into small RGB565 lookup
 *            tables that the per-pixel renderer indexes directly. The
 *            float work (gradient shapes) happens once in begin(); build()
 *            runs every frame with integer math only and bakes the current
 *            glow, flicker, fire rings and pupil core glow into the tables.
 *
 *            All tables hold native RGB565 values (the renderer swaps the
 *            bytes when writing to the DMA buffer).
 */

#pragma once

#include <stdint.h>

#include "EyeState.h"

class EyePalette
{
public:
    static constexpr uint8_t IRIS_STEPS   = 64;   // indexed by normalised r^2 inside the iris
    static constexpr uint8_t GLOW_LEVELS  = 16;   // glow around the pupil, 0 = none
    static constexpr uint8_t PUPIL_STEPS  = 16;   // indexed by normalised r^2 inside the pupil
    static constexpr uint8_t SCLERA_STEPS = 32;   // indexed by socket ellipse distance
    static constexpr uint8_t RIM_STEPS    = 16;   // distance outside the lid edge
    static constexpr uint8_t HALO_STEPS   = 16;   // distance outside the lid rim
    static constexpr uint8_t SHADOW_STEPS = 16;   // distance inside the lid edge

    void begin();
    void build(const EyeRenderState& state);

    uint16_t iris[GLOW_LEVELS][IRIS_STEPS];
    uint16_t pupil[PUPIL_STEPS];
    uint16_t sclera[SCLERA_STEPS];
    uint16_t rim[RIM_STEPS];
    uint16_t halo[HALO_STEPS];
    uint8_t  shadow[SHADOW_STEPS];                // darkening factor 0..32
    uint16_t background = 0;
    uint16_t highlight  = 0;
    uint16_t highlight2 = 0;

private:
    struct Rgb { int16_t r, g, b; };

    Rgb     irisBase_[IRIS_STEPS];
    Rgb     scleraBase_[SCLERA_STEPS];
    uint8_t ringPhase_[IRIS_STEPS];               // fire ring phase per iris step (1/256 turns)
    uint8_t rimProfile_[RIM_STEPS];               // Q8 falloff curves
    uint8_t haloProfile_[HALO_STEPS];
    uint8_t coreProfile_[PUPIL_STEPS];
    uint8_t glowProfile_[GLOW_LEVELS];
    int8_t  sine_[256];
};
