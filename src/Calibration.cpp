/*
 * Pumpkin Demon Eyes  -  Calibration mode and test patterns
 * ---------------------------------------------------------
 * Purpose  : Implementation, see Calibration.h
 */

#include "Calibration.h"

#include <Arduino.h>
#include <math.h>

#include "App.h"
#include "Config.h"
#include "EyeGeometry.h"

// ============================================================
// POSES  (head-frame gaze, like the animation uses it)
// ============================================================

namespace
{
    struct Pose
    {
        const char* label;
        float       gx, gy, openness, anger, size, slit;
    };

    constexpr Pose POSES[] = {
        { "centre",      0.0f,   0.0f,  1.0f,  0.15f, 0.45f, 0.55f },
        { "left",       -1.0f,   0.0f,  1.0f,  0.15f, 0.45f, 0.55f },
        { "right",       1.0f,   0.0f,  1.0f,  0.15f, 0.45f, 0.55f },
        { "up",          0.0f,  -1.0f,  1.0f,  0.15f, 0.45f, 0.55f },
        { "down",        0.0f,   1.0f,  1.0f,  0.15f, 0.45f, 0.55f },
        { "up-left",    -0.71f, -0.71f, 1.0f,  0.15f, 0.45f, 0.55f },
        { "up-right",    0.71f, -0.71f, 1.0f,  0.15f, 0.45f, 0.55f },
        { "down-left",  -0.71f,  0.71f, 1.0f,  0.15f, 0.45f, 0.55f },
        { "down-right",  0.71f,  0.71f, 1.0f,  0.15f, 0.45f, 0.55f },
        { "half closed", 0.0f,   0.0f,  0.45f, 0.15f, 0.45f, 0.55f },
        { "evil",        0.0f,   0.1f,  0.78f, 1.0f,  0.05f, 1.0f  },
    };
    constexpr uint8_t POSE_COUNT = sizeof(POSES) / sizeof(POSES[0]);

    constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }

    constexpr uint16_t COL_GREY   = rgb(90, 90, 90);
    constexpr uint16_t COL_DARK   = rgb(45, 45, 45);
    constexpr uint16_t COL_WHITE  = rgb(255, 255, 255);
    constexpr uint16_t COL_YELLOW = rgb(255, 230, 0);
    constexpr uint16_t COL_ORANGE = rgb(255, 140, 0);

    uint16_t sideColor(EyeSide side)    { return side == EyeSide::Left ? rgb(0, 230, 255) : rgb(255, 40, 255); }
    uint16_t sideColorDim(EyeSide side) { return side == EyeSide::Left ? rgb(0, 90, 110) : rgb(110, 20, 110); }

    // Line between two LOCAL points, transformed to the display
    void lineLocal(EyeDisplay& d, const EyeTransform& xf, float u0, float v0, float u1, float v1, uint16_t col)
    {
        Vec2 a = xf.toDisplay(u0, v0), b = xf.toDisplay(u1, v1);
        d.drawLine(lroundf(a.x), lroundf(a.y), lroundf(b.x), lroundf(b.y), col);
    }
}

// ============================================================
// OVERLAY DRAWING
// ============================================================

