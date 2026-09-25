#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "widgets.h"

// Owns the panel and the off-screen sprite. Frames are composed into the
// sprite and pushed in one go so nothing flickers; the few full-screen
// countdown screens draw straight to the panel instead.
namespace Display {
    void begin();

    void setRotation(int r);          // 0..3, recreates the sprite
    int  rotation();

    void setBacklight(bool on);
    bool backlightOn();

    // ST7789 SLPIN/SLPOUT around light sleep.
    void panelSleep(bool in);

    int  width();
    int  height();
    bool landscape();

    // The surface modules draw on. Valid between beginFrame and endFrame.
    TFT_eSPI& canvas();

    void beginFrame(uint16_t color = Ui::COLOR_BG);
    void endFrame();

    // Direct-to-panel mode for the countdown screens: tears the sprite down,
    // forces the backlight on, and hands back the raw device.
    TFT_eSPI& beginDirect();
    void      endDirect();
}
