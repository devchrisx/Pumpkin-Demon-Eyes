/*
 * Pumpkin Demon Eyes  -  Application context
 * -------------------------------------------
 * Purpose  : Bundles all long-lived objects (displays, geometry, eyes,
 *            renderers, animation) so the operating modes (animation,
 *            hardware test, calibration) can share them. Also measures the
 *            frame rate and render times.
 */

#pragma once

#include <stdint.h>

#include "AnimationController.h"
#include "DisplayManager.h"
#include "Eye.h"
#include "EyeGeometry.h"
#include "EyeRenderer.h"
#include "Random.h"

class FrameStats
{
public:
    void addFrame(uint32_t frameUs, uint32_t leftUs, uint32_t rightUs);
    void report(uint32_t now, const char* stateName);     // prints every STATS_INTERVAL_MS

private:
    uint32_t frames_   = 0;
    uint64_t frameSum_ = 0;
    uint64_t leftSum_  = 0;
    uint64_t rightSum_ = 0;
    uint32_t frameMax_ = 0;
    uint32_t last_     = 0;
};

struct App
{
    DisplayManager      displays;
    EyeGeometry         geometry[2];
    Eye                 eyes[2];
    EyeRenderer         renderers[2];
    AnimationController animation;
    Rng                 rng;
    FrameStats          stats;

    void begin(uint32_t now);

    // Advances both eye models and draws them (one complete frame)
    void updateAndRenderEyes(uint32_t now);

    // Draws an explicit state on one eye (used by test and calibration mode)
    void renderState(EyeSide side, const EyeRenderState& state);
};
