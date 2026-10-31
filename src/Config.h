/*
 * Pumpkin Demon Eyes  -  Seeed XIAO ESP32-C3 + 2x GC9A01 (240x240)
 * -----------------------------------------------------------------
 * Purpose  : Central configuration. Every value that is meant to be tuned
 *            (wiring, eye geometry, animation timing, colours) lives here.
 *            Changing values in this file never requires touching the
 *            animation, pupil or blink code.
 * Wiring   : see README.md, section "Anschlussplan"
 *
 * Coordinate conventions used throughout this file
 *   - Display pixels: x to the right, y downwards, origin top-left,
 *     after the display rotation (DISPLAY_ROTATION_*) has been applied.
 *   - "Left"/"right" eye: as seen by a VIEWER standing in front of the
 *     pumpkin. Display 1 = left eye, display 2 = right eye.
 *   - Eye-local units: pixels of the eye design at scale 1.0. The rendered
 *     size on screen is local size * EyeGeometryConfig::scale.
 */

#pragma once

#include <stdint.h>

// ============================================================
// OPERATING MODE
// ============================================================

enum class AppMode : uint8_t
{
    Animation,      // normal operation: autonomous demon eyes
    HardwareTest,   // diagnostic sequence: colours, rotation, geometry, animation demo
    Calibration     // static eye + alignment overlay for mounting in the pumpkin
};

constexpr AppMode  APP_MODE                = AppMode::Animation;

constexpr uint32_t SERIAL_BAUD             = 115200;
constexpr uint32_t SERIAL_WAIT_MS          = 1500;   // wait this long for a USB host at boot (0 = no wait)
constexpr uint32_t STATS_INTERVAL_MS       = 5000;   // FPS / timing report period on Serial (0 = off)
constexpr bool     LOG_STATE_CHANGES       = true;   // print every animation state change on Serial

// ============================================================
// HARDWARE  (XIAO ESP32-C3 pin numbers are GPIO numbers, not Dx labels)
// ============================================================

// Display panel
constexpr int16_t  DISPLAY_WIDTH           = 240;
constexpr int16_t  DISPLAY_HEIGHT          = 240;
constexpr bool     DISPLAY_INVERT          = true;   // most GC9A01 modules are IPS -> colours need inversion
constexpr bool     DISPLAY_RGB_ORDER       = false;  // set true if red and blue are swapped

// Display rotation (whole coordinate system, handled by LovyanGFX)
//   0..3 = 0/90/180/270 degrees clockwise, 4..7 = same but mirrored by the panel.
//   This is independent of the per-eye rotationDeg further below.
//   Default: both displays mounted turned by 90 degrees (left counter-clockwise,
//   right clockwise), as in the reference pumpkin. Use 0 for upright displays.
constexpr uint8_t  DISPLAY_ROTATION_LEFT   = 3;
constexpr uint8_t  DISPLAY_ROTATION_RIGHT  = 1;

// SPI bus (shared by both displays)
//   GPIO6/GPIO7 are the native FSPI IO_MUX pins of the ESP32-C3 (FSPICLK/FSPID).
//   GPIO8 (D8) is a strapping pin and is deliberately NOT used.
constexpr int16_t  PIN_SPI_SCLK            = 6;      // D4
constexpr int16_t  PIN_SPI_MOSI            = 7;      // D5
constexpr int16_t  PIN_SPI_MISO            = -1;     // not needed, displays are write-only
constexpr uint32_t SPI_WRITE_FREQ_HZ       = 40000000;  // 40 MHz safe default, 80 MHz see README
constexpr bool     SPI_FORCE_IOMUX         = false;  // re-route SCLK/MOSI via IO_MUX after init (only GPIO6/7)

// Chip select (one per display, always separate)
constexpr int16_t  PIN_TFT_CS_LEFT         = 3;      // D1
constexpr int16_t  PIN_TFT_CS_RIGHT        = 10;     // D10

