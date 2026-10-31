/*
 * Pumpkin Demon Eyes  -  Application context
 * -------------------------------------------
 * Purpose  : Implementation, see App.h
 */

#include "App.h"

#include <Arduino.h>
#include <esp_system.h>

#include "Config.h"

// ============================================================
// FRAME STATISTICS
// ============================================================

void FrameStats::addFrame(uint32_t frameUs, uint32_t leftUs, uint32_t rightUs)
{
    ++frames_;
    frameSum_ += frameUs;
    leftSum_  += leftUs;
    rightSum_ += rightUs;
    if (frameUs > frameMax_) frameMax_ = frameUs;
}

void FrameStats::report(uint32_t now, const char* stateName)
{
    if (STATS_INTERVAL_MS == 0) return;
    if (last_ == 0) { last_ = now; return; }
    uint32_t span = now - last_;
    if (span < STATS_INTERVAL_MS) return;

    if (frames_ > 0)
    {
        Serial.printf("[stats] %.1f fps | frame avg %.1f ms max %.1f ms | render L %.1f ms R %.1f ms | "
                      "heap free %lu min %lu | %s\n",
                      frames_ * 1000.0f / span,
                      frameSum_ / 1000.0f / frames_, frameMax_ / 1000.0f,
                      leftSum_ / 1000.0f / frames_, rightSum_ / 1000.0f / frames_,
                      (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), stateName);
    }
    frames_ = 0;
    frameSum_ = leftSum_ = rightSum_ = 0;
    frameMax_ = 0;
    last_ = now;
}

// ============================================================
// APPLICATION
// ============================================================

void App::begin(uint32_t now)
{
    rng.seed(RANDOM_SEED ? RANDOM_SEED : esp_random());

    geometry[0].begin(LEFT_EYE_CONFIG, "left");
    geometry[1].begin(RIGHT_EYE_CONFIG, "right");
    for (uint8_t i = 0; i < 2; ++i)
    {
        eyes[i].begin(geometry[i].config(), &rng, now);
        renderers[i].begin((EyeSide)i, &geometry[i]);
    }
    animation.begin(&eyes[0], &eyes[1], &rng, now);
}

void App::updateAndRenderEyes(uint32_t now)
{
    const uint32_t t0 = micros();

    eyes[0].update(now);
    eyes[1].update(now);
    renderers[0].render(eyes[0].state(), displays);
    renderers[1].render(eyes[1].state(), displays);
    displays.finish();

    stats.addFrame(micros() - t0, renderers[0].lastRenderUs(), renderers[1].lastRenderUs());
}

void App::renderState(EyeSide side, const EyeRenderState& state)
{
    renderers[(uint8_t)side].render(state, displays);
    displays.finish();
}
