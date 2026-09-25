#pragma once
#include <Arduino.h>

static const int MAX_WIFIS = 8;

struct WifiCred {
    String ssid;
    String password;
};

// Everything that is not an account: the WiFi list and device-level
// preferences. Accounts live in account.h because they are the part that
// grows per provider module.
struct DeviceSettings {
    WifiCred wifis[MAX_WIFIS];
    int      wifiCount = 0;

    int    refreshMin    = 5;   // 1..60 - how often the visible account refetches
    int    idleSleepMin  = 5;   // 0 disables tier-2 light sleep
    int    rotation      = 1;   // 0..3
    String adminUser     = "admin";
    String adminPassword = "admin";

    bool hasWifi() const { return wifiCount > 0; }
};

namespace Settings {
    void load(DeviceSettings& out);
    bool save(const DeviceSettings& s);

    void saveRotation(int rotation);   // written on its own so a BOOT press is cheap
    void clearAll();                   // factory reset: wipes the whole namespace

    int  findWifi(const DeviceSettings& s, const String& ssid);
    bool addOrUpdateWifi(DeviceSettings& s, const String& ssid, const String& password);
    bool removeWifiAt(DeviceSettings& s, int index);
    // Swap with the neighbour at index+delta. List order is connection
    // priority, so this is what the up/down arrows in the admin UI do.
    bool moveWifi(DeviceSettings& s, int index, int delta);
}
