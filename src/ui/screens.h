#pragma once
#include <Arduino.h>
#include "../core/provider.h"

// Everything the shell draws. Provider modules never appear here beyond the
// one call that hands them their content rectangle.
namespace Screens {

// The header strip: which account is on screen, plus device status.
struct Chrome {
    int  index      = 0;    // 0-based position in the carousel
    int  total      = 0;
    int  batteryMv  = 0;
    int  refreshSec = 0;    // until the next automatic refresh
    bool stale      = false;  // last fetch failed, numbers are from cache
    bool loading    = false;  // a fetch is in flight right now
    bool wifiOk     = true;
};

// Device-level facts only. Deliberately short: which account is on screen is
// the stats header's job, and repeating it here helps nobody.
struct DeviceInfo {
    String        ssid;
    int           rssi       = 0;
    String        ip;
    String        host;
    int           batteryMv  = 0;
    unsigned long uptimeMs   = 0;
    int           refreshMin = 5;
};

void stats(Provider& provider, const Chrome& chrome, time_t now);
void info(const DeviceInfo& info);

// Shown for ~800 ms right after a switch so the new identity registers
// before the numbers do.
void switchOverlay(Provider& provider, int index, int total);

void loading(const String& msg);
void connecting(const String& ssid, int index, int total);
void provisioning(const String& apSsid, const String& ip);
void message(const String& title, const String& body, uint16_t color);
void noAccounts(const String& url);

// Full-screen cancellable countdown, drawn straight to the panel.
void sleepArmed(const char* title, int secondsLeft, const char* subtitle);
void deepSleep();
void resetting();

// Logo, briefly, right after Display::begin() and before anything WiFi- or
// account-related has a chance to draw. Purely decorative - never blocks on
// anything, the caller decides how long it stays up.
void splash();

}  // namespace Screens
