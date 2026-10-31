/*
 * Pumpkin Demon Eyes  -  Animation controller (state machine)
 * -----------------------------------------------------------
 * Purpose  : Implementation, see AnimationController.h
 */

#include "AnimationController.h"

#include <Arduino.h>
#include <math.h>

#include "Config.h"
#include "Eye.h"
#include "Random.h"

// ============================================================
// CONSTANTS  (behaviour fine-tuning that is not worth a Config entry)
// ============================================================

namespace
{
    constexpr float    IDLE_DRIFT_REACH   = 0.30f;   // how far IDLE drifts from the centre
    constexpr float    IDLE_GLANCE_REACH  = 0.65f;
    constexpr float    STARE_CONVERGENCE  = 0.10f;   // slight cross-eye when staring at the viewer
    constexpr uint32_t STATE_HOLD_MS      = 3600000; // "forever" for non-autonomous mode

    // Canonical glance directions (head frame, unit circle)
    constexpr float DIRS[9][2] = {
        { -1.0f,  0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f },
        { -0.71f, -0.71f }, { 0.71f, -0.71f }, { -0.71f, 0.71f }, { 0.71f, 0.71f },
        {  0.0f,  0.0f }
    };
}

// ============================================================
// PUBLIC API
// ============================================================

void AnimationController::begin(Eye* left, Eye* right, Rng* rng, uint32_t now)
{
    eyes_[0] = left;
    eyes_[1] = right;
    rng_     = rng;
    enterBehaviour(AnimState::WAKE_UP, now);    // the pumpkin "wakes up" after power-on
}

void AnimationController::forceState(AnimState state, uint32_t now)
{
    if (state == AnimState::BLINK || state == AnimState::DOUBLE_BLINK) startTransient(state, now);
    else                                                                 enterBehaviour(state, now);
}

const char* AnimationController::stateName(AnimState state)
{
    switch (state)
    {
        case AnimState::IDLE:         return "IDLE";
        case AnimState::LOOK_AROUND:  return "LOOK_AROUND";
        case AnimState::STARE:        return "STARE";
        case AnimState::EVIL_GLARE:   return "EVIL_GLARE";
        case AnimState::SLEEP:        return "SLEEP";
        case AnimState::WAKE_UP:      return "WAKE_UP";
        case AnimState::BLINK:        return "BLINK";
        case AnimState::DOUBLE_BLINK: return "DOUBLE_BLINK";
    }
    return "?";
}

void AnimationController::update(uint32_t now)
{
    if (state_ == AnimState::BLINK || state_ == AnimState::DOUBLE_BLINK)
    {
        updateTransient(now);
        return;
    }

    // Automatic blinking interrupts the behaviour for a moment
    if (autoBlink_ && (int32_t)(now - nextBlink_) >= 0)
    {
        startTransient(rng_->chance(DOUBLE_BLINK_CHANCE) ? AnimState::DOUBLE_BLINK : AnimState::BLINK, now);
        return;
    }

    // Behaviour finished -> pick the next one
    if ((int32_t)(now - stateEnd_) >= 0)
    {
        if (autonomous_) enterBehaviour(chooseNextBehaviour(), now);
        else             stateEnd_ = now + STATE_HOLD_MS;
        return;
    }

    switch (behaviour_)
    {
        case AnimState::IDLE:        updateIdle(now);       break;
        case AnimState::LOOK_AROUND: updateLookAround(now); break;
        case AnimState::STARE:       updateStare(now);      break;
        case AnimState::EVIL_GLARE:  updateEvilGlare(now);  break;
        case AnimState::SLEEP:       updateSleep(now);      break;
        case AnimState::WAKE_UP:     updateWakeUp(now);     break;
        default: break;
    }
}

// ============================================================
// STATE MACHINE
// ============================================================

void AnimationController::enterBehaviour(AnimState state, uint32_t now)
{
    state_      = state;
    behaviour_  = state;
    phase_      = 0;
    autoBlink_  = true;
    blinkScale_ = 1.0f;
    blinkType_  = BlinkType::Normal;
    microBoth(MICRO_SACCADE_AMPLITUDE);
    unrestBoth(PUPIL_HIPPUS_AMPLITUDE, GLOW_FLICKER_AMOUNT);

    switch (state)
    {
        case AnimState::IDLE:        enterIdle(now);       break;
        case AnimState::LOOK_AROUND: enterLookAround(now); break;
        case AnimState::STARE:       enterStare(now);      break;
        case AnimState::EVIL_GLARE:  enterEvilGlare(now);  break;
        case AnimState::SLEEP:       enterSleep(now);      break;
        case AnimState::WAKE_UP:     enterWakeUp(now);     break;
        default: break;
    }
    if (autoBlink_) scheduleBlink(now);
    logState(now);
}

