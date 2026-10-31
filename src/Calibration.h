/*
 * Pumpkin Demon Eyes  -  Calibration mode and test patterns
 * ---------------------------------------------------------
 * Purpose  : Helps to align the eyes after mounting the displays in the
 *            printed pumpkin. Shows a still eye plus an overlay:
 *              grey   : visible circle, display centre and display axes
 *              colour : eye centre, eye axes (rotated with the eye), eye
 *                       outline, pupil travel limit, "+x" arrow (inner
 *                       corner / mirroring), "top" marker, rotation arc
 *              yellow : bounding box of the rendered eye area
 *            Left display = cyan, right display = magenta.
 *            The gaze cycles through all directions so the pupil limits
 *            can be checked. All values are also printed on Serial.
 */

#pragma once

#include <stdint.h>

#include "DisplayManager.h"

struct App;
class  EyeGeometry;

namespace CalibrationDraw
{
    // Alignment overlay on top of an already rendered eye
    void overlay(EyeDisplay& display, const EyeGeometry& geometry, EyeSide side, const char* poseLabel);

    // Orientation pattern for the display rotation test (no eye)
    void orientation(EyeDisplay& display, EyeSide side);
}

class CalibrationMode
{
public:
    void begin(App* app, uint32_t now);
    void update(uint32_t now);

private:
    void showPose(uint32_t now);

    App*     app_    = nullptr;
    uint8_t  pose_   = 0;
    uint32_t nextAt_ = 0;
};
