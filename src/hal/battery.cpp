#include "battery.h"
#include "board.h"

void Battery::begin() {
    pinMode(Board::PIN_BAT_ADC, INPUT);
    analogReadResolution(12);
}

int Battery::millivolts() {
    // The divider halves the pack voltage; average a few reads because the
    // ADC is noisy while WiFi is transmitting.
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) acc += analogReadMilliVolts(Board::PIN_BAT_ADC);
    return (int)((acc / 8) * 2);
}

int Battery::percent(int mv) {
    struct Point { int mv; int pct; };
    // Discharge curve for a single-cell LiPo under a light load.
    static const Point curve[] = {
        { 3300,   0 }, { 3600,  10 }, { 3700,  25 }, { 3750,  40 },
        { 3850,  60 }, { 3950,  75 }, { 4050,  90 }, { 4200, 100 },
    };
    static const int n = sizeof(curve) / sizeof(curve[0]);

    if (mv <= curve[0].mv)     return 0;
    if (mv >= curve[n - 1].mv) return 100;
    for (int i = 1; i < n; i++) {
        if (mv < curve[i].mv) {
            const Point& a = curve[i - 1];
            const Point& b = curve[i];
            return a.pct + (mv - a.mv) * (b.pct - a.pct) / (b.mv - a.mv);
        }
    }
    return 100;
}

bool Battery::isCharging(int mv) {
    return mv >= 4150;
}