void AnimationController::startTransient(AnimState state, uint32_t now)
{
    state_ = state;
    if (state == AnimState::DOUBLE_BLINK)
    {
        blinkBoth(BlinkType::Fast, now, 2);
    }
    else
    {
        // Mostly the behaviour's own blink, sometimes a long or fast one
        BlinkType type = blinkType_;
        float r = rng_->uniform();
        if      (r < 0.10f) type = BlinkType::Long;
        else if (r < 0.22f) type = BlinkType::Fast;
        blinkBoth(type, now);
    }
    logState(now);
}

void AnimationController::updateTransient(uint32_t now)
{
    if (anyEyeBlinking()) return;
    state_ = behaviour_;                       // continue the interrupted behaviour
    if (autoBlink_) scheduleBlink(now);
    logState(now);
}

AnimState AnimationController::chooseNextBehaviour()
{
    int idx = rng_->weighted(TRANSITION_WEIGHTS[(uint8_t)behaviour_], BEHAVIOUR_COUNT);
    if (idx < 0) return AnimState::IDLE;       // all weights zero -> safe fallback
    return (AnimState)idx;
}

void AnimationController::logState(uint32_t now) const
{
    if (!LOG_STATE_CHANGES) return;
    if (state_ == behaviour_)
        Serial.printf("[anim] %8lu ms  %-12s (%lu ms)\n", (unsigned long)now, stateName(state_),
                      (unsigned long)(stateEnd_ - now));
    else
        Serial.printf("[anim] %8lu ms  %-12s (in %s)\n", (unsigned long)now, stateName(state_),
                      stateName(behaviour_));
}

// ============================================================
// IDLE  (calm, alive gaze; sometimes eerie total stillness)
// ============================================================

void AnimationController::enterIdle(uint32_t now)
{
    stateEnd_   = now + rng_->rangeU32(IDLE_MS_MIN, IDLE_MS_MAX);
    nextAction_ = now + rng_->rangeU32(400, 1600);
    blinkType_  = BlinkType::Normal;

    opennessBoth(clampOpen(rng_->range(IDLE_OPENNESS_MIN, IDLE_OPENNESS_MAX)), now, 700, Ease::Smooth);
    angerBoth(rng_->range(0.05f, 0.25f), now, 900, Ease::Smooth);
    pupilBoth(PUPIL_SIZE_DEFAULT + rng_->range(-0.1f, 0.1f), PUPIL_SLIT_DEFAULT, now, PUPIL_CHANGE_MS, Ease::InOutSine);
    glowBoth(GLOW_IDLE, now, 800, Ease::Smooth);
    coreGlowBoth(0.0f, now, 700, Ease::Smooth);

    if (rng_->chance(CALM_PHASE_CHANCE))
    {
        uint32_t span = stateEnd_ - now;
        calmStart_ = now + rng_->rangeU32(span / 6, span / 2);
        calmEnd_   = calmStart_ + rng_->rangeU32(CALM_PHASE_MS_MIN, CALM_PHASE_MS_MAX);
        if ((int32_t)(calmEnd_ - stateEnd_) > 0) stateEnd_ = calmEnd_ + 600;
        phase_ = 1;                                          // calm phase pending
    }
}

