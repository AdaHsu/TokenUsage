#pragma once
#include <Arduino.h>

// Shared chrome for both web surfaces. Everything is inline and hand-rolled:
// the setup portal runs with no internet at all, so a CDN stylesheet or font
// would simply hang.
namespace WebUi {
    extern const char HEAD_META[];
    extern const char STYLES[];
    extern const char ICON_SVG[];

    String page(const String& title, const String& body);
    String brand(const char* badge = nullptr);
}
