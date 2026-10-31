/*
 * Pumpkin Demon Eyes  -  Eye geometry and transformation
 * ------------------------------------------------------
 * Purpose  : Implementation, see EyeGeometry.h
 */

#include "EyeGeometry.h"

#include <Arduino.h>
#include <math.h>

#include "Tween.h"

// ============================================================
// EYE TRANSFORM
// ============================================================

void EyeTransform::configure(const EyeGeometryConfig& cfg)
{
    sx_    = cfg.mirrorX ? -1.0f : 1.0f;
    sy_    = cfg.mirrorY ? -1.0f : 1.0f;
    scale_ = cfg.scale;
    float rad = cfg.rotationDeg * (3.14159265f / 180.0f);
    cos_   = cosf(rad);
    sin_   = sinf(rad);
    cx_    = (float)(cfg.centerX + cfg.offsetX);
    cy_    = (float)(cfg.centerY + cfg.offsetY);

    // Inverse:  p_r = p - c ;  p_s = R(-a) p_r ;  p_m = p_s / scale ;  (u,v) = mirror(p_m)
    float k = 1.0f / scale_;
    ia_ =  sx_ * cos_ * k;
    ib_ =  sx_ * sin_ * k;
    ic_ = -(ia_ * cx_ + ib_ * cy_);
    id_ = -sy_ * sin_ * k;
    ie_ =  sy_ * cos_ * k;
    if_ = -(id_ * cx_ + ie_ * cy_);
}

Vec2 EyeTransform::toDisplay(float u, float v) const
{
    float mx = u * sx_ * scale_;                 // 2. mirror, 3. scale
    float my = v * sy_ * scale_;
    return { cx_ + mx * cos_ - my * sin_,        // 4. rotate (clockwise on screen), 5. translate
             cy_ + mx * sin_ + my * cos_ };
}

Vec2 EyeTransform::toLocal(float x, float y) const
{
    return { ia_ * x + ib_ * y + ic_,
             id_ * x + ie_ * y + if_ };
}

// ============================================================
// EYE SHAPE  (lid curves; the renderer uses the same formulas in fixed point)
// ============================================================

float EyeShape::upperLid(float u, float openness, float anger) const
{
    float q = 1.0f - (u / rx) * (u / rx);
    return closeLine * (1.0f - openness)
         + openness * (-ryTop * q + anger * ryTop * (ANGER_LID_DROP + ANGER_LID_SLANT * u / rx));
}

float EyeShape::lowerLid(float u, float openness, float anger) const
{
    float q = 1.0f - (u / rx) * (u / rx);
    return closeLine * (1.0f - openness)
         + openness * (ryBot * q - anger * ryTop * ANGER_LOWER_SQUINT);
}

// ============================================================
// EYE GEOMETRY
// ============================================================

void EyeGeometry::begin(const EyeGeometryConfig& cfg, const char* name)
{
    cfg_  = cfg;
    name_ = name;
    sanitize();
    xf_.configure(cfg_);

    shape_.rx        = cfg_.radiusX;
    shape_.ryTop     = cfg_.radiusY;
    shape_.ryBot     = cfg_.radiusY * EYE_LOWER_LID_RATIO;
    shape_.closeLine = cfg_.radiusY * EYE_CLOSE_LINE;
    shape_.socketCy  = (shape_.ryBot - shape_.ryTop) * 0.5f;
    shape_.socketRy  = (shape_.ryBot + shape_.ryTop) * 0.5f;
    shape_.haloW     = EYE_HALO_WIDTH;
    shape_.rimW      = EYE_LID_RIM_WIDTH;
    shape_.shadowW   = EYE_LID_SHADOW_WIDTH;
    shape_.irisR     = cfg_.radiusY * IRIS_RADIUS_RATIO;
    shape_.travelX   = cfg_.radiusX * GAZE_RANGE_X;
    shape_.travelY   = cfg_.radiusY * GAZE_RANGE_Y;

    computeSpans();
}

