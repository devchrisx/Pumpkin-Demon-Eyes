/*
 * Pumpkin Demon Eyes  -  Hardware test mode
 * -----------------------------------------
 * Purpose  : Step-by-step diagnosis before the full animation is used:
 *              1  display 1 red, display 2 black   (CS / wiring of display 1)
 *              2  display 1 black, display 2 green (CS / wiring of display 2)
 *              3  display 1 blue, display 2 yellow (independent control, colour order)
 *              4  orientation pattern              (display rotation)
 *              5  eye geometry + overlay           (position, radius, rotation, mirroring)
 *              6  animation demo                   (all states, blinking, pupils)
 *            Every step is announced on Serial; the sequence repeats.
 */

#pragma once

#include <stdint.h>

struct App;

class TestMode
{
public:
    void begin(App* app, uint32_t now);
    void update(uint32_t now);

private:
    void startStep(uint32_t now);

    App*     app_      = nullptr;
    uint8_t  step_     = 0;
    uint8_t  demo_     = 0;      // index into the animation demo sequence
    uint32_t stepEnd_  = 0;
    uint32_t demoNext_ = 0;
};
