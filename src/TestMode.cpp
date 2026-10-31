/*
 * Pumpkin Demon Eyes  -  Hardware test mode
 * -----------------------------------------
 * Purpose  : Implementation, see TestMode.h
 */

#include "TestMode.h"

#include <Arduino.h>

#include "App.h"
#include "Calibration.h"
#include "Config.h"

// ============================================================
// STEPS
// ============================================================

namespace
{
    enum Step : uint8_t { COLOR_LEFT, COLOR_RIGHT, COLOR_BOTH, ORIENTATION, GEOMETRY, ANIMATION, STEP_COUNT };

    constexpr AnimState DEMO[] = {
        AnimState::IDLE, AnimState::LOOK_AROUND, AnimState::BLINK, AnimState::DOUBLE_BLINK,
        AnimState::STARE, AnimState::EVIL_GLARE, AnimState::SLEEP, AnimState::WAKE_UP
    };
    constexpr uint8_t DEMO_COUNT = sizeof(DEMO) / sizeof(DEMO[0]);

    constexpr uint16_t BLACK  = lgfx::color565(0, 0, 0);
    constexpr uint16_t RED    = lgfx::color565(255, 0, 0);
    constexpr uint16_t GREEN  = lgfx::color565(0, 255, 0);
    constexpr uint16_t BLUE   = lgfx::color565(0, 0, 255);
    constexpr uint16_t YELLOW = lgfx::color565(255, 255, 0);
}

// ============================================================
// PUBLIC API
// ============================================================

void TestMode::begin(App* app, uint32_t now)
{
    app_  = app;
    step_ = 0;
    Serial.println(F("[test] HARDWARE TEST MODE (set APP_MODE = AppMode::Animation for normal operation)"));
    app_->displays.printReport(Serial);
    for (uint8_t i = 0; i < 2; ++i) app_->geometry[i].printReport(Serial);
    startStep(now);
}

void TestMode::update(uint32_t now)
{
    if ((int32_t)(now - stepEnd_) >= 0)
    {
        step_ = (uint8_t)((step_ + 1) % STEP_COUNT);
        startStep(now);
        return;
    }

    if (step_ != ANIMATION) return;               // all other steps are static pictures

    if ((int32_t)(now - demoNext_) >= 0 && demo_ < DEMO_COUNT)
    {
        Serial.printf("[test]   demo %u/%u: %s\n", demo_ + 1, DEMO_COUNT,
                      AnimationController::stateName(DEMO[demo_]));
        app_->animation.forceState(DEMO[demo_], now);
        ++demo_;
        demoNext_ = now + TEST_ANIMATION_STEP_MS;
    }
    app_->animation.update(now);
    app_->updateAndRenderEyes(now);
    app_->stats.report(now, AnimationController::stateName(app_->animation.state()));
}

// ============================================================
// INTERNALS
// ============================================================

void TestMode::startStep(uint32_t now)
{
    DisplayManager& dm = app_->displays;
    stepEnd_ = now + TEST_STEP_MS;

    switch (step_)
    {
        case COLOR_LEFT:
            Serial.println(F("[test] 1/6 display 1 RED, display 2 BLACK  -> only the LEFT display may be red"));
            dm.fill(EyeSide::Left, RED);
            dm.fill(EyeSide::Right, BLACK);
            break;

        case COLOR_RIGHT:
            Serial.println(F("[test] 2/6 display 1 BLACK, display 2 GREEN -> only the RIGHT display may be green"));
            dm.fill(EyeSide::Left, BLACK);
            dm.fill(EyeSide::Right, GREEN);
            break;

        case COLOR_BOTH:
            Serial.println(F("[test] 3/6 display 1 BLUE, display 2 YELLOW -> blue/red swapped? set DISPLAY_RGB_ORDER; "
                             "inverted colours? toggle DISPLAY_INVERT"));
            dm.fill(EyeSide::Left, BLUE);
            dm.fill(EyeSide::Right, YELLOW);
            break;

        case ORIENTATION:
            Serial.println(F("[test] 4/6 orientation: the arrow must point to the top of the pumpkin "
                             "-> otherwise change DISPLAY_ROTATION_LEFT / _RIGHT"));
            dm.finish();
            CalibrationDraw::orientation(dm.display(EyeSide::Left), EyeSide::Left);
            CalibrationDraw::orientation(dm.display(EyeSide::Right), EyeSide::Right);
            break;

        case GEOMETRY:
        {
            Serial.println(F("[test] 5/6 eye geometry: centre, radius, rotation, mirroring (see calibration overlay)"));
            stepEnd_ = now + 2 * TEST_STEP_MS;
            for (uint8_t i = 0; i < 2; ++i)
            {
                dm.fill((EyeSide)i, BLACK);
                EyeRenderState st;                    // default: open, looking straight ahead
                st.glow = 0.8f;
                app_->renderState((EyeSide)i, st);
                CalibrationDraw::overlay(dm.display((EyeSide)i), app_->geometry[i], (EyeSide)i, "geometry");
            }
            break;
        }

        case ANIMATION:
            Serial.println(F("[test] 6/6 animation demo (all states), FPS on Serial"));
            stepEnd_ = now + DEMO_COUNT * TEST_ANIMATION_STEP_MS + 500;
            dm.fill(EyeSide::Left, BLACK);
            dm.fill(EyeSide::Right, BLACK);
            app_->animation.setAutonomous(false);
            demo_     = 0;
            demoNext_ = now;
            break;
    }
}