void EyeGeometry::sanitize()
{
    // Keep values in a range the fixed-point renderer is designed for
    EyeGeometryConfig c = cfg_;
    c.scale   = clampf(c.scale,   0.1f, 4.0f);
    c.radiusX = clampf(c.radiusX, 8.0f, 400.0f);
    c.radiusY = clampf(c.radiusY, 8.0f, 400.0f);
    c.radiusY = clampf(c.radiusY, c.radiusX / 3.0f, c.radiusX * 1.5f);   // sensible almond aspect
    if (c.radiusX * c.scale > 600.0f) c.scale = 600.0f / c.radiusX;
    if (c.radiusY * c.scale > 600.0f) c.scale = 600.0f / c.radiusY;
    c.centerX = (int16_t)constrain(c.centerX, -DISPLAY_WIDTH, 2 * DISPLAY_WIDTH);
    c.centerY = (int16_t)constrain(c.centerY, -DISPLAY_HEIGHT, 2 * DISPLAY_HEIGHT);
    c.offsetX = (int16_t)constrain(c.offsetX, -DISPLAY_WIDTH, DISPLAY_WIDTH);
    c.offsetY = (int16_t)constrain(c.offsetY, -DISPLAY_HEIGHT, DISPLAY_HEIGHT);
    sanitized_ = (c.scale != cfg_.scale) || (c.radiusX != cfg_.radiusX) || (c.radiusY != cfg_.radiusY)
              || (c.centerX != cfg_.centerX) || (c.centerY != cfg_.centerY)
              || (c.offsetX != cfg_.offsetX) || (c.offsetY != cfg_.offsetY);
    cfg_ = c;
}

void EyeGeometry::computeSpans()
{
    // Area that can ever be non-background: socket ellipse + rim + halo + 1 px
    const double ax = shape_.rx + shape_.rimW + shape_.haloW + 1.0;
    const double ay = shape_.socketRy + shape_.rimW + shape_.haloW + 1.0;
    const double ax2 = ax * ax, ay2 = ay * ay;
    const double ia = xf_.invA(), ib = xf_.invB(), ic = xf_.invC();
    const double id = xf_.invD(), ie = xf_.invE(), iF = xf_.invF() - shape_.socketCy;

    rowMin_     = DISPLAY_HEIGHT;
    rowMax_     = -1;
    pixelCount_ = 0;
    clipped_    = false;

    for (int16_t y = 0; y < DISPLAY_HEIGHT; ++y)
    {
        spanStart_[y] = 1;
        spanEnd_[y]   = 0;                       // empty

        const double Y  = y + 0.5;               // pixel centre
        const double u0 = ib * Y + ic;
        const double v0 = ie * Y + iF;
        const double A  = ia * ia / ax2 + id * id / ay2;
        const double B  = 2.0 * (ia * u0 / ax2 + id * v0 / ay2);
        const double C  = u0 * u0 / ax2 + v0 * v0 / ay2 - 1.0;
        const double disc = B * B - 4.0 * A * C;
        if (disc < 0.0) continue;

        const double sq = sqrt(disc);
        double x0 = (-B - sq) / (2.0 * A) - 0.5;  // range of pixel indices
        double x1 = (-B + sq) / (2.0 * A) - 0.5;

        if (CLIP_TO_VISIBLE_CIRCLE)
        {
            const double dy = Y - DISPLAY_VISIBLE_CENTER_Y;
            const double r2 = (double)DISPLAY_VISIBLE_RADIUS * DISPLAY_VISIBLE_RADIUS - dy * dy;
            if (r2 <= 0.0) { clipped_ = true; continue; }
            const double h  = sqrt(r2);
            const double c0 = DISPLAY_VISIBLE_CENTER_X - h - 0.5;
            const double c1 = DISPLAY_VISIBLE_CENTER_X + h - 0.5;
            if (x0 < c0) { x0 = c0; clipped_ = true; }
            if (x1 > c1) { x1 = c1; clipped_ = true; }
        }

        int32_t xs = (int32_t)ceil(x0);
        int32_t xe = (int32_t)floor(x1);
        if (xs < 0)                 { xs = 0; clipped_ = true; }
        if (xe > DISPLAY_WIDTH - 1) { xe = DISPLAY_WIDTH - 1; clipped_ = true; }
        if (xs > xe) continue;

        spanStart_[y] = (int16_t)xs;
        spanEnd_[y]   = (int16_t)xe;
        pixelCount_  += (uint32_t)(xe - xs + 1);
        if (y < rowMin_) rowMin_ = y;
        if (y > rowMax_) rowMax_ = y;
    }
    if (rowMax_ < 0) rowMin_ = 0;

    // How far does the eye (socket + halo) poke out of the visible glass?
    overflow_ = 0.0f;
    for (int i = 0; i < 96; ++i)
    {
        float a = i * (6.2831853f / 96.0f);
        Vec2  p = xf_.toDisplay(cosf(a) * (shape_.rx + shape_.rimW + shape_.haloW),
                                shape_.socketCy + sinf(a) * (shape_.socketRy + shape_.rimW + shape_.haloW));
        float dx = p.x - DISPLAY_VISIBLE_CENTER_X, dy = p.y - DISPLAY_VISIBLE_CENTER_Y;
        float over = sqrtf(dx * dx + dy * dy) - DISPLAY_VISIBLE_RADIUS;
        if (over > overflow_) overflow_ = over;
    }
}

