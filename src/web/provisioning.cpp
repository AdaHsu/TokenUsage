#include "provisioning.h"
#include "webui.h"
#include "../hal/board.h"
#include "../ui/screens.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <vector>
#include <algorithm>

namespace {

WebServer      server(80);
DNSServer      dns;
DeviceSettings working;
bool           saved = false;

struct ScannedNetwork {
    String ssid;
    int    rssi;
};
std::vector<ScannedNetwork> scanned;

// AP+STA lets the radio keep hosting the portal while it scans, same as any
// commercial "pick your WiFi" setup flow. One scan per portal session is
// enough - a "Rescan" link re-triggers it if the list looks stale.
void rescan() {
    scanned.clear();
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;  // hidden network, nothing to show

        bool dupe = false;
        for (auto& s : scanned) {
            if (s.ssid == ssid) { dupe = true; if (WiFi.RSSI(i) > s.rssi) s.rssi = WiFi.RSSI(i); break; }
        }
        if (!dupe) scanned.push_back({ ssid, (int)WiFi.RSSI(i) });
    }
    WiFi.scanDelete();
    std::sort(scanned.begin(), scanned.end(),
              [](const ScannedNetwork& a, const ScannedNetwork& b) { return a.rssi > b.rssi; });
    Serial.printf("[setup] scan found %d network(s)\n", (int)scanned.size());
}

// Escapes for embedding inside a single-quoted JS string literal. The result
// still needs htmlEscape() around the whole attribute value afterwards, since
// it ends up inside an onclick="..." attribute.
String jsEscape(const String& s) {
    String out;
    out.reserve(s.length() + 4);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\\' || c == '\'') out += '\\';
        out += c;
    }
    return out;
}

String formPage(const String& error) {
    String body = WebUi::brand("setup");

    if (!error.isEmpty()) {
        body += "<div class=err>" + htmlEscape(error) + "</div>";
    }

    body += "<section><h2>WiFi</h2>";

    // <datalist> is the "correct" zero-JS way to offer suggestions on a text
    // field, but mobile captive-portal browsers (iOS's Captive Network
    // Assistant in particular) mostly don't render it at all. A row of plain
    // tappable buttons that fill the field via a one-line onclick works
    // everywhere a captive portal itself works, so use that instead.
    if (!scanned.empty()) {
        body += "<label>Networks in range</label><div class=actions>";
        for (const auto& s : scanned) {
            String onclick = "document.getElementById('ssid').value='" +
                             jsEscape(s.ssid) + "';return false;";
            body += "<button type=button class=\"btnGhost mini\" onclick=\"";
            body += htmlEscape(onclick);
            body += "\">" + htmlEscape(s.ssid) + " (" + String(s.rssi) + " dBm)</button>";
        }
        body += "</div>";
    } else {
        body += "<p class=hint>No networks found nearby. Type the name by hand, "
                "or try Rescan.</p>";
    }

    body += "<form method=post action=/save>";
    body += "<label>Network name";
    body += "<input id=ssid name=ssid autocomplete=off required></label>";
    body += "<label>Password";
    body += "<input type=password name=password autocomplete=off></label>";
    body += "<div class=actions><button type=submit>Save and restart</button>";
    body += "<a class=\"btn btnGhost\" href=/rescan>Rescan</a></div>";
    body += "</form>";

    body += "<p class=hint>Accounts are added afterwards from a computer: once this "
            "device is online its screen shows the address of the admin page.</p>";
    body += "</section>";

    if (working.wifiCount > 0) {
        body += "<section><h2>Already saved</h2>";
        for (int i = 0; i < working.wifiCount; i++) {
            body += "<div class=row><div class=grow><div class=name>";
            body += htmlEscape(working.wifis[i].ssid);
            body += "</div></div><span class=pill>" + String(i + 1) + "</span></div>";
        }
        body += "</section>";
    }
    return WebUi::page("TokenUsage setup", body);
}

void handleRoot() {
    server.send(200, "text/html", formPage(""));
}

void handleRescan() {
    rescan();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

void handleSave() {
    String ssid = server.arg("ssid");
    ssid.trim();
    if (ssid.isEmpty()) {
        server.send(200, "text/html", formPage("A network name is required."));
        return;
    }

    if (!Settings::addOrUpdateWifi(working, ssid, server.arg("password"))) {
        server.send(200, "text/html", formPage("The network list is full."));
        return;
    }
    Settings::save(working);

    String body = WebUi::brand("setup");
    body += "<div class=ok>Saved. The device is restarting and will join <b>";
    body += htmlEscape(ssid);
    body += "</b>.</div>";
    server.send(200, "text/html", WebUi::page("Saved", body));

    saved = true;
}

}  // namespace

DeviceSettings Provisioning::run(const DeviceSettings& current) {
    working = current;
    saved   = false;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(Board::AP_SSID);
    delay(300);
    rescan();

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[setup] AP %s at %s\n", Board::AP_SSID, ip.toString().c_str());
    Screens::provisioning(Board::AP_SSID, ip.toString());

    // Answer every lookup with our own address so phones pop the portal.
    dns.start(53, "*", ip);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/rescan", HTTP_GET, handleRescan);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);
    server.begin();

    while (!saved) {
        dns.processNextRequest();
        server.handleClient();
        delay(5);
    }

    delay(800);  // let the confirmation page finish sending
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    return working;
}
