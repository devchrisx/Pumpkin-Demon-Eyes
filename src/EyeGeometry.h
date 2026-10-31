/*
 * Pumpkin Demon Eyes  -  Eye geometry and transformation
 * ------------------------------------------------------
 * Purpose  : Everything that depends on WHERE and HOW an eye is placed on
 *            its display, derived once from an EyeGeometryConfig:
 *              - EyeTransform: the one and only local <-> display mapping
 *              - EyeShape    : local shape constants (lids, iris, halo)
 *              - row spans   : which display pixels the eye can ever touch
 *            The animation never sees any of this; it only produces local,
 *            normalised values (EyeRenderState).
 *
 * Transformation order (local -> display), see also Config.h:
 *   mirror -> scale -> rotate (clockwise positive) -> translate -> clip
 */

#pragma once

#include <stdint.h>

#include "Config.h"

class Print;

struct Vec2
{
    float x;
    float y;
};

// ============================================================
// LOCAL <-> DISPLAY TRANSFORMATION
// ============================================================

class EyeTransform
{
public:
    void configure(const EyeGeometryConfig& cfg);

    Vec2  toDisplay(float u, float v) const;     // local -> continuous display coordinates
    Vec2  toLocal(float x, float y) const;       // display -> local

    // Inverse mapping as an affine function of the display coordinate:
    //   u = invA * x + invB * y + invC,   v = invD * x + invE * y + invF
    float invA() const { return ia_; }
    float invB() const { return ib_; }
    float invC() const { return ic_; }
    float invD() const { return id_; }
    float invE() const { return ie_; }
    float invF() const { return if_; }

    float signX() const   { return sx_; }        // -1 if mirrored horizontally
    float signY() const   { return sy_; }
    float scale() const   { return scale_; }
    float centerX() const { return cx_; }        // centre incl. offset
    float centerY() const { return cy_; }

private:
    float sx_ = 1, sy_ = 1, scale_ = 1, cos_ = 1, sin_ = 0, cx_ = 0, cy_ = 0;
    float ia_ = 1, ib_ = 0, ic_ = 0, id_ = 0, ie_ = 1, if_ = 0;
};

// ============================================================
// LOCAL SHAPE  (local units, origin = eye centre)
// ============================================================

struct EyeShape
{
    float rx;          // half-width of the opening (corners at +-rx)
    float ryTop;       // upper lid arc height
    float ryBot;       // lower lid arc height
    float closeLine;   // v where the lids meet when closed
    float socketCy;    // centre offset of the socket ellipse (fitted to the almond)
    float socketRy;    // vertical semi-axis of the socket ellipse
    float haloW;       // halo width
    float rimW;        // lid rim width
    float shadowW;     // lid shadow width
    float irisR;       // iris radius
    float travelX;     // max pupil centre travel
    float travelY;

    // Lid curves for a given state (v coordinate of the lid edge at u)
    float upperLid(float u, float openness, float anger) const;
    float lowerLid(float u, float openness, float anger) const;
};

// ============================================================
// EYE GEOMETRY  (config + transform + shape + row spans)
// ============================================================

class EyeGeometry
{
public:
    void begin(const EyeGeometryConfig& cfg, const char* name);

    const EyeGeometryConfig& config() const    { return cfg_; }
    const EyeTransform&      transform() const { return xf_; }
    const EyeShape&          shape() const     { return shape_; }
    const char*              name() const      { return name_; }

    // Pixel spans the eye may cover: row y uses [spanStart(y), spanEnd(y)],
    // empty if spanStart > spanEnd. Always inside the display.
    int16_t  spanStart(int16_t y) const { return spanStart_[y]; }
    int16_t  spanEnd(int16_t y) const   { return spanEnd_[y]; }
    int16_t  rowMin() const             { return rowMin_; }
    int16_t  rowMax() const             { return rowMax_; }
    uint32_t pixelCount() const         { return pixelCount_; }
    void     bounds(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1) const;

    // Keeps the pupil centre (local units) inside the eye opening
    Vec2     clampPupil(float px, float py, float pupilRx, float pupilRy) const;

    // Largest distance (display px) the eye area pokes out of the visible circle
    float    visibleOverflow() const    { return overflow_; }
    bool     clipped() const            { return clipped_; }

    void     printReport(Print& out) const;

private:
    void     sanitize();
    void     computeSpans();

    EyeGeometryConfig cfg_ {};
    EyeTransform      xf_;
    EyeShape          shape_ {};
    const char*       name_ = "";

    int16_t  spanStart_[DISPLAY_HEIGHT];
    int16_t  spanEnd_[DISPLAY_HEIGHT];
    int16_t  rowMin_     = 0;
    int16_t  rowMax_     = -1;
    uint32_t pixelCount_ = 0;
    float    overflow_   = 0.0f;
    bool     clipped_    = false;
    bool     sanitized_  = false;
};