void AnimationController::updateIdle(uint32_t now)
{
    // Eerie stillness: no jitter, no blinking, no pupil unrest ...
    if (phase_ == 1 && (int32_t)(now - calmStart_) >= 0)
    {
        phase_ = 2;
        microBoth(0.0f);
        unrestBoth(0.0f, GLOW_FLICKER_AMOUNT * 0.3f);
        nextBlink_ = calmEnd_ + rng_->rangeU32(300, 1200);
    }
    if (phase_ == 2)
    {
        if ((int32_t)(now - calmEnd_) < 0) return;
        // ... then a sudden jump that breaks the silence
        phase_ = 3;
        microBoth(MICRO_SACCADE_AMPLITUDE);
        unrestBoth(PUPIL_HIPPUS_AMPLITUDE, GLOW_FLICKER_AMOUNT);
        float x, y;
        randomTarget(0.9f, x, y);
        snapBoth(x, y, now);
        pupilBoth(rng_->range(0.1f, 0.25f), 0.8f, now, 200, Ease::OutCubic);
        nextAction_ = now + rng_->rangeU32(900, 1800);
        return;
    }

    if ((int32_t)(now - nextAction_) < 0 || anyEyeMoving()) return;

    uint32_t dur;
    if (rng_->chance(0.7f))
    {
        // Slow drift around the current fixation point
        float x = clampf(gazeX_ * 0.5f + rng_->signedUnit() * IDLE_DRIFT_REACH, -0.6f, 0.6f);
        float y = clampf(gazeY_ * 0.5f + rng_->signedUnit() * IDLE_DRIFT_REACH * 0.6f, -0.5f, 0.5f);
        dur = glideBoth(x, y, now, false);
    }
    else
    {
        float x, y;
        randomTarget(IDLE_GLANCE_REACH, x, y);
        dur = saccadeBoth(x, y, now, true);
    }

    // Pupils follow the mood slowly
    if (rng_->chance(0.3f))
        pupilBoth(PUPIL_SIZE_DEFAULT + rng_->range(-0.15f, 0.2f), PUPIL_SLIT_DEFAULT + rng_->range(-0.1f, 0.1f),
                  now, PUPIL_CHANGE_MS, Ease::InOutSine);

    nextAction_ = now + dur + rng_->rangeU32(700, 2600);
}

// ============================================================
// LOOK_AROUND  (glances: quick saccades and slow creeping looks)
// ============================================================

void AnimationController::enterLookAround(uint32_t now)
{
    stateEnd_   = now + rng_->rangeU32(LOOK_AROUND_MS_MIN, LOOK_AROUND_MS_MAX);
    nextAction_ = now;
    blinkType_  = BlinkType::Normal;
    blinkScale_ = 0.8f;                                     // moving eyes blink a bit more

    opennessBoth(clampOpen(rng_->range(0.9f, 1.0f)), now, 500, Ease::Smooth);
    angerBoth(rng_->range(0.1f, 0.3f), now, 700, Ease::Smooth);
    pupilBoth(PUPIL_SIZE_DEFAULT + 0.1f, PUPIL_SLIT_DEFAULT, now, PUPIL_CHANGE_MS, Ease::InOutSine);
    glowBoth(GLOW_IDLE + 0.05f, now, 600, Ease::Smooth);
    coreGlowBoth(0.0f, now, 600, Ease::Smooth);
}

void AnimationController::updateLookAround(uint32_t now)
{
    if ((int32_t)(now - nextAction_) < 0 || anyEyeMoving()) return;

    float x, y;
    if (rng_->chance(0.55f))
    {
        const float* d = DIRS[rng_->rangeU32(0, 8)];
        float reach = rng_->range(0.8f, 1.0f);
        x = d[0] * reach;
        y = d[1] * reach;
    }
    else
    {
        randomTarget(1.0f, x, y);                           // any intermediate position
    }

    uint32_t dur;
    float r = rng_->uniform();
    if      (r < 0.55f) dur = saccadeBoth(x, y, now, true);
    else if (r < 0.90f) dur = glideBoth(x, y, now, true);
    else                dur = snapBoth(x, y, now);          // sudden, aggressive

    nextAction_ = now + dur + rng_->rangeU32(LOOK_DWELL_MS_MIN, LOOK_DWELL_MS_MAX);
}

// ============================================================
// STARE  (wide open, constricted slit pupils, frozen gaze)
// ============================================================

void AnimationController::enterStare(uint32_t now)
{
    stateEnd_   = now + rng_->rangeU32(STARE_MS_MIN, STARE_MS_MAX);
    blinkType_  = BlinkType::Slow;
    blinkScale_ = 2.5f;                                     // staring eyes hardly blink

    float x = 0.0f, y = 0.05f;
    if (rng_->chance(0.3f)) randomTarget(0.35f, x, y);
    moveBoth(x, y, now, (uint32_t)((SACCADE_BASE_MS + 40) * ANIM_GAZE_SPEED), Ease::OutCubic,
             false, STARE_CONVERGENCE);

    opennessBoth(OPENNESS_MAX, now, 450, Ease::OutCubic);
    angerBoth(0.35f, now, 900, Ease::Smooth);
    pupilBoth(0.12f, 0.85f, now, 1800, Ease::InOutSine);
    glowBoth(GLOW_STARE, now, 1500, Ease::InOutSine);
    coreGlowBoth(0.25f, now, 1500, Ease::InOutSine);
    microBoth(MICRO_SACCADE_AMPLITUDE * 0.2f);
    unrestBoth(PUPIL_HIPPUS_AMPLITUDE * 0.3f, GLOW_FLICKER_AMOUNT);

    phaseAt_ = now + rng_->rangeU32(1500, 2800);
}

