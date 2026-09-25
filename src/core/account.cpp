#include "account.h"
#include "nvs_ns.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

namespace {
const char* ACCOUNTS_PATH = "/accounts.json";

bool writeJson(const JsonDocument& doc) {
    File f = LittleFS.open(ACCOUNTS_PATH, "w");
    if (!f) return false;
    bool ok = serializeJson(doc, f) > 0 || doc.as<JsonArrayConst>().size() == 0;
    f.close();
    return ok;
}

bool readJson(JsonDocument& doc) {
    File f = LittleFS.open(ACCOUNTS_PATH, "r");
    if (!f) return false;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    return !err;
}
}  // namespace

bool Accounts::begin() {
    // true = format LittleFS if the spiffs partition has never been
    // initialised (first boot, or after a factory reset that wiped it).
    if (!LittleFS.begin(true)) {
        Serial.println("[accounts] LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists(ACCOUNTS_PATH)) {
        JsonDocument empty;
        empty.to<JsonArray>();
        writeJson(empty);
    }
    return true;
}

void Accounts::load(std::vector<AccountRecord>& out) {
    out.clear();

    JsonDocument doc;
    if (!readJson(doc)) return;

    for (JsonVariantConst v : doc.as<JsonArrayConst>()) {
        if (out.size() >= (size_t)MAX_ACCOUNTS) break;
        AccountRecord r;
        r.type = v["type"] | "";
        if (r.type.isEmpty()) continue;
        r.label   = v["label"]   | "";
        r.enabled = v["enabled"] | true;

        // The settings sub-object is itself provider-owned JSON; re-serialise
        // it back to a string rather than trying to interpret it here.
        JsonVariantConst settings = v["settings"];
        String blob;
        if (settings.isNull()) blob = "{}";
        else                   serializeJson(settings, blob);
        r.settings = blob;

        out.push_back(r);
    }
}

bool Accounts::saveAll(const std::vector<AccountRecord>& list) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    size_t count = list.size();
    if (count > (size_t)MAX_ACCOUNTS) count = MAX_ACCOUNTS;

    for (size_t i = 0; i < count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["type"]    = list[i].type;
        o["label"]   = list[i].label;
        o["enabled"] = list[i].enabled;

        // Parse the module's settings string back into a real sub-object so
        // the file on disk stays human-readable instead of a string-in-a-string.
        JsonDocument settingsDoc;
        if (deserializeJson(settingsDoc, list[i].settings)) {
            o["settings"].to<JsonObject>();  // malformed blob: store empty rather than fail the whole save
        } else {
            o["settings"] = settingsDoc;
        }
    }

    return writeJson(doc);
}

bool Accounts::saveSettingsAt(int index, const String& settings) {
    if (index < 0 || index >= MAX_ACCOUNTS) return false;

    JsonDocument doc;
    if (!readJson(doc)) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (index >= (int)arr.size()) return false;

    JsonDocument settingsDoc;
    if (deserializeJson(settingsDoc, settings)) return false;
    arr[index]["settings"] = settingsDoc;

    return writeJson(doc);
}

int Accounts::loadActiveIndex() {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return 0;
    int v = p.getInt("actIdx", 0);
    p.end();
    return v < 0 ? 0 : v;
}

void Accounts::saveActiveIndex(int index) {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.putInt("actIdx", index);
        p.end();
    }
}