void CalibrationDraw::overlay(EyeDisplay& d, const EyeGeometry& g, EyeSide side, const char* poseLabel)
{
    const EyeTransform&      xf  = g.transform();
    const EyeShape&          s   = g.shape();
    const EyeGeometryConfig& cfg = g.config();
    const uint16_t col = sideColor(side), dim = sideColorDim(side);
    const int16_t  dcx = (int16_t)DISPLAY_VISIBLE_CENTER_X, dcy = (int16_t)DISPLAY_VISIBLE_CENTER_Y;

    d.startWrite();

    // Display references: visible circle, axes and centre (not rotated)
    d.drawCircle(dcx, dcy, (int32_t)DISPLAY_VISIBLE_RADIUS - 1, COL_GREY);
    d.drawLine(0, dcy, DISPLAY_WIDTH - 1, dcy, COL_DARK);
    d.drawLine(dcx, 0, dcx, DISPLAY_HEIGHT - 1, COL_DARK);
    d.drawLine(dcx - 6, dcy, dcx + 6, dcy, COL_WHITE);
    d.drawLine(dcx, dcy - 6, dcx, dcy + 6, COL_WHITE);

    // Bounding box of the rendered eye area
    int16_t bx0, by0, bx1, by1;
    g.bounds(bx0, by0, bx1, by1);
    if (bx1 >= bx0) d.drawRect(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1, COL_YELLOW);

    // Eye axes (rotate with the eye)
    const float ext = s.rx + s.rimW + s.haloW;
    lineLocal(d, xf, -ext, 0.0f, ext, 0.0f, col);
    lineLocal(d, xf, 0.0f, -s.ryTop - s.haloW, 0.0f, s.ryBot + s.haloW, col);

    // "+x" arrow at the local +u end: inner corner of the design / mirroring indicator
    lineLocal(d, xf, ext, 0.0f, ext - 10.0f, -6.0f, col);
    lineLocal(d, xf, ext, 0.0f, ext - 10.0f,  6.0f, col);
    // "top" marker: small triangle above the upper lid
    const float ty = -s.ryTop - s.haloW;
    lineLocal(d, xf, -7.0f, ty + 6.0f, 0.0f, ty - 4.0f, col);
    lineLocal(d, xf,  7.0f, ty + 6.0f, 0.0f, ty - 4.0f, col);

    // Eye outline (fully open lids) and pupil travel limit
    const int N = 28;
    for (int i = 0; i < N; ++i)
    {
        float u0 = -s.rx + 2.0f * s.rx * i / N, u1 = -s.rx + 2.0f * s.rx * (i + 1) / N;
        lineLocal(d, xf, u0, s.upperLid(u0, 1.0f, 0.0f), u1, s.upperLid(u1, 1.0f, 0.0f), col);
        lineLocal(d, xf, u0, s.lowerLid(u0, 1.0f, 0.0f), u1, s.lowerLid(u1, 1.0f, 0.0f), col);
        float a0 = 6.2831853f * i / N, a1 = 6.2831853f * (i + 1) / N;
        lineLocal(d, xf, cosf(a0) * s.travelX, sinf(a0) * s.travelY,
                         cosf(a1) * s.travelX, sinf(a1) * s.travelY, dim);
    }

    // Eye centre
    Vec2 c = xf.toDisplay(0.0f, 0.0f);
    d.drawCircle(lroundf(c.x), lroundf(c.y), 4, col);

    // Rotation arc from the display horizontal to the eye's x axis
    if (fabsf(cfg.rotationDeg) > 0.5f)
    {
        float a0 = 0.0f, a1 = cfg.rotationDeg * (3.14159265f / 180.0f);
        for (int i = 0; i < 12; ++i)
        {
            float p0 = a0 + (a1 - a0) * i / 12.0f, p1 = a0 + (a1 - a0) * (i + 1) / 12.0f;
            d.drawLine(lroundf(c.x + 36.0f * cosf(p0)), lroundf(c.y + 36.0f * sinf(p0)),
                       lroundf(c.x + 36.0f * cosf(p1)), lroundf(c.y + 36.0f * sinf(p1)), COL_ORANGE);
        }
    }

    // Text
    if (CALIBRATION_SHOW_TEXT)
    {
        char line[40];
        d.setTextSize(1);
        d.setTextDatum(lgfx::textdatum_t::middle_center);
        d.setTextColor(col);
        d.drawString(side == EyeSide::Left ? "LEFT / display 1" : "RIGHT / display 2", dcx, 22);
        d.setTextColor(COL_WHITE);
        d.fillRect(dcx - 45, 29, 90, 11, rgb(0, 0, 0));      // clear the previous pose label
        d.drawString(poseLabel, dcx, 34);
        snprintf(line, sizeof(line), "c=%d,%d  off=%d,%d", cfg.centerX, cfg.centerY, cfg.offsetX, cfg.offsetY);
        d.drawString(line, dcx, 196);
        snprintf(line, sizeof(line), "r=%.0fx%.0f  scale=%.2f", cfg.radiusX, cfg.radiusY, cfg.scale);
        d.drawString(line, dcx, 207);
        snprintf(line, sizeof(line), "rot=%.1f  mirror=%d/%d", cfg.rotationDeg, cfg.mirrorX, cfg.mirrorY);
        d.drawString(line, dcx, 218);
    }

    d.endWrite();
}

