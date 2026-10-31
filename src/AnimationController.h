/*
 * Pumpkin Demon Eyes  -  Animation controller (state machine)
 * -----------------------------------------------------------
 * Purpose  : Decides WHAT the two eyes do. Behaviour states run for a
 *            random duration and then hand over to the next state chosen
 *            from TRANSITION_WEIGHTS. BLINK and DOUBLE_BLINK are short
 *            interruptions that return to the running behaviour.
 *
 *              IDLE         calm, alive gaze with drifts and eerie stillness
 *              LOOK_AROUND  glances in all directions, quick and creeping
 *              BLINK        single blink (normal / fast / long / slow)
 *              DOUBLE_BLINK two fast blinks
 *              STARE        wide open, constricted slit pupils, frozen gaze
 *              EVIL_GLARE   sudden snap, slanted lids, blazing glow
 *              SLEEP        slow, reluctant closing; glowing seam
 *              WAKE_UP      slow opening, dilated pupils, double blink
 *
 *            Both eyes receive the same commands with small random lags and
 *            variations; sometimes one eye reacts noticeably on its own.
 *            Nothing blocks: update() only compares millis() time stamps.
 */

#pragma once

#include <stdint.h>

#include "BlinkController.h"
#include "Tween.h"

class Eye;
class Rng;

enum class AnimState : uint8_t
{
    IDLE = 0,        // behaviour states: same order as TRANSITION_WEIGHTS
    LOOK_AROUND,
    STARE,
    EVIL_GLARE,
    SLEEP,
    WAKE_UP,
    BLINK,           // transient states
    DOUBLE_BLINK
};

class AnimationController
{
public:
    void        begin(Eye* left, Eye* right, Rng* rng, uint32_t now);
    void        update(uint32_t now);

    void        setAutonomous(bool autonomous) { autonomous_ = autonomous; }
    void        forceState(AnimState state, uint32_t now);
    AnimState   state() const { return state_; }
    static const char* stateName(AnimState state);

private:
    // State machine
    void        enterBehaviour(AnimState state, uint32_t now);
    void        startTransient(AnimState state, uint32_t now);
    void        updateTransient(uint32_t now);
    AnimState   chooseNextBehaviour();
    void        logState(uint32_t now) const;

    // Behaviour states
    void        enterIdle(uint32_t now);
    void        updateIdle(uint32_t now);
    void        enterLookAround(uint32_t now);
    void        updateLookAround(uint32_t now);
    void        enterStare(uint32_t now);
    void        updateStare(uint32_t now);
    void        enterEvilGlare(uint32_t now);
    void        updateEvilGlare(uint32_t now);
    void        enterSleep(uint32_t now);
    void        updateSleep(uint32_t now);
    void        enterWakeUp(uint32_t now);
    void        updateWakeUp(uint32_t now);

    // Commands for both eyes
    uint32_t    saccadeBoth(float x, float y, uint32_t now, bool allowIndependent);
    uint32_t    glideBoth(float x, float y, uint32_t now, bool allowIndependent);
    uint32_t    snapBoth(float x, float y, uint32_t now);
    void        moveBoth(float x, float y, uint32_t now, uint32_t durationMs, Ease ease,
                         bool allowIndependent, float convergence = 0.0f);
    void        blinkBoth(BlinkType type, uint32_t now, uint8_t repeats = 1);
    void        opennessBoth(float level, uint32_t now, uint32_t durationMs, Ease ease);
    void        angerBoth(float level, uint32_t now, uint32_t durationMs, Ease ease);
    void        pupilBoth(float size, float slit, uint32_t now, uint32_t durationMs, Ease ease);
    void        glowBoth(float glow, uint32_t now, uint32_t durationMs, Ease ease);
    void        coreGlowBoth(float core, uint32_t now, uint32_t durationMs, Ease ease);
    void        microBoth(float amplitude);
    void        unrestBoth(float hippus, float flicker);
    bool        anyEyeMoving() const;
    bool        anyEyeBlinking() const;

    void        scheduleBlink(uint32_t now);
    void        randomTarget(float reach, float& x, float& y);
    float       clampOpen(float level) const;

    Eye*        eyes_[2]    = { nullptr, nullptr };
    Rng*        rng_        = nullptr;

    AnimState   state_      = AnimState::IDLE;   // what is shown right now
    AnimState   behaviour_  = AnimState::IDLE;   // behaviour to return to after a blink
    bool        autonomous_ = true;
    bool        autoBlink_  = true;

    uint32_t    stateEnd_   = 0;
    uint32_t    nextAction_ = 0;
    uint32_t    nextBlink_  = 0;
    float       blinkScale_ = 1.0f;              // state dependent blink interval factor
    BlinkType   blinkType_  = BlinkType::Normal; // preferred blink of the behaviour

    uint8_t     phase_      = 0;                 // sub-phase inside a behaviour
    uint32_t    phaseAt_    = 0;                 // time of the next sub-phase step
    uint32_t    calmStart_  = 0;                 // IDLE: eerie stillness window
    uint32_t    calmEnd_    = 0;
    float       gazeX_      = 0.0f;              // last commanded head-frame gaze
    float       gazeY_      = 0.0f;
};
