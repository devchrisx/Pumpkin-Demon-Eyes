/*
 * Pumpkin Demon Eyes  -  Eye renderer
 * -----------------------------------
 * Purpose  : Implementation, see EyeRenderer.h
 *
 * Fixed-point formats used in the pixel loop
 *   Q16  u, v while stepping along a row (local units * 65536)
 *   Q4   u4, v4 and all lid / centre positions (local units * 16)
 *   uu0  u4^2 >> 4 = local units^2 * 16
 *   du2  (u4 - pu)^2 = local units^2 * 256       (iris, pupil, reflexes)
 *   Q12  normalised ellipse distances, 4096 = on the ellipse
 *   Every "k" factor is chosen so that k * value stays below 2^32 for all
 *   pixels inside the precomputed spans (see EyeGeometry::sanitize()).
 */

#include "EyeRenderer.h"

#include <Arduino.h>
#include <math.h>

#include "Config.h"
#include "Tween.h"

// ============================================================
// STRIP BUFFERS AND PIXEL HELPERS
// ============================================================

namespace
{
    // Two strips: one is rendered while the other is on its way via DMA.
    // Static internal RAM is DMA capable on the ESP32-C3.
    alignas(4) uint16_t s_strip[2][DISPLAY_WIDTH * RENDER_STRIP_ROWS];
    uint8_t             s_current = 0;

    inline int32_t q16(float v) { return (int32_t)lroundf(v * 65536.0f); }

