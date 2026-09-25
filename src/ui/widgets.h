#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>

// Drawing primitives shared by the shell and by provider modules. A module
// is free to ignore all of them and draw whatever it likes inside its
// rectangle; these exist so the common shapes look the same everywhere.
namespace Ui {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t COLOR_BG      = rgb565(  8,  10,  16);
constexpr uint16_t COLOR_PANEL   = rgb565( 16,  19,  28);
constexpr uint16_t COLOR_DIVIDER = rgb565( 40,  44,  56);
constexpr uint16_t COLOR_TEXT    = rgb565(255, 255, 255);
constexpr uint16_t COLOR_MUTED   = rgb565(140, 146, 160);
constexpr uint16_t COLOR_BAR_BG  = rgb565( 32,  36,  46);
constexpr uint16_t COLOR_ACCENT  = rgb565( 90, 170, 255);
constexpr uint16_t COLOR_OK      = rgb565( 60, 200, 120);
constexpr uint16_t COLOR_MINT    = rgb565(120, 220, 180);
constexpr uint16_t COLOR_WARN    = rgb565(240, 200,  70);
constexpr uint16_t COLOR_ORANGE  = rgb565(245, 150,  60);
constexpr uint16_t COLOR_DANGER  = rgb565(235,  80,  80);

// Vendor tints for the header dot.
constexpr uint16_t COLOR_CLAUDE  = rgb565(217, 119,  87);
constexpr uint16_t COLOR_OPENAI  = rgb565( 16, 163, 127);

// How fast a quota is being spent relative to how much of the window has
// elapsed. ratio 1.0 means exactly on pace to finish the window at 100%.
struct Pace {
    double      used    = 0;   // 0..1
    double      elapsed = 0;   // 0..1
    double      ratio   = 0;
    const char* label   = "";
    uint16_t    color   = COLOR_MUTED;
};

Pace pace(double utilPercent, time_t resetsAt, long windowSeconds, time_t now);

// Rounded usage bar. markerPct draws the "where you should be" tick.
void bar(TFT_eSPI& g, int x, int y, int w, int h,
         double percent, uint16_t color, double markerPercent = -1);

// One quota window: title, percent, reset countdown, bar and pace label.
// Used by the Claude module for its two windows and by the Codex module for
// its primary/secondary ones.
void windowRow(TFT_eSPI& g, int x, int y, int w, int h,
               const String& title, double percent,
               time_t resetsAt, time_t now,
               const Pace& p, bool showPaceLabel);

// Compact bordered label. Returns the width it occupied so callers can stack
// badges left to right.
int badge(TFT_eSPI& g, int x, int y, const String& text, uint16_t color);
int badgeWidth(TFT_eSPI& g, const String& text);

void centeredMessage(TFT_eSPI& g, int x, int y, int w, int h,
                     const String& msg, uint16_t color = COLOR_MUTED);

// Truncates with an ellipsis so long account labels cannot push the header
// items off screen.
String fit(TFT_eSPI& g, const String& text, int maxWidth, uint8_t font);

}  // namespace Ui
