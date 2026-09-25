#pragma once
#include <Arduino.h>

// Thin wrapper over WiFiClientSecure + HTTPClient so provider modules don't
// each repeat the TLS / timeout / error-mapping boilerplate.
namespace Http {
    struct Header { const char* name; String value; };

    struct Response {
        int    status = 0;     // HTTP status, 0 when the request never went out
        String body;
        bool   ok() const { return status >= 200 && status < 300; }
    };

    Response get(const String& url, const Header* headers, size_t headerCount,
                 uint32_t timeoutMs = 10000);

    Response post(const String& url, const String& body,
                  const Header* headers, size_t headerCount,
                  uint32_t timeoutMs = 15000);
}
