/*
 * Pumpkin Demon Eyes  -  Eye renderer
 * -----------------------------------
 * Purpose  : Draws one eye procedurally, pixel by pixel, straight into small
 *            DMA strips - no frame buffer, no images.
 *
 *            For every display pixel inside the eye's precomputed row spans
 *            the INVERSE transformation gives the local eye coordinate
 *            (u, v). Because the mapping is affine, (u, v) is advanced with
 *            two additions per pixel. All shading then happens in the local
 *            frame, so position, scale, rotation and mirroring never touch
 *            the shading code:
 *              halo  -> lid skin -> glowing lid rim -> eyeball -> iris
 *              -> pupil glow -> pupil -> light reflexes -> lid shadow
 *
 *            The ESP32-C3 has no FPU, so the inner loop is pure 32-bit
 *            fixed point (Q4 positions, Q12 normalised distances). Floats are
 *            only used once per frame to derive the coefficients.
 */

#pragma once

#include <stdint.h>

#include "DisplayManager.h"
#include "EyeGeometry.h"
#include "EyePalette.h"
#include "EyeState.h"

class EyeRenderer
{
public:
    void begin(EyeSide side, const EyeGeometry* geometry);
    void render(const EyeRenderState& state, DisplayManager& displays);

    const EyeGeometry& geometry() const    { return *geo_; }
    uint32_t           lastRenderUs() const { return lastUs_; }

private:
    // Per-frame fixed-point coefficients (see EyeRenderer.cpp for the formats)
    struct Frame
    {
        int32_t  socketCy4;                          // centre of the eye area ellipse (Q4)
        uint32_t kEx, kEy;                           // eye area ellipse -> Q12 (4096 = edge)
        uint32_t kSclera;                            // ellipse distance -> eyeball gradient index
        int32_t  uuMax, rx4;                         // |u| < rx (Q4 px^2), rx (Q4)
        int32_t  cornerPos4, cornerNeg4;             // v of the lid seam at u = +rx / -rx (Q4)
        int32_t  up0, up1, up2, lo0, lo2;            // upper / lower lid curves
        int32_t  rimW4, kRim, haloW4, kHalo;         // glowing rim and halo outside the lids
        int32_t  shadowW4, kShadow;                  // lid shadow inside the opening
        int32_t  pu, pv;                             // pupil / iris centre (Q4)
        int32_t  irisBound;                          // circle around the iris (Q8)
        uint32_t kIx, kIy;                           // iris ellipse -> table index
        int32_t  pBoundX, pBoundY;                   // pupil + AA band bounds (Q8)
        uint32_t kPx, kPy;                           // pupil ellipse -> Q12
        int32_t  aaBand, kAA;                        // pupil edge anti-aliasing
        int32_t  gBoundX, gBoundY;                   // glow ellipse bounds (Q8)
        uint32_t kGx, kGy;                           // glow ellipse -> Q12
        int32_t  h1x, h1y, h1R2, kH1;                // main light reflex
        int32_t  h2x, h2y, h2R2, kH2;                // secondary reflex
    };

    void prepare(const EyeRenderState& state);
    void shadeSpan(uint16_t* __restrict dst, int16_t y, int16_t x0, int16_t x1) const;

    EyeSide            side_   = EyeSide::Left;
    const EyeGeometry* geo_    = nullptr;
    EyePalette         pal_;
    Frame              f_ {};
    int32_t            ia_ = 0, ib_ = 0, ic_ = 0;    // u(x,y) in Q16 at pixel centres
    int32_t            id_ = 0, ie_ = 0, if_ = 0;    // v(x,y) in Q16
    uint32_t           lastUs_ = 0;
};
