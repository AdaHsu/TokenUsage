#pragma once
#include <Arduino.h>
#include <vector>

struct ScannedNetwork {
    String ssid;
    int    rssi;
};

namespace WifiScan {
    // Scans and returns visible networks, deduped by SSID (keeping the
    // strongest RSSI seen for repeats across channels), sorted strongest
    // first. Hidden SSIDs are dropped since there is nothing to show for
    // them - the caller's form still takes free text for those.
    //
    // Works whether the radio is idle (provisioning's AP+STA setup) or
    // already associated (the admin page, mid-connection) - scanning while
    // connected causes a brief hiccup as the radio hops channels, so callers
    // on an already-online device should only do this on explicit request,
    // not on every page load.
    std::vector<ScannedNetwork> scan();
}