void CalibrationDraw::orientation(EyeDisplay& d, EyeSide side)
{
    const uint16_t col = sideColor(side);
    const int16_t  cx = DISPLAY_WIDTH / 2, cy = DISPLAY_HEIGHT / 2;

    d.startWrite();
    d.fillScreen(rgb(0, 0, 0));
    d.drawCircle(cx, cy, DISPLAY_WIDTH / 2 - 1, COL_GREY);
    // Arrow pointing to the top of the display coordinate system
    d.fillTriangle(cx, 20, cx - 18, 50, cx + 18, 50, col);
    d.fillRect(cx - 6, 50, 12, 30, col);
    // Marker in the top-left quadrant shows mirroring
    d.fillRect(52, 52, 16, 16, COL_YELLOW);

    d.setTextDatum(lgfx::textdatum_t::middle_center);
    d.setTextColor(col);
    d.setTextSize(4);
    d.drawString(side == EyeSide::Left ? "L1" : "R2", cx, cy + 12);
    d.setTextSize(1);
    d.setTextColor(COL_WHITE);
    char line[32];
    snprintf(line, sizeof(line), "rotation = %u", d.configuredRotation());
    d.drawString("TOP", cx, 90);
    d.drawString(line, cx, cy + 50);
    d.drawString("yellow square = top-left", cx, cy + 62);
    d.endWrite();
}

// ============================================================
// CALIBRATION MODE
// ============================================================

void CalibrationMode::begin(App* app, uint32_t now)
{
    app_    = app;
    pose_   = 0;
    nextAt_ = now;

    Serial.println(F("[cal] CALIBRATION MODE - adjust LEFT_EYE_CONFIG / RIGHT_EYE_CONFIG in Config.h"));
    Serial.println(F("[cal] grey = display, colour = eye, yellow = rendered area, arrow = local +x (inner corner)"));
    for (uint8_t i = 0; i < 2; ++i) app_->geometry[i].printReport(Serial);
}

void CalibrationMode::update(uint32_t now)
{
    if ((int32_t)(now - nextAt_) < 0) return;    // static picture between pose changes
    showPose(now);
    pose_   = (uint8_t)((pose_ + 1) % POSE_COUNT);
    nextAt_ = now + CALIBRATION_POSE_MS;
}

void CalibrationMode::showPose(uint32_t now)
{
    const Pose& p = POSES[pose_];
    Serial.printf("[cal] pose: %s\n", p.label);

    for (uint8_t i = 0; i < 2; ++i)
    {
        const EyeGeometry& g = app_->geometry[i];
        EyeRenderState st;
        st.gazeX     = p.gx * g.transform().signX();       // head frame -> local, as Eye::lookAt does
        st.gazeY     = p.gy * g.transform().signY();
        st.openness  = p.openness;
        st.anger     = p.anger;
        st.pupilSize = p.size;
        st.pupilSlit = p.slit;
        st.glow      = 0.8f;
        st.coreGlow  = p.anger > 0.5f ? 1.0f : 0.0f;
        st.firePhase = 0.0f;

        app_->renderState((EyeSide)i, st);
        CalibrationDraw::overlay(app_->displays.display((EyeSide)i), g, (EyeSide)i, p.label);
    }
    (void)now;
}
