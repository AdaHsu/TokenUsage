#pragma once
#include <Arduino.h>
#include "../core/settings.h"

// First-boot setup. Brings up an open SoftAP with a captive portal that asks
// for one thing only: WiFi. Credentials for the model providers are long
// opaque strings that belong on a real keyboard, so they are added later from
// the admin page over the network.
namespace Provisioning {
    // Blocks until the user submits a network, then returns the updated
    // settings. The caller is expected to reboot.
    DeviceSettings run(const DeviceSettings& current);
}