void AnimationController::updateStare(uint32_t now)
{
    if ((int32_t)(now - phaseAt_) < 0) return;

    // A single pupil "throb" in the middle of the stare
    if (phase_ == 0)
    {
        pupilBoth(0.4f, 0.6f, now, 160, Ease::OutBack);
        phase_   = 1;
        phaseAt_ = now + 450;
    }
    else if (phase_ == 1)
    {
        pupilBoth(0.1f, 0.9f, now, 900, Ease::InOutSine);
        phase_ = 2;
    }
}

// ============================================================
// EVIL_GLARE  (sudden snap, slanted lids, blazing glow)
// ============================================================

void AnimationController::enterEvilGlare(uint32_t now)
{
    stateEnd_   = now + rng_->rangeU32(EVIL_GLARE_MS_MIN, EVIL_GLARE_MS_MAX);
    blinkType_  = BlinkType::Fast;
    blinkScale_ = 3.0f;

    snapBoth(rng_->range(-0.15f, 0.15f), rng_->range(0.0f, 0.15f), now);
    angerBoth(1.0f, now, 140, Ease::OutCubic);
    opennessBoth(clampOpen(0.78f), now, 140, Ease::OutCubic);

    if (rng_->chance(0.35f)) pupilBoth(1.0f, 0.25f, now, 110, Ease::OutBack);   // sudden dilation
    else                     pupilBoth(0.05f, 1.0f, now, 120, Ease::OutCubic);  // razor slit

    glowBoth(GLOW_EVIL, now, 120, Ease::OutCubic);
    coreGlowBoth(1.0f, now, 450, Ease::OutQuad);
    microBoth(MICRO_SACCADE_AMPLITUDE * 0.4f);
    unrestBoth(PUPIL_HIPPUS_AMPLITUDE * 0.3f, GLOW_FLICKER_AMOUNT * 2.5f);

    nextAction_ = now + rng_->rangeU32(500, 1200);
}

void AnimationController::updateEvilGlare(uint32_t now)
{
    if ((int32_t)(now - nextAction_) < 0) return;

    float r = rng_->uniform();
    if (r < 0.4f)
    {
        // Aggressive twitch of the gaze
        snapBoth(clampf(gazeX_ + rng_->signedUnit() * 0.12f, -0.5f, 0.5f),
                 clampf(gazeY_ + rng_->signedUnit() * 0.08f, -0.2f, 0.3f), now);
    }
    else if (r < 0.6f)
    {
        // Only one lid twitches - an independent, creepy reaction
        Eye* e = eyes_[rng_->rangeU32(0, 1)];
        e->lids().blink(BlinkType::Fast, now, 0, 1, rng_->range(0.2f, 0.35f));
    }
    else
    {
        // Pulsing glow
        glowBoth(GLOW_EVIL * rng_->range(0.8f, 1.05f), now, rng_->rangeU32(150, 350), Ease::InOutSine);
    }
    nextAction_ = now + rng_->rangeU32(400, 1100);
}

// ============================================================
// SLEEP  (drowsy, fights it once, then closes; the seam keeps glowing)
// ============================================================

void AnimationController::enterSleep(uint32_t now)
{
    autoBlink_ = false;
    stateEnd_  = now + STATE_HOLD_MS;                       // set properly once fully closed

    uint32_t droop = (uint32_t)(SLEEP_CLOSE_MS * 0.45f);
    opennessBoth(0.42f, now, droop, Ease::InOutSine);
    angerBoth(0.0f, now, droop, Ease::Smooth);
    glideBoth(rng_->range(-0.2f, 0.2f), 0.5f, now, false);
    pupilBoth(0.75f, 0.35f, now, SLEEP_CLOSE_MS, Ease::InOutSine);
    glowBoth(GLOW_SLEEP * 2.0f, now, SLEEP_CLOSE_MS, Ease::InOutSine);
    coreGlowBoth(0.0f, now, 800, Ease::Smooth);
    microBoth(MICRO_SACCADE_AMPLITUDE * 0.3f);

    phase_   = 0;
    phaseAt_ = now + droop;
}

