#include "ntp.h"

static const time_t SANE_EPOCH = 1700000000;  // 2023-11-14, "clock is set"

bool Ntp::isSynced() {
    return time(nullptr) > SANE_EPOCH;
}

bool Ntp::sync(uint32_t timeoutMs) {
    setenv("TZ", "UTC0", 1);
    tzset();
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    unsigned long start = millis();
    while (!isSynced() && millis() - start < timeoutMs) {
        delay(200);
    }
    return isSynced();
}