// Data/command and reset: shared or separate lines
constexpr bool     SHARED_DC               = true;   // both DC inputs on PIN_TFT_DC
constexpr bool     SHARED_RST              = true;   // both RST inputs on PIN_TFT_RST
constexpr int16_t  PIN_TFT_DC              = 4;      // D2  (left display, or both if shared)
constexpr int16_t  PIN_TFT_RST             = 5;      // D3  (left display, or both if shared)
constexpr int16_t  PIN_TFT_DC_RIGHT        = 21;     // D6  only used if SHARED_DC == false
constexpr int16_t  PIN_TFT_RST_RIGHT       = 2;      // D0  only used if SHARED_RST == false (strapping pin, see README)

// Backlight (both BL inputs tied together; -1 = BL not connected / always on)
constexpr int16_t  PIN_BACKLIGHT           = -1;     // -1 = BL open / always on, 20 = D7 for PWM dimming
constexpr uint8_t  BACKLIGHT_PWM_CHANNEL   = 0;      // ESP32-C3 has LEDC channels 0..5
constexpr uint32_t BACKLIGHT_PWM_FREQ      = 22000;  // Hz, above the audible range
constexpr uint8_t  BACKLIGHT_BRIGHTNESS    = 255;    // 0..255

// Visible area of the round glass. Pixels outside this circle are never sent.
constexpr float    DISPLAY_VISIBLE_CENTER_X = 120.0f;
constexpr float    DISPLAY_VISIBLE_CENTER_Y = 120.0f;
constexpr float    DISPLAY_VISIBLE_RADIUS   = 120.0f;
constexpr bool     CLIP_TO_VISIBLE_CIRCLE   = true;

// ============================================================
// RENDERING
// ============================================================

constexpr uint16_t TARGET_FPS              = 24;     // frame limiter, 0 = render as fast as possible
constexpr uint8_t  RENDER_STRIP_ROWS       = 8;      // rows per DMA strip (2 strips of 240*rows*2 bytes in RAM)

// ============================================================
// EYE GEOMETRY  (one block per eye, the ONLY place to align the eyes)
// ============================================================
//
// Transformation order, local eye coordinates -> display pixels:
//   1. local (u, v), origin = eye centre
//   2. mirror   : u = -u if mirrorX, v = -v if mirrorY
//   3. scale    : * scale  (uniform)
//   4. rotate   : by rotationDeg, positive = CLOCKWISE on the screen
//   5. translate: + (centerX + offsetX, centerY + offsetY)
//   6. clip     : to the display and the visible circle
//
// Size on screen: horizontal half-width = radiusX * scale,
//                 vertical half-height  = radiusY * scale.
// radiusX/radiusY define the SHAPE (aspect), scale shrinks/grows the whole eye.
//
// mirrorX/mirrorY mirror the eye SHAPE (lid slant, inner corner). Gaze
// direction and the light reflex stay screen-consistent, so mirrored and
// non-mirrored eyes still look in the same direction.

struct EyeGeometryConfig
{
    int16_t centerX;      // eye centre, display pixels
    int16_t centerY;

    float   rotationDeg;  // extra rotation of the eye image, degrees, + = clockwise
    float   scale;        // uniform size factor (1.0 = design size)

    float   radiusX;      // half-width of the eye opening, local units
    float   radiusY;      // half-height of the upper lid arc, local units

    int16_t offsetX;      // mounting correction, display pixels, added after rotation
    int16_t offsetY;      // (+ = right / down)

    bool    mirrorX;      // mirror the eye shape horizontally
    bool    mirrorY;      // mirror the eye shape vertically
};

// Left eye (viewer's left). The inner corner (towards the nose) is on the
// +x side of its local design, i.e. on the right.
constexpr EyeGeometryConfig LEFT_EYE_CONFIG = {
    120, 120,         // centerX, centerY
    0.0f, 1.0f,       // rotationDeg, scale
    100.0f, 78.0f,    // radiusX, radiusY
    0, 0,             // offsetX, offsetY
    false, false      // mirrorX, mirrorY
};

// Right eye (viewer's right): mirrored so that its inner corner points left.
constexpr EyeGeometryConfig RIGHT_EYE_CONFIG = {
    120, 120,
    0.0f, 1.0f,
    100.0f, 78.0f,
    0, 0,
    true, false
};

