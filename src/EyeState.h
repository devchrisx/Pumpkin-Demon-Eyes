/*
 * Pumpkin Demon Eyes  -  Eye render state
 * ---------------------------------------
 * Purpose  : The complete, geometry-free description of what ONE eye looks
 *            like in a given frame. It is the only interface between the
 *            animation (Eye, AnimationController) and the renderer.
 *            All values are in the eye's LOCAL frame and normalised; the
 *            renderer turns them into pixels using the eye geometry.
 */

#pragma once

struct EyeRenderState
{
    float gazeX     = 0.0f;   // local gaze, unit circle = max travel (+x = local right)
    float gazeY     = 0.0f;   // (+y = local down)
    float openness  = 1.0f;   // 0 = closed, 1 = fully open
    float anger     = 0.0f;   // 0 = relaxed lids, 1 = slanted, lowered, squinting lids
    float pupilSize = 0.5f;   // 0 = constricted, 1 = dilated
    float pupilSlit = 0.0f;   // 0 = round, 1 = narrow vertical slit
    float glow      = 1.0f;   // glow intensity (1.0 = nominal colours)
    float coreGlow  = 0.0f;   // 0 = black pupil, 1 = glowing pupil core
    float firePhase = 0.0f;   // phase of the moving rings in the iris (fractional part used)
};
