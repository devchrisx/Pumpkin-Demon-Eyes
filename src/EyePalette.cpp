/*
 * Pumpkin Demon Eyes  -  Colour palette / gradient tables
 * -------------------------------------------------------
 * Purpose  : Implementation, see EyePalette.h
 */

#include "EyePalette.h"

#include <math.h>

#include "Config.h"
#include "Tween.h"

// ============================================================
// COLOUR HELPERS
// ============================================================

namespace
{
    struct ColorF { float r, g, b; };

    ColorF unpack(uint32_t rgb)
    {
        return { (float)((rgb >> 16) & 0xFF), (float)((rgb >> 8) & 0xFF), (float)(rgb & 0xFF) };
    }

    ColorF mix(const ColorF& a, const ColorF& b, float t)
    {
        return { lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t) };
    }

    inline int16_t clamp255(int32_t v) { return (int16_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

    inline uint16_t pack565(int32_t r, int32_t g, int32_t b)
    {
        r = clamp255(r); g = clamp255(g); b = clamp255(b);
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    inline uint16_t pack565(uint32_t rgb, int32_t kQ8)
    {
        return pack565((int32_t)((rgb >> 16) & 0xFF) * kQ8 >> 8,
                       (int32_t)((rgb >> 8) & 0xFF) * kQ8 >> 8,
                       (int32_t)(rgb & 0xFF) * kQ8 >> 8);
    }

    inline int32_t toQ8(float v) { return (int32_t)(v * 256.0f + 0.5f); }

    // Multi-stop gradient through the iris colours, r = 0 (centre) .. 1 (edge)
    ColorF irisGradient(float r)
    {
        static const float  pos[] = { 0.00f, 0.32f, 0.55f, 0.78f, 0.92f, 1.00f };
        const ColorF        col[] = { unpack(COLOR_IRIS_INNER), unpack(COLOR_IRIS_INNER),
                                      unpack(COLOR_IRIS_MID),   unpack(COLOR_IRIS_OUTER),
                                      unpack(COLOR_IRIS_RIM),   mix(unpack(COLOR_IRIS_RIM), unpack(COLOR_SCLERA_CENTER), 0.5f) };
        for (int i = 1; i < 6; ++i)
        {
            if (r <= pos[i])
            {
                float t = (r - pos[i - 1]) / (pos[i] - pos[i - 1]);
                return mix(col[i - 1], col[i], applyEase(Ease::Smooth, t));
            }
        }
        return col[5];
    }
}

// ============================================================
// STATIC TABLES  (once at start-up)
// ============================================================

void EyePalette::begin()
{
    for (uint8_t i = 0; i < IRIS_STEPS; ++i)
    {
        float  r = sqrtf((i + 0.5f) / IRIS_STEPS);         // table is indexed by r^2
        ColorF c = irisGradient(r);
        irisBase_[i]  = { (int16_t)c.r, (int16_t)c.g, (int16_t)c.b };
        ringPhase_[i] = (uint8_t)((int32_t)(r * GLOW_FIRE_RINGS * 256.0f) & 0xFF);
    }

    const ColorF sc = unpack(COLOR_SCLERA_CENTER), se = unpack(COLOR_SCLERA_EDGE);
    for (uint8_t i = 0; i < SCLERA_STEPS; ++i)
    {
        float  t = (float)i / (SCLERA_STEPS - 1);
        ColorF c = mix(sc, se, powf(t, 0.8f));
        scleraBase_[i] = { (int16_t)c.r, (int16_t)c.g, (int16_t)c.b };
    }

    for (uint8_t i = 0; i < RIM_STEPS; ++i)
    {
        float t = (float)i / RIM_STEPS;
        rimProfile_[i]  = (uint8_t)(255.0f * (1.0f - t) * (1.0f - t));
        haloProfile_[i] = (uint8_t)(255.0f * powf(1.0f - t, 2.2f));
    }
    for (uint8_t i = 0; i < PUPIL_STEPS; ++i)
    {
        float t = (float)i / PUPIL_STEPS;
        coreProfile_[i] = (uint8_t)(255.0f * powf(1.0f - t, 1.5f));
    }
    for (uint8_t g = 0; g < GLOW_LEVELS; ++g)
    {
        float t = (float)g / (GLOW_LEVELS - 1);
        glowProfile_[g] = (uint8_t)(255.0f * t * t);
    }
    for (uint8_t i = 0; i < SHADOW_STEPS; ++i)
    {
        float t = (float)i / (SHADOW_STEPS - 1);
        shadow[i] = (uint8_t)(32.0f * (0.22f + 0.78f * powf(t, 1.3f)) + 0.5f);
    }
    for (int i = 0; i < 256; ++i)
        sine_[i] = (int8_t)(127.0f * sinf(i * (6.2831853f / 256.0f)));

    background = pack565(COLOR_BACKGROUND, 256);
    highlight  = pack565(COLOR_HIGHLIGHT, 256);
    highlight2 = pack565(COLOR_HIGHLIGHT, 220);
}

// ============================================================
// PER-FRAME TABLES  (integer math only)
// ============================================================

void EyePalette::build(const EyeRenderState& st)
{
    const float glow  = clampf(st.glow, 0.0f, 1.6f);
    const float glow1 = glow > 1.0f ? 1.0f : glow;

    // Iris: base gradient * intensity * moving fire rings, plus pupil glow
    const int32_t kIris  = toQ8(0.25f + 0.75f * glow);
    const int32_t fire   = toQ8(GLOW_FIRE_AMOUNT);
    const uint8_t phase  = (uint8_t)((int32_t)(st.firePhase * 256.0f) & 0xFF);
    const int32_t kGlow  = toQ8(GLOW_PUPIL_STRENGTH * glow);
    const int32_t gr = (COLOR_GLOW >> 16) & 0xFF, gg = (COLOR_GLOW >> 8) & 0xFF, gb = COLOR_GLOW & 0xFF;

    int16_t glowR[GLOW_LEVELS], glowG[GLOW_LEVELS], glowB[GLOW_LEVELS];
    for (uint8_t g = 0; g < GLOW_LEVELS; ++g)
    {
        int32_t k = glowProfile_[g] * kGlow >> 8;
        glowR[g] = (int16_t)(gr * k >> 8);
        glowG[g] = (int16_t)(gg * k >> 8);
        glowB[g] = (int16_t)(gb * k >> 8);
    }

    for (uint8_t i = 0; i < IRIS_STEPS; ++i)
    {
        int32_t mod = 256 + (fire * sine_[(uint8_t)(ringPhase_[i] - phase)] >> 7);
        int32_t k   = kIris * mod >> 8;
        int32_t r   = irisBase_[i].r * k >> 8;
        int32_t g   = irisBase_[i].g * k >> 8;
        int32_t b   = irisBase_[i].b * k >> 8;
        for (uint8_t lv = 0; lv < GLOW_LEVELS; ++lv)
            iris[lv][i] = pack565(r + glowR[lv], g + glowG[lv], b + glowB[lv]);
    }

    // Pupil: black, optionally with a glowing core
    const int32_t kCore = toQ8(clampf(st.coreGlow, 0.0f, 1.0f) * (0.5f + 0.5f * glow1));
    const int32_t pr = (COLOR_PUPIL >> 16) & 0xFF, pg = (COLOR_PUPIL >> 8) & 0xFF, pb = COLOR_PUPIL & 0xFF;
    const int32_t cr = (COLOR_PUPIL_CORE >> 16) & 0xFF, cg = (COLOR_PUPIL_CORE >> 8) & 0xFF, cb = COLOR_PUPIL_CORE & 0xFF;
    for (uint8_t i = 0; i < PUPIL_STEPS; ++i)
    {
        int32_t t = coreProfile_[i] * kCore >> 8;
        pupil[i] = pack565(pr + ((cr - pr) * t >> 8), pg + ((cg - pg) * t >> 8), pb + ((cb - pb) * t >> 8));
    }

    // Eyeball
    const int32_t kSclera = toQ8(0.35f + 0.65f * glow1);
    for (uint8_t i = 0; i < SCLERA_STEPS; ++i)
        sclera[i] = pack565(scleraBase_[i].r * kSclera >> 8, scleraBase_[i].g * kSclera >> 8,
                            scleraBase_[i].b * kSclera >> 8);

    // Outer halo: glow around the opening, fades into the background
    const int32_t kHalo = toQ8(clampf(GLOW_HALO_STRENGTH * glow * (0.25f + 0.75f * st.openness), 0.0f, 1.5f));
    const int32_t hr = ((COLOR_HALO >> 16) & 0xFF) * kHalo >> 8;
    const int32_t hg = ((COLOR_HALO >> 8) & 0xFF) * kHalo >> 8;
    const int32_t hb = (COLOR_HALO & 0xFF) * kHalo >> 8;
    const int32_t br = (COLOR_BACKGROUND >> 16) & 0xFF, bg = (COLOR_BACKGROUND >> 8) & 0xFF, bb = COLOR_BACKGROUND & 0xFF;
    int32_t h0r = br, h0g = bg, h0b = bb;
    for (uint8_t i = 0; i < HALO_STEPS; ++i)
    {
        int32_t t = haloProfile_[i];
        int32_t r = br + ((hr - br) * t >> 8), g = bg + ((hg - bg) * t >> 8), b = bb + ((hb - bb) * t >> 8);
        if (i == 0) { h0r = r; h0g = g; h0b = b; }
        halo[i] = pack565(r, g, b);
    }

    // Glowing lid rim: hottest at the lid edge, blends into the halo.
    // Keeps a minimum glow so a closed eye still shows an ember seam.
    const int32_t kRim = toQ8(clampf(GLOW_RIM_STRENGTH * (0.4f + 0.6f * glow) * (0.7f + 0.5f * st.anger),
                                     0.0f, 1.5f));
    const int32_t rr = ((COLOR_LID_RIM >> 16) & 0xFF) * kRim >> 8;
    const int32_t rg = ((COLOR_LID_RIM >> 8) & 0xFF) * kRim >> 8;
    const int32_t rb = (COLOR_LID_RIM & 0xFF) * kRim >> 8;
    for (uint8_t i = 0; i < RIM_STEPS; ++i)
    {
        int32_t t = rimProfile_[i];
        rim[i] = pack565(h0r + ((rr - h0r) * t >> 8), h0g + ((rg - h0g) * t >> 8), h0b + ((rb - h0b) * t >> 8));
    }
}