// Shape of the eye design, shared by both eyes (fractions of the eye radii)
constexpr float EYE_LOWER_LID_RATIO        = 0.78f;  // lower arc height relative to radiusY
constexpr float EYE_CLOSE_LINE             = 0.30f;  // where the lids meet, fraction of radiusY below centre
constexpr float EYE_HALO_WIDTH             = 10.0f;  // outer glow width beyond the lid rim, local units
constexpr float EYE_LID_RIM_WIDTH          = 4.0f;   // glowing lid edge width, local units
constexpr float EYE_LID_SHADOW_WIDTH       = 12.0f;  // lid shadow on the eyeball, local units

constexpr float IRIS_RADIUS_RATIO          = 0.64f;  // iris radius relative to radiusY
constexpr float IRIS_FORESHORTEN           = 0.16f;  // iris squash when looking sideways (0 = off)

constexpr float PUPIL_RADIUS_MIN           = 0.16f;  // round pupil radius at size 0, relative to iris radius
constexpr float PUPIL_RADIUS_MAX           = 0.52f;  // round pupil radius at size 1
constexpr float PUPIL_SLIT_WIDTH           = 0.24f;  // width factor of a full slit (pupilSlit = 1)
constexpr float PUPIL_SLIT_HEIGHT          = 1.85f;  // height factor of a full slit
constexpr float PUPIL_GLOW_MARGIN          = 0.50f;  // reach of the glow around the pupil, fraction of iris radius

constexpr float GAZE_RANGE_X               = 0.46f;  // max horizontal pupil travel, fraction of radiusX
constexpr float GAZE_RANGE_Y               = 0.30f;  // max vertical pupil travel, fraction of radiusY

constexpr float HIGHLIGHT_X                = -0.36f; // main reflex position, fraction of iris radius
constexpr float HIGHLIGHT_Y                = -0.40f; // (- = up / towards the viewer's left)
constexpr float HIGHLIGHT_RADIUS           = 0.17f;  // main reflex radius, fraction of iris radius
constexpr float HIGHLIGHT2_X               = 0.30f;  // secondary reflex
constexpr float HIGHLIGHT2_Y               = 0.34f;
constexpr float HIGHLIGHT2_RADIUS          = 0.07f;
constexpr uint8_t HIGHLIGHT_ALPHA          = 26;     // 0..32 opacity of the main reflex
constexpr uint8_t HIGHLIGHT2_ALPHA         = 16;

constexpr float ANGER_LID_DROP             = 0.30f;  // upper lid lowering at anger 1, fraction of radiusY
constexpr float ANGER_LID_SLANT            = 0.55f;  // inner corner extra drop at anger 1
constexpr float ANGER_LOWER_SQUINT         = 0.20f;  // lower lid rise at anger 1

// ============================================================
// COLOURS  (0xRRGGBB, converted to RGB565 at runtime)
// ============================================================

constexpr uint32_t COLOR_BACKGROUND        = 0x000000;  // everything outside the eye
constexpr uint32_t COLOR_LID_RIM           = 0xFF3A08;  // glowing lid edge
constexpr uint32_t COLOR_HALO              = 0x8A0A00;  // glow around the eye opening
constexpr uint32_t COLOR_SCLERA_CENTER     = 0x5A0606;  // eyeball centre (dark red)
constexpr uint32_t COLOR_SCLERA_EDGE       = 0x0E0104;  // eyeball edge
constexpr uint32_t COLOR_IRIS_INNER        = 0xFFC21A;  // iris near the pupil (yellow-orange)
constexpr uint32_t COLOR_IRIS_MID          = 0xFF6A00;  // orange
constexpr uint32_t COLOR_IRIS_OUTER        = 0xC80C00;  // blood red
constexpr uint32_t COLOR_IRIS_RIM          = 0x3A0010;  // dark red / violet limbal ring
constexpr uint32_t COLOR_PUPIL             = 0x000000;
constexpr uint32_t COLOR_PUPIL_CORE        = 0x6A0030;  // glowing pupil core (dark violet-red)
constexpr uint32_t COLOR_GLOW              = 0xFFF070;  // light emitted around the pupil
constexpr uint32_t COLOR_HIGHLIGHT         = 0xFFF4D0;  // light reflex

