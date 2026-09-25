#include "wifi_scan.h"
#include <WiFi.h>
#include <algorithm>

std::vector<ScannedNetwork> WifiScan::scan() {
    std::vector<ScannedNetwork> out;

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;  // hidden network, nothing to show

        bool dupe = false;
        for (auto& s : out) {
            if (s.ssid == ssid) {
                dupe = true;
                if (WiFi.RSSI(i) > s.rssi) s.rssi = WiFi.RSSI(i);
                break;
            }
        }
        if (!dupe) out.push_back({ ssid, (int)WiFi.RSSI(i) });
    }
    WiFi.scanDelete();

    std::sort(out.begin(), out.end(),
              [](const ScannedNetwork& a, const ScannedNetwork& b) { return a.rssi > b.rssi; });
    return out;
}