void AnimationController::updateSleep(uint32_t now)
{
    if ((int32_t)(now - phaseAt_) < 0) return;

    switch (phase_)
    {
        case 0:   // fights the sleep for a moment
            opennessBoth(0.62f, now, 450, Ease::OutQuad);
            phase_   = 1;
            phaseAt_ = now + 650;
            break;
        case 1:   // gives in, closes completely
        {
            uint32_t close = (uint32_t)(SLEEP_CLOSE_MS * 0.55f);
            opennessBoth(0.0f, now, close, Ease::InOutSine);
            glowBoth(GLOW_SLEEP, now, close, Ease::InOutSine);
            phase_    = 2;
            phaseAt_  = now + close;
            if (autonomous_) stateEnd_ = phaseAt_ + rng_->rangeU32(SLEEP_MS_MIN, SLEEP_MS_MAX);
            break;
        }
        default:  // asleep: the glowing seam "breathes"
        {
            float level = (phase_ & 1) ? GLOW_SLEEP * 0.6f : GLOW_SLEEP * 1.6f;
            glowBoth(level, now, 1700, Ease::InOutSine);
            phase_   = (phase_ == 2) ? 3 : 2;
            phaseAt_ = now + 1700;
            break;
        }
    }
}

// ============================================================
// WAKE_UP  (slow opening, dilated pupils, double blink)
// ============================================================

void AnimationController::enterWakeUp(uint32_t now)
{
    autoBlink_ = false;
    stateEnd_  = now + WAKE_OPEN_MS + 1500;

    uint32_t half = WAKE_OPEN_MS / 2;
    opennessBoth(0.5f, now, half, Ease::InOutSine);
    angerBoth(0.1f, now, WAKE_OPEN_MS, Ease::Smooth);
    glideBoth(0.0f, 0.15f, now, false);
    pupilBoth(0.95f, 0.2f, now, half, Ease::InOutSine);     // dilated, as if out of darkness
    glowBoth(GLOW_IDLE * 0.85f, now, WAKE_OPEN_MS, Ease::InOutSine);
    coreGlowBoth(0.0f, now, 500, Ease::Smooth);

    phase_   = 0;
    phaseAt_ = now + half;
}

void AnimationController::updateWakeUp(uint32_t now)
{
    if ((int32_t)(now - phaseAt_) < 0) return;

    if (phase_ == 0)
    {
        phase_   = 1;
        phaseAt_ = now + 250;
        startTransient(AnimState::DOUBLE_BLINK, now);      // returns to WAKE_UP afterwards
    }
    else if (phase_ == 1)
    {
        opennessBoth(clampOpen(rng_->range(0.88f, 1.0f)), now, WAKE_OPEN_MS / 2, Ease::InOutSine);
        pupilBoth(PUPIL_SIZE_DEFAULT, PUPIL_SLIT_DEFAULT, now, 1400, Ease::InOutSine);
        phase_     = 2;
        phaseAt_   = now + WAKE_OPEN_MS / 2;
        autoBlink_ = true;
        scheduleBlink(now);
    }
    else if (phase_ == 2)
    {
        float x, y;
        randomTarget(0.7f, x, y);
        saccadeBoth(x, y, now, true);                       // first look around
        phase_ = 3;
    }
}

// ============================================================
// COMMANDS FOR BOTH EYES
// ============================================================

uint32_t AnimationController::saccadeBoth(float x, float y, uint32_t now, bool allowIndependent)
{
    float dist = sqrtf((x - gazeX_) * (x - gazeX_) + (y - gazeY_) * (y - gazeY_));
    uint32_t dur = (uint32_t)((SACCADE_BASE_MS + SACCADE_MS_PER_UNIT * dist) * ANIM_GAZE_SPEED);
    moveBoth(x, y, now, dur, Ease::OutCubic, allowIndependent);
    return dur;
}

uint32_t AnimationController::glideBoth(float x, float y, uint32_t now, bool allowIndependent)
{
    uint32_t dur = (uint32_t)(rng_->rangeU32(GLIDE_MS_MIN, GLIDE_MS_MAX) * ANIM_GAZE_SPEED);
    moveBoth(x, y, now, dur, Ease::InOutSine, allowIndependent);
    return dur;
}

