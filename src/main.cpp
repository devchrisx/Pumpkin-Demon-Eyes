/*
 * Pumpkin Demon Eyes  -  Seeed XIAO ESP32-C3 + 2x GC9A01 (240x240)
 * -----------------------------------------------------------------
 * Purpose  : Two glowing, procedurally drawn demon eyes for a 3D-printed
 *            Halloween pumpkin. Runs autonomously - no sensors, no WiFi.
 * Wiring   : shared SPI bus (SCLK GPIO6, MOSI GPIO7), CS GPIO3 / GPIO10,
 *            shared DC GPIO4 and RST GPIO5, backlight GPIO20 (see README.md)
 * Modes    : APP_MODE in Config.h - Animation, HardwareTest, Calibration
 *
 * Serial (USB-CDC, 115200): init report, state changes, FPS statistics
 */

#include <Arduino.h>

#include "App.h"
#include "Calibration.h"
#include "Config.h"
#include "TestMode.h"

// ============================================================
// GLOBAL OBJECTS
// ============================================================

static App             app;
static TestMode        testMode;
static CalibrationMode calibrationMode;
static uint32_t        nextFrameUs = 0;

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t waitStart = millis();
    while (!Serial && (millis() - waitStart) < SERIAL_WAIT_MS) { }    // give the USB host a moment

    Serial.println();
    Serial.println(F("=== Pumpkin Demon Eyes ==="));
    Serial.printf("CPU %lu MHz, heap free %lu bytes\n",
                  (unsigned long)getCpuFrequencyMhz(), (unsigned long)ESP.getFreeHeap());

    bool ok = app.displays.begin();
    app.displays.printReport(Serial);
    if (!ok) Serial.println(F("WARNING: at least one display failed to initialise - continuing anyway"));

    const uint32_t now = millis();
    app.begin(now);
    app.geometry[0].printReport(Serial);
    app.geometry[1].printReport(Serial);

    switch (APP_MODE)
    {
        case AppMode::HardwareTest: testMode.begin(&app, now);        break;
        case AppMode::Calibration:  calibrationMode.begin(&app, now); break;
        case AppMode::Animation:
            Serial.println(F("Mode: ANIMATION"));
            break;
    }
    Serial.printf("Setup done, heap free %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    nextFrameUs = micros();
}

// ============================================================
// LOOP  (non-blocking; one frame per call when it is due)
// ============================================================

void loop()
{
    if (TARGET_FPS > 0)
    {
        const uint32_t nowUs = micros();
        if ((int32_t)(nowUs - nextFrameUs) < 0) return;          // not due yet
        nextFrameUs += 1000000UL / TARGET_FPS;
        if ((int32_t)(nowUs - nextFrameUs) > 0) nextFrameUs = nowUs;   // running late: don't catch up
    }

    const uint32_t now = millis();
    switch (APP_MODE)
    {
        case AppMode::Animation:
            app.animation.update(now);
            app.updateAndRenderEyes(now);
            app.stats.report(now, AnimationController::stateName(app.animation.state()));
            break;

        case AppMode::HardwareTest:
            testMode.update(now);
            break;

        case AppMode::Calibration:
            calibrationMode.update(now);
            break;
    }
}
