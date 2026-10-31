/*
 * Pumpkin Demon Eyes  -  Display manager (2x GC9A01 on one SPI bus)
 * ----------------------------------------------------------------
 * Purpose  : Implementation, see DisplayManager.h
 */

#include "DisplayManager.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <soc/io_mux_reg.h>

#include "Config.h"

namespace
{
    constexpr uint8_t GC9A01_CMD_RAMWR = 0x2C;   // harmless command used to close a DMA burst
}

// ============================================================
// EYE DISPLAY  (one panel)
// ============================================================

void EyeDisplay::configure(lgfx::Bus_SPI* bus, int16_t pinCs, int16_t pinRst,
                           uint8_t rotation, lgfx::Light_PWM* light)
{
    auto cfg = panel_.config();
    cfg.pin_cs          = pinCs;
    cfg.pin_rst         = pinRst;                // -1 when the reset line is shared
    cfg.pin_busy        = -1;
    cfg.panel_width     = DISPLAY_WIDTH;
    cfg.panel_height    = DISPLAY_HEIGHT;
    cfg.memory_width    = DISPLAY_WIDTH;
    cfg.memory_height   = DISPLAY_HEIGHT;
    cfg.offset_x        = 0;
    cfg.offset_y        = 0;
    cfg.offset_rotation = 0;
    cfg.readable        = false;                 // MISO is not connected
    cfg.invert          = DISPLAY_INVERT;
    cfg.rgb_order       = DISPLAY_RGB_ORDER;
    cfg.dlen_16bit      = false;
    cfg.bus_shared      = false;                 // no SD card on this bus
    panel_.config(cfg);
    panel_.setBus(bus);
    if (light) panel_.setLight(light);
    setPanel(&panel_);
    rotation_ = rotation;
}

// ============================================================
// INITIALISATION
// ============================================================

bool DisplayManager::begin()
{
    uint32_t t0 = millis();

    // Bus (one object if DC is shared, see header)
    auto bc = busLeft_.config();
    bc.spi_host    = SPI2_HOST;
    bc.spi_mode    = 0;
    bc.freq_write  = SPI_WRITE_FREQ_HZ;
    bc.freq_read   = 16000000;
    bc.spi_3wire   = true;
    bc.use_lock    = true;
    bc.dma_channel = SPI_DMA_CH_AUTO;
    bc.pin_sclk    = PIN_SPI_SCLK;
    bc.pin_mosi    = PIN_SPI_MOSI;
    bc.pin_miso    = PIN_SPI_MISO;
    bc.pin_dc      = PIN_TFT_DC;
    busLeft_.config(bc);
    if (!SHARED_DC)
    {
        bc.pin_dc = PIN_TFT_DC_RIGHT;
        busRight_.config(bc);
    }

    // Backlight (both BL inputs on one pin, driven through the left panel)
    lgfx::Light_PWM* light = nullptr;
    if (PIN_BACKLIGHT >= 0)
    {
        auto lc = light_.config();
        lc.pin_bl      = PIN_BACKLIGHT;
        lc.invert      = false;
        lc.freq        = BACKLIGHT_PWM_FREQ;
        lc.pwm_channel = BACKLIGHT_PWM_CHANNEL;
        light_.config(lc);
        light = &light_;
    }

    displays_[0].configure(&busLeft_, PIN_TFT_CS_LEFT, SHARED_RST ? -1 : PIN_TFT_RST,
                           DISPLAY_ROTATION_LEFT, light);
    displays_[1].configure(SHARED_DC ? &busLeft_ : &busRight_, PIN_TFT_CS_RIGHT,
                           SHARED_RST ? -1 : PIN_TFT_RST_RIGHT, DISPLAY_ROTATION_RIGHT, nullptr);

    // Deselect both panels before the first byte goes out, so the second
    // panel does not listen to the init sequence of the first one
    pinMode(PIN_TFT_CS_LEFT, OUTPUT);
    pinMode(PIN_TFT_CS_RIGHT, OUTPUT);
    digitalWrite(PIN_TFT_CS_LEFT, HIGH);
    digitalWrite(PIN_TFT_CS_RIGHT, HIGH);

    if (SHARED_RST) hardwareReset();

    for (uint8_t i = 0; i < 2; ++i)
    {
        ok_[i] = displays_[i].init();
        if (ok_[i])
        {
            displays_[i].setRotation(displays_[i].configuredRotation());
            displays_[i].fillScreen(lgfx::color565((COLOR_BACKGROUND >> 16) & 0xFF,
                                                   (COLOR_BACKGROUND >> 8) & 0xFF,
                                                   COLOR_BACKGROUND & 0xFF));
        }
    }

    if (SPI_FORCE_IOMUX) restoreIoMux();
    setBacklight(BACKLIGHT_BRIGHTNESS);

    initMs_ = millis() - t0;
    return ok_[0] && ok_[1];
}