// Glow intensity (1.0 = colours as defined above)
constexpr float GLOW_INTENSITY_SCALE       = 1.0f;   // master brightness of all glowing parts
constexpr float GLOW_PUPIL_STRENGTH        = 0.85f;  // light around the pupil
constexpr float GLOW_HALO_STRENGTH         = 0.85f;  // outer glow relative to the iris
constexpr float GLOW_RIM_STRENGTH          = 0.9f;   // lid edge glow
constexpr float GLOW_FLICKER_AMOUNT        = 0.10f;  // fire-like flicker, 0 = steady
constexpr float GLOW_FIRE_RINGS            = 2.5f;   // number of moving rings in the iris
constexpr float GLOW_FIRE_AMOUNT           = 0.14f;  // brightness modulation of those rings
constexpr float GLOW_FIRE_SPEED            = 0.45f;  // rings per second (outward motion)

// ============================================================
// ANIMATION
// ============================================================

constexpr uint32_t RANDOM_SEED             = 0;      // 0 = hardware random, otherwise fixed sequence

// Global speed factors (1.0 = default, < 1 faster, > 1 slower)
constexpr float ANIM_GAZE_SPEED            = 1.0f;   // scales all eye movement durations
constexpr float ANIM_PUPIL_SPEED           = 1.0f;   // scales all pupil size/shape transitions

// Eye movements
constexpr uint32_t SACCADE_BASE_MS         = 45;     // quick jump: base duration
constexpr uint32_t SACCADE_MS_PER_UNIT     = 70;     // + per unit of travel (full range = 1.0)
constexpr uint32_t GLIDE_MS_MIN            = 700;    // slow creeping look
constexpr uint32_t GLIDE_MS_MAX            = 2400;
constexpr uint32_t SNAP_MS                 = 85;     // aggressive snap (with overshoot)
constexpr float    MICRO_SACCADE_AMPLITUDE = 0.025f; // tiny jitter while fixating (gaze units)
constexpr uint32_t MICRO_SACCADE_MS_MIN    = 180;
constexpr uint32_t MICRO_SACCADE_MS_MAX    = 900;
constexpr uint32_t EYE_LAG_MS_MAX          = 20;     // normal delay between the two eyes
constexpr uint32_t EYE_INDEPENDENT_LAG_MS  = 80;     // creepy delay when one eye reacts on its own
constexpr float    EYE_INDEPENDENT_CHANCE  = 0.12f;  // chance per gaze change for that

// Blinking
constexpr uint32_t BLINK_INTERVAL_MIN_MS   = 1800;
constexpr uint32_t BLINK_INTERVAL_MAX_MS   = 6500;
constexpr float    DOUBLE_BLINK_CHANCE     = 0.18f;  // chance that an automatic blink is a double blink
constexpr uint32_t BLINK_CLOSE_MS          = 75;     // normal blink
constexpr uint32_t BLINK_HOLD_MS           = 35;
constexpr uint32_t BLINK_OPEN_MS           = 150;
constexpr uint32_t BLINK_FAST_CLOSE_MS     = 45;     // fast blink (also used for double blinks)
constexpr uint32_t BLINK_FAST_HOLD_MS      = 15;
constexpr uint32_t BLINK_FAST_OPEN_MS      = 85;
constexpr uint32_t BLINK_LONG_CLOSE_MS     = 130;    // long closing
constexpr uint32_t BLINK_LONG_HOLD_MS_MIN  = 350;
constexpr uint32_t BLINK_LONG_HOLD_MS_MAX  = 900;
constexpr uint32_t BLINK_LONG_OPEN_MS      = 320;
constexpr uint32_t BLINK_SLOW_CLOSE_MS     = 420;    // slow, heavy blink
constexpr uint32_t BLINK_SLOW_HOLD_MS      = 150;
constexpr uint32_t BLINK_SLOW_OPEN_MS      = 650;
constexpr uint32_t DOUBLE_BLINK_GAP_MS     = 70;
constexpr uint32_t BLINK_EYE_OFFSET_MS     = 10;     // max random delay between the two eyes

