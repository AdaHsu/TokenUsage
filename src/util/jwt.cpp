#include "jwt.h"
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <memory>

namespace {

// base64url -> raw bytes. Returns an empty String on malformed input.
String decodeSegment(const String& seg) {
    String b64 = seg;
    b64.replace('-', '+');
    b64.replace('_', '/');
    while (b64.length() % 4 != 0) b64 += '=';

    size_t outLen = 0;
    // First call sizes the output buffer.
    mbedtls_base64_decode(nullptr, 0, &outLen,
                          (const unsigned char*)b64.c_str(), b64.length());
    if (outLen == 0 || outLen > 8192) return String();

    std::unique_ptr<unsigned char[]> buf(new (std::nothrow) unsigned char[outLen + 1]);
    if (!buf) return String();
    size_t written = 0;
    if (mbedtls_base64_decode(buf.get(), outLen, &written,
                              (const unsigned char*)b64.c_str(), b64.length()) != 0) {
        return String();
    }
    buf[written] = '\0';
    return String((const char*)buf.get());
}

// The account id lives under a namespaced auth claim in ChatGPT tokens; the
// exact key has moved around, so probe the shapes we know about.
void extractAuthClaims(JsonVariantConst root, Jwt::Claims& out) {
    static const char* const authKeys[] = {
        "https://api.openai.com/auth",
        "https://chatgpt.com/auth",
        "auth",
    };
    for (const char* key : authKeys) {
        JsonVariantConst auth = root[key];
        if (auth.isNull()) continue;
        const char* acc = auth["chatgpt_account_id"] | auth["account_id"] | (const char*)nullptr;
        if (acc && *acc && out.accountId.isEmpty()) out.accountId = acc;
        const char* plan = auth["chatgpt_plan_type"] | auth["plan_type"] | (const char*)nullptr;
        if (plan && *plan && out.planType.isEmpty()) out.planType = plan;
    }
    // Flat fallbacks.
    if (out.accountId.isEmpty()) {
        const char* acc = root["chatgpt_account_id"] | (const char*)nullptr;
        if (acc && *acc) out.accountId = acc;
    }
}

}  // namespace

Jwt::Claims Jwt::read(const String& token) {
    Claims c;
    int firstDot = token.indexOf('.');
    if (firstDot < 0) return c;
    int secondDot = token.indexOf('.', firstDot + 1);
    if (secondDot < 0) return c;

    String payload = decodeSegment(token.substring(firstDot + 1, secondDot));
    if (payload.isEmpty()) return c;

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return c;

    c.valid     = true;
    c.expiresAt = (time_t)(doc["exp"] | 0);
    extractAuthClaims(doc.as<JsonVariantConst>(), c);
    return c;
}

int Jwt::daysUntilExpiry(const Claims& c, time_t now) {
    if (!c.valid || c.expiresAt <= 0 || now <= 0) return INT32_MAX;
    long secs = (long)(c.expiresAt - now);
    // Round towards zero so "0d" means "expires within the day".
    return (int)(secs / 86400);
}