void DisplayManager::hardwareReset()
{
    // One reset pulse for both panels (GC9A01: >= 10 us low, 120 ms until ready)
    pinMode(PIN_TFT_RST, OUTPUT);
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(5);
    digitalWrite(PIN_TFT_RST, LOW);
    delay(20);
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(150);
}

void DisplayManager::restoreIoMux()
{
    // The Arduino SPI driver routes the pins through the GPIO matrix. GPIO6 and
    // GPIO7 are the native FSPICLK/FSPID pins of the ESP32-C3; connecting them
    // directly through IO_MUX allows the documented 80 MHz.
    if (PIN_SPI_SCLK == 6 && PIN_SPI_MOSI == 7)
    {
        gpio_iomux_out(6, FUNC_MTCK_FSPICLK, false);
        gpio_iomux_out(7, FUNC_MTDO_FSPID, false);
    }
}

// ============================================================
// OUTPUT
// ============================================================

void DisplayManager::pushStrip(EyeSide side, int16_t x, int16_t y, int16_t w, int16_t h,
                               const uint16_t* data)
{
    const int8_t idx = (int8_t)side;
    if (!ok_[idx] || w <= 0 || h <= 0) return;

    if (active_ != idx)
    {
        finish();                                // never two selected panels at once
        displays_[idx].startWrite();
        active_ = idx;
    }

    // setWindow() waits until the previous DMA burst has left the bus,
    // which is exactly what makes the renderer's double buffering safe
    EyeDisplay& d = displays_[idx];
    d.setWindow(x, y, x + w - 1, y + h - 1);
    d.writePixelsDMA(data, (int32_t)w * h, false);   // false = data already in panel byte order
}

void DisplayManager::finish()
{
    if (active_ < 0) return;
    EyeDisplay& d = displays_[active_];
    // A plain command waits for the DMA burst and clears the DMA enable bit
    // of this bus object before the next panel takes over the shared SPI host
    d.writeCommand(GC9A01_CMD_RAMWR);
    d.endWrite();
    active_ = -1;
}

void DisplayManager::fill(EyeSide side, uint16_t color565)
{
    finish();
    if (ok_[(uint8_t)side]) displays_[(uint8_t)side].fillScreen(color565);
}

void DisplayManager::setBacklight(uint8_t brightness)
{
    if (PIN_BACKLIGHT >= 0 && ok_[0]) displays_[0].setBrightness(brightness);
}

void DisplayManager::printReport(Print& out) const
{
    out.printf("[disp] SPI2 @ %lu MHz, SCLK=GPIO%d MOSI=GPIO%d, DC %s, RST %s, IO_MUX re-route %s\n",
               (unsigned long)(SPI_WRITE_FREQ_HZ / 1000000UL), PIN_SPI_SCLK, PIN_SPI_MOSI,
               SHARED_DC ? "shared" : "separate", SHARED_RST ? "shared" : "separate",
               SPI_FORCE_IOMUX ? "on" : "off");
    out.printf("[disp] display 1 (left,  CS=GPIO%d, rotation %u): %s\n", PIN_TFT_CS_LEFT,
               DISPLAY_ROTATION_LEFT, ok_[0] ? "init sequence sent" : "INIT FAILED");
    out.printf("[disp] display 2 (right, CS=GPIO%d, rotation %u): %s\n", PIN_TFT_CS_RIGHT,
               DISPLAY_ROTATION_RIGHT, ok_[1] ? "init sequence sent" : "INIT FAILED");
    out.printf("[disp] init took %lu ms. Note: without MISO the panels cannot be read back -\n"
               "       'init sequence sent' does not prove a display is connected.\n",
               (unsigned long)initMs_);
}
