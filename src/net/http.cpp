#include "http.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace {

// Certificate validation is intentionally skipped: maintaining a root CA
// bundle on the device is not worth it for a read-only usage widget on a
// trusted LAN. The credentials it carries are the real secret, not the pipe.
Http::Response run(const String& url, const String& body, bool isPost,
                   const Http::Header* headers, size_t headerCount,
                   uint32_t timeoutMs) {
    Http::Response res;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(timeoutMs / 1000);

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(timeoutMs);
    http.setReuse(false);

    if (!http.begin(client, url)) return res;
    for (size_t i = 0; i < headerCount; i++) {
        http.addHeader(headers[i].name, headers[i].value);
    }

    int code = isPost ? http.POST(body) : http.GET();
    res.status = code;
    if (code > 0) res.body = http.getString();
    http.end();
    return res;
}

}  // namespace

Http::Response Http::get(const String& url, const Header* headers,
                         size_t headerCount, uint32_t timeoutMs) {
    return run(url, String(), false, headers, headerCount, timeoutMs);
}

Http::Response Http::post(const String& url, const String& body,
                          const Header* headers, size_t headerCount,
                          uint32_t timeoutMs) {
    return run(url, body, true, headers, headerCount, timeoutMs);
}
