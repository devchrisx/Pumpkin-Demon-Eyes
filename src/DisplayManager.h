/*
 * Pumpkin Demon Eyes  -  Display manager (2x GC9A01 on one SPI bus)
 * ----------------------------------------------------------------
 * Purpose  : Owns both LovyanGFX display devices, initialises them
 *            (shared or separate DC / RST lines) and pushes pixel strips
 *            via DMA.
 *
 *            Rules for the shared bus (enforced here):
 *              - only one display is selected (in a transaction) at a time;
 *                the Arduino SPI lock is not recursive, so nesting two
 *                transactions would dead-lock
 *              - with a shared DC line both panels use ONE Bus_SPI object,
 *                so the DMA state of the bus is always consistent
 *              - a shared RST line is pulsed once by this class, the panels
 *                themselves get pin_rst = -1 (otherwise initialising the
 *                second panel would reset the first one again)
 */

#pragma once

#include <stdint.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

enum class EyeSide : uint8_t
{
    Left  = 0,     // display 1
    Right = 1      // display 2
};

// One GC9A01 panel as a LovyanGFX device
class EyeDisplay : public lgfx::LGFX_Device
{
public:
    void configure(lgfx::Bus_SPI* bus, int16_t pinCs, int16_t pinRst,
                   uint8_t rotation, lgfx::Light_PWM* light);
    uint8_t configuredRotation() const { return rotation_; }

private:
    lgfx::Panel_GC9A01 panel_;
    uint8_t            rotation_ = 0;
};

class DisplayManager
{
public:
    bool begin();                                       // false if a display failed to initialise

    EyeDisplay& display(EyeSide side) { return displays_[(uint8_t)side]; }
    bool        initialised(EyeSide side) const { return ok_[(uint8_t)side]; }

    // Strip output for the renderer (x, y, w, h in display pixels, data
    // already in panel byte order). The buffer must stay untouched until the
    // next pushStrip()/finish() call returns.
    void pushStrip(EyeSide side, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);
    void finish();                                      // waits for DMA, deselects the display

    void fill(EyeSide side, uint16_t color565);         // full-screen fill (not for animation)
    void setBacklight(uint8_t brightness);

    void printReport(Print& out) const;

private:
    void hardwareReset();
    void restoreIoMux();

    lgfx::Bus_SPI   busLeft_;
    lgfx::Bus_SPI   busRight_;                          // only used with SHARED_DC == false
    lgfx::Light_PWM light_;
    EyeDisplay      displays_[2];
    bool            ok_[2]  = { false, false };
    int8_t          active_ = -1;                       // display currently selected for DMA output
    uint32_t        initMs_ = 0;
};
