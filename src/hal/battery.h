#pragma once
#include <Arduino.h>

namespace Battery {
    void begin();
    int  millivolts();              // raw pack voltage
    int  percent(int mv);           // piecewise-linear LiPo curve, 0..100
    bool isCharging(int mv);        // USB feeds the rail above ~4.15 V
}