uint32_t AnimationController::snapBoth(float x, float y, uint32_t now)
{
    uint32_t dur = (uint32_t)(SNAP_MS * ANIM_GAZE_SPEED);
    moveBoth(x, y, now, dur, Ease::OutBack, false);
    return dur;
}

void AnimationController::moveBoth(float x, float y, uint32_t now, uint32_t durationMs, Ease ease,
                                   bool allowIndependent, float convergence)
{
    gazeX_ = x;
    gazeY_ = y;

    // One eye trails the other a little - or, rarely, clearly reacts on its own
    uint8_t  lagging = (uint8_t)rng_->rangeU32(0, 1);
    uint32_t lag     = rng_->rangeU32(0, EYE_LAG_MS_MAX);
    float    offX    = 0.0f, offY = 0.0f;
    if (allowIndependent && rng_->chance(EYE_INDEPENDENT_CHANCE))
    {
        lag  = (uint32_t)(EYE_INDEPENDENT_LAG_MS * rng_->range(0.6f, 1.0f));
        offX = rng_->signedUnit() * 0.12f;
        offY = rng_->signedUnit() * 0.08f;
    }

    for (uint8_t i = 0; i < 2; ++i)
    {
        // Convergence: the left eye turns right, the right eye turns left
        float cx = (i == 0) ? convergence : -convergence;
        float ex = x + cx + (i == lagging ? offX : 0.0f);
        float ey = y + (i == lagging ? offY : 0.0f);
        eyes_[i]->lookAt(ex, ey, now, durationMs, ease, i == lagging ? lag : 0);
    }
}

void AnimationController::blinkBoth(BlinkType type, uint32_t now, uint8_t repeats)
{
    uint8_t  late   = (uint8_t)rng_->rangeU32(0, 1);
    uint32_t offset = rng_->rangeU32(0, BLINK_EYE_OFFSET_MS);
    for (uint8_t i = 0; i < 2; ++i)
        eyes_[i]->lids().blink(type, now, i == late ? offset : 0, repeats);
}

void AnimationController::opennessBoth(float level, uint32_t now, uint32_t durationMs, Ease ease)
{
    for (Eye* e : eyes_) e->lids().setOpenness(level, now, durationMs, ease);
}

void AnimationController::angerBoth(float level, uint32_t now, uint32_t durationMs, Ease ease)
{
    for (Eye* e : eyes_) e->lids().setAnger(level, now, durationMs, ease);
}

void AnimationController::pupilBoth(float size, float slit, uint32_t now, uint32_t durationMs, Ease ease)
{
    for (Eye* e : eyes_)
    {
        e->pupil().setSize(size + rng_->range(-0.04f, 0.04f), now, durationMs, ease);
        e->pupil().setSlit(slit, now, durationMs, ease);
    }
}

void AnimationController::glowBoth(float glow, uint32_t now, uint32_t durationMs, Ease ease)
{
    for (Eye* e : eyes_) e->pupil().setGlow(glow * rng_->range(0.97f, 1.03f), now, durationMs, ease);
}

void AnimationController::coreGlowBoth(float core, uint32_t now, uint32_t durationMs, Ease ease)
{
    for (Eye* e : eyes_) e->pupil().setCoreGlow(core, now, durationMs, ease);
}

void AnimationController::microBoth(float amplitude)
{
    for (Eye* e : eyes_) e->gaze().setMicroAmplitude(amplitude);
}

void AnimationController::unrestBoth(float hippus, float flicker)
{
    for (Eye* e : eyes_) e->pupil().setUnrest(hippus, flicker);
}

bool AnimationController::anyEyeMoving() const
{
    return eyes_[0]->gaze().moving() || eyes_[1]->gaze().moving();
}

bool AnimationController::anyEyeBlinking() const
{
    return eyes_[0]->lids().blinking() || eyes_[1]->lids().blinking();
}

// ============================================================
// HELPERS
// ============================================================

void AnimationController::scheduleBlink(uint32_t now)
{
    uint32_t interval = rng_->rangeU32(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
    nextBlink_ = now + (uint32_t)(interval * blinkScale_);
}

void AnimationController::randomTarget(float reach, float& x, float& y)
{
    float ang = rng_->range(0.0f, 6.2831853f);
    float rad = reach * sqrtf(rng_->uniform());             // uniform over the disc
    x = cosf(ang) * rad;
    y = sinf(ang) * rad;
}

float AnimationController::clampOpen(float level) const
{
    return clampf(level, OPENNESS_MIN, OPENNESS_MAX);
}
