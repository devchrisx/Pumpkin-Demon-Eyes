/*
 * Pumpkin Demon Eyes  -  Eye model
 * --------------------------------
 * Purpose  : One animated eye: combines gaze, eyelid/blink and pupil
 *            controllers and produces an EyeRenderState every frame.
 *
 *            Gaze commands are given in the HEAD frame (+x = viewer's right,
 *            +y = down), identical for both eyes. The eye converts them into
 *            its local frame, compensating a configured mirrorX / mirrorY,
 *            so mirrored eyes still look in the same direction. Rotation,
 *            scale and position are NOT applied here - that is purely the
 *            renderer's job.
 */

#pragma once

#include <stdint.h>

#include "BlinkController.h"
#include "Config.h"
#include "EyeState.h"
#include "GazeController.h"
#include "PupilController.h"

class Rng;

class Eye
{
public:
    void begin(const EyeGeometryConfig& geometry, Rng* rng, uint32_t now);
    void update(uint32_t now);

    // Gaze in the head frame; delayMs postpones the movement (eye lag)
    void lookAt(float headX, float headY, uint32_t now, uint32_t durationMs,
                Ease ease, uint32_t delayMs = 0);

    GazeController&       gaze()  { return gaze_; }
    BlinkController&      lids()  { return lids_; }
    PupilController&      pupil() { return pupil_; }
    const EyeRenderState& state() const { return state_; }

private:
    struct PendingGaze
    {
        bool     active = false;
        uint32_t startAt = 0;
        float    x = 0.0f, y = 0.0f;
        uint32_t duration = 0;
        Ease     ease = Ease::Linear;
    };

    GazeController  gaze_;
    BlinkController lids_;
    PupilController pupil_;
    PendingGaze     pending_;
    EyeRenderState  state_;
    float           signX_ = 1.0f;   // head frame -> local frame
    float           signY_ = 1.0f;
};
