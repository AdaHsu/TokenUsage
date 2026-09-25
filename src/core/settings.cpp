#include "settings.h"
#include "nvs_ns.h"
#include <Preferences.h>

namespace {
void wifiKeys(int i, char* ssidKey, char* passKey) {
    snprintf(ssidKey, 8, "w%ds", i);
    snprintf(passKey, 8, "w%dp", i);
}
}  // namespace

void Settings::load(DeviceSettings& out) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return;

    out.refreshMin    = p.getInt("refMin", 5);
    out.idleSleepMin  = p.getInt("idleMin", 5);
    out.rotation      = p.getInt("rot", 1);
    out.adminUser     = p.getString("admU", "admin");
    out.adminPassword = p.getString("admP", "admin");

    int count = p.getInt("wN", 0);
    if (count < 0)         count = 0;
    if (count > MAX_WIFIS) count = MAX_WIFIS;
    out.wifiCount = count;
    for (int i = 0; i < count; i++) {
        char sk[8], pk[8];
        wifiKeys(i, sk, pk);
        out.wifis[i].ssid     = p.getString(sk, "");
        out.wifis[i].password = p.getString(pk, "");
    }
    p.end();

    if (out.refreshMin < 1 || out.refreshMin > 60) out.refreshMin = 5;
}

bool Settings::save(const DeviceSettings& s) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return false;

    p.putInt("refMin", s.refreshMin);
    p.putInt("idleMin", s.idleSleepMin);
    p.putInt("rot", s.rotation);
    p.putString("admU", s.adminUser);
    p.putString("admP", s.adminPassword);

    // Rewrite the list, then drop any trailing rows a shorter list leaves behind.
    p.putInt("wN", s.wifiCount);
    for (int i = 0; i < s.wifiCount; i++) {
        char sk[8], pk[8];
        wifiKeys(i, sk, pk);
        p.putString(sk, s.wifis[i].ssid);
        p.putString(pk, s.wifis[i].password);
    }
    for (int i = s.wifiCount; i < MAX_WIFIS; i++) {
        char sk[8], pk[8];
        wifiKeys(i, sk, pk);
        p.remove(sk);
        p.remove(pk);
    }
    p.end();
    return true;
}

void Settings::saveRotation(int rotation) {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.putInt("rot", rotation & 3);
        p.end();
    }
}

void Settings::clearAll() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}

int Settings::findWifi(const DeviceSettings& s, const String& ssid) {
    for (int i = 0; i < s.wifiCount; i++) {
        if (s.wifis[i].ssid == ssid) return i;
    }
    return -1;
}

bool Settings::addOrUpdateWifi(DeviceSettings& s, const String& ssid, const String& password) {
    if (ssid.isEmpty()) return false;
    int idx = findWifi(s, ssid);
    if (idx >= 0) {
        s.wifis[idx].password = password;
        return true;
    }
    if (s.wifiCount >= MAX_WIFIS) return false;
    s.wifis[s.wifiCount].ssid     = ssid;
    s.wifis[s.wifiCount].password = password;
    s.wifiCount++;
    return true;
}

bool Settings::removeWifiAt(DeviceSettings& s, int index) {
    if (index < 0 || index >= s.wifiCount) return false;
    for (int i = index; i < s.wifiCount - 1; i++) s.wifis[i] = s.wifis[i + 1];
    s.wifis[s.wifiCount - 1] = WifiCred{};
    s.wifiCount--;
    return true;
}

bool Settings::moveWifi(DeviceSettings& s, int index, int delta) {
    int dst = index + delta;
    if (index < 0 || index >= s.wifiCount) return false;
    if (dst < 0 || dst >= s.wifiCount)     return false;
    WifiCred tmp   = s.wifis[index];
    s.wifis[index] = s.wifis[dst];
    s.wifis[dst]   = tmp;
    return true;
}