// Eye opening (base level outside blinks, 0.0 = closed, 1.0 = fully open)
constexpr float    OPENNESS_MIN            = 0.55f;
constexpr float    OPENNESS_MAX            = 0.82f;
constexpr float    IDLE_OPENNESS_MIN       = 0.70f;  // IDLE picks a random level in this range
constexpr float    IDLE_OPENNESS_MAX       = 0.82f;  // (clamped to OPENNESS_MIN..OPENNESS_MAX)

// Pupil (size 0..1, slit 0 = round .. 1 = narrow vertical slit)
constexpr float    PUPIL_SIZE_DEFAULT      = 0.45f;
constexpr float    PUPIL_SLIT_DEFAULT      = 0.55f;  // demonic oval by default
constexpr float    PUPIL_HIPPUS_AMPLITUDE  = 0.05f;  // slow natural pupil unrest
constexpr uint32_t PUPIL_CHANGE_MS         = 900;    // typical size/shape transition

// Glow levels (multiplied with GLOW_INTENSITY_SCALE)
constexpr float    GLOW_IDLE               = 0.72f;
constexpr float    GLOW_STARE              = 0.90f;
constexpr float    GLOW_EVIL               = 1.15f;
constexpr float    GLOW_SLEEP              = 0.18f;

// State durations
constexpr uint32_t IDLE_MS_MIN             = 2500;
constexpr uint32_t IDLE_MS_MAX             = 7000;
constexpr uint32_t LOOK_AROUND_MS_MIN      = 4000;
constexpr uint32_t LOOK_AROUND_MS_MAX      = 9000;
constexpr uint32_t LOOK_DWELL_MS_MIN       = 250;    // pause between two glances
constexpr uint32_t LOOK_DWELL_MS_MAX       = 1400;
constexpr uint32_t STARE_MS_MIN            = 3000;
constexpr uint32_t STARE_MS_MAX            = 7000;
constexpr uint32_t EVIL_GLARE_MS_MIN       = 2500;
constexpr uint32_t EVIL_GLARE_MS_MAX       = 5000;
constexpr uint32_t SLEEP_MS_MIN            = 2000;   // time spent closed (after closing)
constexpr uint32_t SLEEP_MS_MAX            = 6000;
constexpr uint32_t SLEEP_CLOSE_MS          = 2400;   // slow closing
constexpr uint32_t WAKE_OPEN_MS            = 2200;   // slow opening
constexpr uint32_t CALM_PHASE_MS_MIN       = 1500;   // eerie total stillness inside IDLE
constexpr uint32_t CALM_PHASE_MS_MAX       = 4000;
constexpr float    CALM_PHASE_CHANCE       = 0.30f;  // chance per IDLE phase

// Behaviour transition weights: row = current state, column = next state.
// Order: IDLE, LOOK_AROUND, STARE, EVIL_GLARE, SLEEP, WAKE_UP.
// Weights are relative (they do not need to add up to 100). SLEEP always
// continues with WAKE_UP. BLINK / DOUBLE_BLINK are short interruptions
// triggered by the blink timer and do not appear here.
constexpr uint8_t BEHAVIOUR_COUNT = 6;
constexpr uint8_t TRANSITION_WEIGHTS[BEHAVIOUR_COUNT][BEHAVIOUR_COUNT] = {
    //  IDLE  LOOK  STARE  EVIL  SLEEP  WAKE
    {    10,   40,    20,   15,     8,    0 },   // from IDLE
    {    45,   12,    15,   20,     8,    0 },   // from LOOK_AROUND
    {    40,   30,     0,   25,     5,    0 },   // from STARE
    {    55,   35,    10,    0,     0,    0 },   // from EVIL_GLARE
    {     0,    0,     0,    0,     0,  100 },   // from SLEEP
    {    45,   25,     5,   25,     0,    0 },   // from WAKE_UP
};

// ============================================================
// TEST / CALIBRATION MODE
// ============================================================

constexpr uint32_t TEST_STEP_MS            = 2500;   // duration of each hardware test step
constexpr uint32_t TEST_ANIMATION_STEP_MS  = 3500;   // duration of each animation demo step
constexpr uint32_t CALIBRATION_POSE_MS     = 1800;   // time per gaze pose in calibration mode
constexpr bool     CALIBRATION_SHOW_TEXT   = true;   // print parameters on the displays