void EyeGeometry::bounds(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1) const
{
    x0 = DISPLAY_WIDTH;
    x1 = -1;
    for (int16_t y = rowMin_; y <= rowMax_; ++y)
    {
        if (spanStart_[y] > spanEnd_[y]) continue;
        if (spanStart_[y] < x0) x0 = spanStart_[y];
        if (spanEnd_[y]   > x1) x1 = spanEnd_[y];
    }
    y0 = rowMin_;
    y1 = rowMax_;
}

Vec2 EyeGeometry::clampPupil(float px, float py, float pupilRx, float pupilRy) const
{
    const EyeShape& s = shape_;
    float limX = s.rx * 0.92f - pupilRx;
    if (limX < 0.0f) limX = 0.0f;
    px = clampf(px, -limX, limX);

    float q   = 1.0f - (px / s.rx) * (px / s.rx);
    float top = -s.ryTop * q + pupilRy * 0.55f;
    float bot =  s.ryBot * q - pupilRy * 0.55f;
    py = (top > bot) ? 0.5f * (top + bot) : clampf(py, top, bot);
    return { px, py };
}

void EyeGeometry::printReport(Print& out) const
{
    int16_t x0, y0, x1, y1;
    bounds(x0, y0, x1, y1);
    out.printf("[geo] %s eye: center=(%d,%d) offset=(%d,%d) rot=%.1f deg scale=%.2f "
               "radius=%.0fx%.0f mirrorX=%d mirrorY=%d\n",
               name_, cfg_.centerX, cfg_.centerY, cfg_.offsetX, cfg_.offsetY, cfg_.rotationDeg,
               cfg_.scale, cfg_.radiusX, cfg_.radiusY, cfg_.mirrorX, cfg_.mirrorY);
    out.printf("[geo] %s eye: on-screen half size %.0f x %.0f px, render area x=%d..%d y=%d..%d, "
               "%lu px per frame\n",
               name_, cfg_.radiusX * cfg_.scale, cfg_.radiusY * cfg_.scale, x0, x1, y0, y1,
               (unsigned long)pixelCount_);
    if (sanitized_)
        out.printf("[geo] WARNING %s eye: centre/offset/scale/radius out of range, values were limited\n", name_);
    if (overflow_ > 0.5f)
        out.printf("[geo] WARNING %s eye: eye area exceeds the visible circle by %.0f px "
                   "(edges are cut off)\n", name_, overflow_);
    if (pixelCount_ == 0)
        out.printf("[geo] WARNING %s eye: eye is completely outside the display!\n", name_);
}
