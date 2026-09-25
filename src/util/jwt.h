#pragma once
#include <Arduino.h>
#include <time.h>

// Minimal JWT reader. We never verify signatures — the token is handed to us
// by the user and we only want the claims it carries about itself (expiry,
// which ChatGPT account it belongs to).
namespace Jwt {
    struct Claims {
        bool   valid     = false;
        time_t expiresAt = 0;   // "exp"
        String accountId;       // ChatGPT account / workspace id, when present
        String planType;        // e.g. "plus", "business" — when present
    };

    Claims read(const String& token);

    // Days until expiry, negative when already expired, INT32_MAX if unknown.
    int daysUntilExpiry(const Claims& c, time_t now);
}