    // RGB565 spread into 0x07E0F81F so that the three channels can be
    // multiplied in one 32-bit operation without overlapping
    inline uint32_t spread(uint16_t c)  { return (c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }
    inline uint16_t unspread(uint32_t s) { return (uint16_t)(s | (s >> 16)); }

    // alpha 0..32: 0 = bg, 32 = fg
    inline uint16_t blend565(uint16_t bg, uint16_t fg, uint32_t alpha)
    {
        uint32_t b = spread(bg), f = spread(fg);
        return unspread((b + (((f - b) * alpha) >> 5)) & 0x07E0F81Fu);
    }

    // k 0..32: 32 = unchanged
    inline uint16_t darken565(uint16_t c, uint32_t k)
    {
        return unspread(((spread(c) * k) >> 5) & 0x07E0F81Fu);
    }

    inline uint16_t swap16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

    inline void fill16(uint16_t* dst, int32_t count, uint16_t value)
    {
        while (count-- > 0) *dst++ = value;
    }
}

// ============================================================
// SETUP
// ============================================================

void EyeRenderer::begin(EyeSide side, const EyeGeometry* geometry)
{
    side_ = side;
    geo_  = geometry;
    pal_.begin();

    // Inverse affine mapping, evaluated at pixel centres (x + 0.5, y + 0.5)
    const EyeTransform& xf = geo_->transform();
    ia_ = q16(xf.invA());
    ib_ = q16(xf.invB());
    ic_ = q16(0.5f * xf.invA() + 0.5f * xf.invB() + xf.invC());
    id_ = q16(xf.invD());
    ie_ = q16(xf.invE());
    if_ = q16(0.5f * xf.invD() + 0.5f * xf.invE() + xf.invF());
}

// ============================================================
// PER-FRAME COEFFICIENTS  (floats, once per frame)
// ============================================================

void EyeRenderer::prepare(const EyeRenderState& st)
{
    const EyeShape& s  = geo_->shape();
    const float     o  = clampf(st.openness, 0.0f, 1.0f);
    const float     an = clampf(st.anger, 0.0f, 1.0f);
    Frame&          f  = f_;

    // Eye area ellipse = socket ellipse (fitted to the almond) grown by rim +
    // halo + 1 px; identical to the area used for the row spans
    const float ax = s.rx + s.rimW + s.haloW + 1.0f;
    const float ay = s.socketRy + s.rimW + s.haloW + 1.0f;
    f.socketCy4 = (int32_t)(s.socketCy * 16.0f);
    f.kEx       = (uint32_t)(16777216.0f / (ax * ax));
    f.kEy       = (uint32_t)(16777216.0f / (ay * ay));
    f.kSclera   = (uint32_t)(EyePalette::SCLERA_STEPS * 65536.0f / (4096.0f * (s.rx / ax) * (s.rx / ax)));
    f.uuMax     = (int32_t)(16.0f * s.rx * s.rx);
    f.rx4       = (int32_t)(16.0f * s.rx);

    // Lid curves  v(u) = c0 + c1 * u + c2 * u^2  (same as EyeShape::upperLid/lowerLid)
    const float closed = s.closeLine * (1.0f - o);
    f.up0 = (int32_t)(16.0f * (closed + o * (-s.ryTop + an * s.ryTop * ANGER_LID_DROP)));
    f.up1 = q16(o * an * s.ryTop * ANGER_LID_SLANT / s.rx);
    f.up2 = q16(o * s.ryTop / (s.rx * s.rx));
    f.lo0 = (int32_t)(16.0f * (closed + o * (s.ryBot - an * s.ryTop * ANGER_LOWER_SQUINT)));
    f.lo2 = q16(-o * s.ryBot / (s.rx * s.rx));

    // Lid seam at the two corners (anger moves the inner corner down)
    f.cornerPos4 = (int32_t)(8.0f * (s.upperLid( s.rx, o, an) + s.lowerLid( s.rx, o, an)));
    f.cornerNeg4 = (int32_t)(8.0f * (s.upperLid(-s.rx, o, an) + s.lowerLid(-s.rx, o, an)));

    f.rimW4    = (int32_t)(s.rimW * 16.0f);
    f.kRim     = (int32_t)(16.0f * 65536.0f / (float)f.rimW4);
    f.haloW4   = (int32_t)(s.haloW * 16.0f);
    f.kHalo    = (int32_t)(16.0f * 65536.0f / (float)f.haloW4);
    f.shadowW4 = (int32_t)(s.shadowW * 16.0f);
    f.kShadow  = (int32_t)(16.0f * 65536.0f / (float)f.shadowW4);

    // Iris (slightly squashed when looking sideways)
    const float gx  = st.gazeX, gy = st.gazeY;
    const float fx  = 1.0f - IRIS_FORESHORTEN * gx * gx;
    const float fy  = 1.0f - IRIS_FORESHORTEN * gy * gy;
    const float ri  = s.irisR;
    const float rix = ri * fx, riy = ri * fy;

    // Pupil: round .. vertical slit
    const float size = clampf(st.pupilSize, 0.0f, 1.0f);
    const float slit = clampf(st.pupilSlit, 0.0f, 1.0f);
    const float rp   = ri * lerpf(PUPIL_RADIUS_MIN, PUPIL_RADIUS_MAX, size);
    float rpx = rp * lerpf(1.0f, PUPIL_SLIT_WIDTH, slit) * fx;
    float rpy = rp * lerpf(1.0f, PUPIL_SLIT_HEIGHT, slit);
    if (rpy > ri * 0.92f) rpy = ri * 0.92f;
    rpy *= fy;
    if (rpx < 1.2f) rpx = 1.2f;
    if (rpy < 1.2f) rpy = 1.2f;

    // Pupil centre: local gaze -> local pixels, kept inside the eye opening
    const Vec2 pc = geo_->clampPupil(gx * s.travelX, gy * s.travelY, rpx, rpy);
    f.pu = (int32_t)(pc.x * 16.0f);
    f.pv = (int32_t)(pc.y * 16.0f);

    const float rmax = rix > riy ? rix : riy;
    f.irisBound = (int32_t)(256.0f * rmax * rmax);
    f.kIx       = (uint32_t)(4194304.0f / (rix * rix));
    f.kIy       = (uint32_t)(4194304.0f / (riy * riy));

    const float rmin = rpx < rpy ? rpx : rpy;
    f.aaBand  = (int32_t)clampf(8192.0f / rmin, 64.0f, 4096.0f);   // ~1 px soft pupil edge
    f.kAA     = 32 * 65536 / f.aaBand;
    const float b2 = 1.0f + f.aaBand / 4096.0f;
    f.pBoundX = (int32_t)(256.0f * rpx * rpx * b2);
    f.pBoundY = (int32_t)(256.0f * rpy * rpy * b2);
    f.kPx     = (uint32_t)(1048576.0f / (rpx * rpx));
    f.kPy     = (uint32_t)(1048576.0f / (rpy * rpy));

    // Glow around the pupil follows the pupil shape plus a constant margin
    const float gm  = PUPIL_GLOW_MARGIN * ri;
    const float gwx = rpx + gm, gwy = rpy + gm;
    f.gBoundX = (int32_t)(256.0f * gwx * gwx);
    f.gBoundY = (int32_t)(256.0f * gwy * gwy);
    f.kGx     = (uint32_t)(1048576.0f / (gwx * gwx));
    f.kGy     = (uint32_t)(1048576.0f / (gwy * gwy));

    // Light reflexes: positioned screen-consistently (mirroring compensated)
    const EyeTransform& xf = geo_->transform();
    const float h1r = HIGHLIGHT_RADIUS * ri, h2r = HIGHLIGHT2_RADIUS * ri;
    f.h1x  = (int32_t)(HIGHLIGHT_X * ri * xf.signX() * 16.0f);
    f.h1y  = (int32_t)(HIGHLIGHT_Y * ri * xf.signY() * 16.0f);
    f.h1R2 = (int32_t)(256.0f * h1r * h1r);
    f.kH1  = f.h1R2 > 0 ? (int32_t)(HIGHLIGHT_ALPHA * 65536.0f / f.h1R2) : 0;
    f.h2x  = (int32_t)(HIGHLIGHT2_X * ri * xf.signX() * 16.0f);
    f.h2y  = (int32_t)(HIGHLIGHT2_Y * ri * xf.signY() * 16.0f);
    f.h2R2 = (int32_t)(256.0f * h2r * h2r);
    f.kH2  = f.h2R2 > 0 ? (int32_t)(HIGHLIGHT2_ALPHA * 65536.0f / f.h2R2) : 0;
}

// ============================================================
// PIXEL LOOP  (fixed point only)
// ============================================================

void IRAM_ATTR EyeRenderer::shadeSpan(uint16_t* __restrict dst, int16_t y, int16_t x0, int16_t x1) const
{
    const Frame&      f  = f_;
    const EyePalette& p  = pal_;
    const uint16_t    bg = swap16(p.background);

    // Hot values in locals: lets the compiler keep them in registers
    const int32_t  stepU = ia_, stepV = id_;
    const int32_t  cy4 = f.socketCy4, uuMax = f.uuMax;
    const uint32_t kEx = f.kEx, kEy = f.kEy;
    const int32_t  up0 = f.up0, up1 = f.up1, up2 = f.up2, lo0 = f.lo0, lo2 = f.lo2;

    int32_t u = ia_ * x0 + ib_ * y + ic_;
    int32_t v = id_ * x0 + ie_ * y + if_;

    for (int16_t x = x0; x <= x1; ++x, u += stepU, v += stepV)
    {
        const int32_t  u4  = u >> 12;
        const int32_t  v4  = v >> 12;
        const int32_t  uu0 = (u4 * u4) >> 4;
        const int32_t  vc  = v4 - cy4;
        const int32_t  vv0 = (vc * vc) >> 4;
        const uint32_t e   = ((uint32_t)uu0 * kEx + (uint32_t)vv0 * kEy) >> 16;        // Q12

        if (e >= 4096) { *dst++ = bg; continue; }                                    // outside the eye area

        const int32_t vtop = up0 + ((up1 * u4) >> 16) + ((up2 * uu0) >> 16);
        const int32_t vbot = lo0 + ((lo2 * uu0) >> 16);
        const bool    inW  = uu0 < uuMax;
        uint16_t      c;

        if (inW && v4 > vtop && v4 < vbot)
        {
            // ---- inside the lid opening: eyeball, iris, pupil, reflexes
            const uint32_t eIdx = (e * f.kSclera) >> 16;
            c = p.sclera[eIdx < EyePalette::SCLERA_STEPS ? eIdx : EyePalette::SCLERA_STEPS - 1];

            const int32_t du  = u4 - f.pu;
            const int32_t dv  = v4 - f.pv;
            const int32_t du2 = du * du;
            const int32_t dv2 = dv * dv;

            if (du2 + dv2 < f.irisBound)
            {
                const uint32_t ri = ((uint32_t)du2 * f.kIx + (uint32_t)dv2 * f.kIy) >> 24;
                if (ri < EyePalette::IRIS_STEPS)
                {
                    uint32_t level = 0;
                    if (du2 < f.gBoundX && dv2 < f.gBoundY)
                    {
                        const uint32_t pg = ((uint32_t)du2 * f.kGx + (uint32_t)dv2 * f.kGy) >> 16;
                        if (pg < 4096) level = 15 - ((pg * 15) >> 12);
                    }
                    c = p.iris[level][ri];

                    if (du2 < f.pBoundX && dv2 < f.pBoundY)
                    {
                        const uint32_t pp = ((uint32_t)du2 * f.kPx + (uint32_t)dv2 * f.kPy) >> 16;
                        if (pp < 4096)
                            c = p.pupil[pp >> 8];
                        else if (pp < 4096u + (uint32_t)f.aaBand)
                            c = blend565(p.pupil[EyePalette::PUPIL_STEPS - 1], c,
                                         ((pp - 4096) * (uint32_t)f.kAA) >> 16);
                    }

                    const int32_t h1x = du - f.h1x, h1y = dv - f.h1y;
                    const int32_t h1  = h1x * h1x + h1y * h1y;
                    if (h1 < f.h1R2) c = blend565(c, p.highlight, ((f.h1R2 - h1) * f.kH1) >> 16);

                    const int32_t h2x = du - f.h2x, h2y = dv - f.h2y;
                    const int32_t h2  = h2x * h2x + h2y * h2y;
                    if (h2 < f.h2R2) c = blend565(c, p.highlight2, ((f.h2R2 - h2) * f.kH2) >> 16);
                }
            }

            // Lids cast a shadow onto the eyeball
            const int32_t sT = v4 - vtop;
            const int32_t sB = (vbot - v4) * 2;               // lower lid: half the width
            if (sT < f.shadowW4 || sB < f.shadowW4)
            {
                uint32_t k = 32;
                if (sT < f.shadowW4) k = p.shadow[(sT * f.kShadow) >> 16];
                if (sB < f.shadowW4)
                {
                    uint32_t k2 = p.shadow[(sB * f.kShadow) >> 16];
                    if (k2 < k) k = k2;
                }
                c = darken565(c, k);
            }
        }
        else
        {
            // ---- outside the opening: glowing lid rim, then the halo.
            // dd = distance to the nearest lid edge (vertical in the local frame)
            int32_t dd;
            if (inW)
            {
                if (vtop >= vbot)                                // lids overlap: one seam line
                {
                    const int32_t seam = (vtop + vbot) >> 1;
                    dd = v4 > seam ? v4 - seam : seam - v4;
                }
                else
                {
                    dd = (v4 <= vtop) ? vtop - v4 : v4 - vbot;
                }
            }
            else
            {
                // Beyond the eye corners: diamond distance to the corner point,
                // so the rim ends in a short, pointed tip
                const int32_t adu = (u4 < 0 ? -u4 : u4) - f.rx4;
                const int32_t dvc = v4 - (u4 > 0 ? f.cornerPos4 : f.cornerNeg4);
                dd = adu + (dvc < 0 ? -dvc : dvc);
            }

            if (dd < f.rimW4)
                c = p.rim[(dd * f.kRim) >> 16];
            else if (dd - f.rimW4 < f.haloW4)
                c = p.halo[((dd - f.rimW4) * f.kHalo) >> 16];
            else
                c = p.background;
        }

        *dst++ = swap16(c);
    }
}

// ============================================================
// FRAME OUTPUT  (strips, double buffered DMA)
// ============================================================

void EyeRenderer::render(const EyeRenderState& state, DisplayManager& displays)
{
    const uint32_t t0 = micros();

    prepare(state);
    pal_.build(state);

    const EyeGeometry& g  = *geo_;
    const uint16_t     bg = swap16(pal_.background);

    for (int16_t y0 = g.rowMin(); y0 <= g.rowMax(); y0 += RENDER_STRIP_ROWS)
    {
        int16_t y1 = y0 + RENDER_STRIP_ROWS - 1;
        if (y1 > g.rowMax()) y1 = g.rowMax();

        // Window of this strip = union of the row spans
        int16_t xs = DISPLAY_WIDTH, xe = -1;
        for (int16_t y = y0; y <= y1; ++y)
        {
            if (g.spanStart(y) > g.spanEnd(y)) continue;
            if (g.spanStart(y) < xs) xs = g.spanStart(y);
            if (g.spanEnd(y)   > xe) xe = g.spanEnd(y);
        }
        if (xe < xs) continue;

        const int16_t w   = xe - xs + 1;
        uint16_t*     buf = s_strip[s_current];

        for (int16_t y = y0; y <= y1; ++y)
        {
            uint16_t*     row = buf + (y - y0) * w;
            const int16_t sx  = g.spanStart(y);
            const int16_t ex  = g.spanEnd(y);
            if (sx > ex)
            {
                fill16(row, w, bg);
                continue;
            }
            fill16(row, sx - xs, bg);
            shadeSpan(row + (sx - xs), y, sx, ex);
            fill16(row + (ex - xs + 1), xe - ex, bg);
        }

        displays.pushStrip(side_, xs, y0, w, y1 - y0 + 1, buf);
        s_current ^= 1;
    }

    lastUs_ = micros() - t0;
}
