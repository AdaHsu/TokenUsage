#include "wifi_manager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

namespace {
const unsigned long CONNECT_TIMEOUT_MS = 20000;

unsigned long downSinceMs = 0;
int           failures    = 0;
bool          mdnsStarted = false;
String        mdnsHost;
}  // namespace

bool WifiManager::isConnected() { return WiFi.status() == WL_CONNECTED; }
String WifiManager::ip()        { return isConnected() ? WiFi.localIP().toString() : String(); }
String WifiManager::ssid()      { return isConnected() ? WiFi.SSID() : String(); }
int    WifiManager::rssi()      { return isConnected() ? WiFi.RSSI() : 0; }
int    WifiManager::failedRounds() { return failures; }
void   WifiManager::resetFailures() { failures = 0; }

void WifiManager::startMdns(const char* hostname) {
    mdnsHost = hostname;
    if (mdnsStarted) MDNS.end();
    mdnsStarted = MDNS.begin(hostname);
    if (mdnsStarted) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[wifi] mDNS: http://%s.local\n", hostname);
    }
}

bool WifiManager::connectAny(const DeviceSettings& s, ProgressFn onProgress) {
    if (s.wifiCount == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // Which saved networks are actually in range right now.
    bool visible[MAX_WIFIS] = { false };
    int  rssiOf[MAX_WIFIS]  = { 0 };
    int  found = WiFi.scanNetworks();
    for (int i = 0; i < found; i++) {
        int j = Settings::findWifi(s, WiFi.SSID(i));
        if (j >= 0 && !visible[j]) {
            visible[j] = true;
            rssiOf[j]  = WiFi.RSSI(i);
        }
    }
    WiFi.scanDelete();

    int visibleCount = 0;
    for (int i = 0; i < s.wifiCount; i++) {
        if (visible[i]) visibleCount++;
    }
    // A scan can miss a network that is really there (hidden SSID, bad
    // timing), so fall back to trying everything rather than giving up.
    bool tryAll = (visibleCount == 0);
    if (tryAll) Serial.println("[wifi] scan found none of the saved networks, trying all");

    int attempt = 0;
    int total   = tryAll ? s.wifiCount : visibleCount;

    for (int i = 0; i < s.wifiCount; i++) {
        if (!tryAll && !visible[i]) continue;
        const WifiCred& c = s.wifis[i];
        if (c.ssid.isEmpty()) continue;

        Serial.printf("[wifi] trying %s (priority %d, %d dBm)\n",
                      c.ssid.c_str(), i, rssiOf[i]);
        if (onProgress) onProgress(c.ssid, attempt, total);
        attempt++;

        WiFi.begin(c.ssid.c_str(), c.password.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[wifi] connected to %s as %s\n",
                          c.ssid.c_str(), WiFi.localIP().toString().c_str());
            failures    = 0;
            downSinceMs = 0;
            if (!mdnsHost.isEmpty()) startMdns(mdnsHost.c_str());
            return true;
        }
        WiFi.disconnect(false, true);
    }
    return false;
}

bool WifiManager::keepAlive(const DeviceSettings& settings, unsigned long graceMs) {
    if (isConnected()) {
        downSinceMs = 0;
        return true;
    }

    unsigned long now = millis();
    if (downSinceMs == 0) {
        downSinceMs = now;
        return false;
    }
    if (now - downSinceMs < graceMs) return false;

    // Down long enough to be real. Walk the priority list again; a reboot
    // would lose the cached usage data for no benefit.
    Serial.println("[wifi] link down, retrying the saved networks");
    downSinceMs = 0;
    if (connectAny(settings, nullptr)) return true;

    failures++;
    return false;
}
