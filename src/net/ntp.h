#pragma once
#include <Arduino.h>
#include <time.h>

namespace Ntp {
    // Pins the process TZ to UTC so every timestamp in the firmware — parsed,
    // formatted or compared — is UTC. Blocks up to timeoutMs for the sync.
    bool sync(uint32_t timeoutMs = 10000);
    bool isSynced();
}
