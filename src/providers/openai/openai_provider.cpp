#include "openai_provider.h"
#include "../../net/http.h"
#include "../../ui/widgets.h"
#include "../../util/timefmt.h"

namespace {

const char* USAGE_URL = "https://chatgpt.com/backend-api/wham/usage";
const char* TOKEN_URL = "https://auth.openai.com/oauth/token";
// The public client id the Codex CLI itself uses for its OAuth flow.
const char* CLIENT_ID = "app_EMoamEEZ73f0CkXaXp7hrann";

String windowLabel(long seconds) {
    if (seconds <= 0)      return "Window";
    if (seconds >= 604800) return "Weekly";
    if (seconds >= 86400)  return "Daily";
    if (seconds % 3600 == 0) return String(seconds / 3600) + "-hour";
    return String(seconds / 60) + "-min";
}

}  // namespace

uint16_t OpenAiProvider::accent() const { return Ui::COLOR_OPENAI; }

String OpenAiProvider::subLabel() const {
    String p = planType_;
    p.trim();
    p.toUpperCase();
    return p;
}

String OpenAiProvider::credentialId() const {
    if (accessToken_.length() < 12) return String();
    return accessToken_.substring(accessToken_.length() - 12);
}

String OpenAiProvider::credentialHint() const { return maskCredential(accessToken_); }

void OpenAiProvider::setCredential(const String& cred) {
    if (cred.isEmpty() || cred == accessToken_) return;
    accessToken_ = cred;
    readClaims();
    markDirty();
}

void OpenAiProvider::readClaims() {
    claims_ = Jwt::read(accessToken_);
    // The token names the account it belongs to; an explicit id in the form
    // still wins, which is how a second workspace under one login is set up.
    if (accountId_.isEmpty() && !claims_.accountId.isEmpty()) {
        accountId_ = claims_.accountId;
    }
    if (planType_.isEmpty() && !claims_.planType.isEmpty()) {
        planType_ = claims_.planType;
    }
}

int OpenAiProvider::tokenDaysLeft(time_t now) const {
    return Jwt::daysUntilExpiry(claims_, now);
}

void OpenAiProvider::loadSettings(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    accessToken_  = doc["accessToken"]  | "";
    refreshToken_ = doc["refreshToken"] | "";
    accountId_    = doc["accountId"]    | "";
    planType_     = doc["planType"]     | "";
    autoRefresh_  = doc["autoRefresh"]  | false;
    readClaims();
}

String OpenAiProvider::saveSettings() const {
    JsonDocument doc;
    doc["accessToken"]  = accessToken_;
    doc["refreshToken"] = refreshToken_;
    doc["accountId"]    = accountId_;
    doc["planType"]     = planType_;
    doc["autoRefresh"]  = autoRefresh_;
    String out;
    serializeJson(doc, out);
    return out;
}

bool OpenAiProvider::parseLimit(JsonVariantConst v, Limit& out, time_t now) {
    out = Limit{};
    if (v.isNull()) return false;

    // Percentages have appeared under a couple of names; take whichever is there.
    if (!v["used_percent"].isNull())      out.usedPercent = v["used_percent"].as<double>();
    else if (!v["used_percentage"].isNull()) out.usedPercent = v["used_percentage"].as<double>();
    else if (!v["percent_used"].isNull()) out.usedPercent = v["percent_used"].as<double>();
    else return false;

    out.windowSeconds = v["limit_window_seconds"] | 0L;
    if (out.windowSeconds == 0) out.windowSeconds = v["window_seconds"] | 0L;

    long resetAt = v["reset_at"] | 0L;
    if (resetAt > 0) {
        out.resetsAt = (time_t)resetAt;
    } else {
        long after = v["reset_after_seconds"] | 0L;
        if (after > 0 && now > 0) out.resetsAt = now + after;
    }

    const char* name = v["name"] | v["model"] | (const char*)nullptr;
    if (name && *name) out.name = name;

    out.valid = true;
    return true;
}

FetchResult OpenAiProvider::fetchUsage(time_t now) {
    Http::Header headers[4];
    size_t n = 0;
    headers[n++] = { "Authorization", "Bearer " + accessToken_ };
    headers[n++] = { "Accept",        "application/json" };
    headers[n++] = { "originator",    "codex_cli_rs" };
    if (!accountId_.isEmpty()) {
        headers[n++] = { "ChatGPT-Account-Id", accountId_ };
    }

    Http::Response res = Http::get(USAGE_URL, headers, n);
    if (res.status == 401 || res.status == 403) return FetchResult::Unauthorized;
    if (!res.ok()) {
        setError("Network error (HTTP " + String(res.status) + ").");
        return FetchResult::Network;
    }

    JsonDocument doc;
    if (deserializeJson(doc, res.body)) {
        setError("Bad response from chatgpt.com.");
        return FetchResult::Parse;
    }

    const char* plan = doc["plan_type"] | (const char*)nullptr;
    if (plan && *plan) planType_ = plan;

    resetCreditsAvailable_ = doc["rate_limit_reset_credits"]["available_count"] | -1;

    // Confirmed against a live response: the window objects are named
    // "*_window", not the bare "primary"/"secondary" the secondhand research
    // this was originally written from suggested.
    JsonVariantConst rl = doc["rate_limit"];
    parseLimit(rl["primary_window"], primary_, now);
    parseLimit(rl["secondary_window"], secondary_, now);

    extraCount_ = 0;
    for (JsonVariantConst v : doc["additional_rate_limits"].as<JsonArrayConst>()) {
        if (extraCount_ >= (int)(sizeof(extras_) / sizeof(extras_[0]))) break;

        // Each entry nests its own rate_limit.primary_window rather than
        // carrying used_percent directly; only the primary window is shown
        // per entry (the one live example had secondary_window: null here).
        Limit l;
        if (!parseLimit(v["rate_limit"]["primary_window"], l, now)) continue;

        const char* label = v["limit_name"] | v["normal_model_slug"] | (const char*)nullptr;
        if (label && *label) l.name = label;

        extras_[extraCount_++] = l;
    }

    if (!primary_.valid && !secondary_.valid && extraCount_ == 0) {
        // This endpoint is undocumented and the field names were guessed from
        // secondhand research, never confirmed against a live response. Dump
        // the raw body so the parser can be fixed against something real.
        Serial.println("[openai] no rate_limit/additional_rate_limits in response, raw body:");
        Serial.println(res.body);
        setError("No rate limit data in the response.");
        return FetchResult::Parse;
    }

    setError("");
    return FetchResult::Ok;
}

bool OpenAiProvider::tryRefreshToken() {
    if (refreshToken_.isEmpty()) return false;

    JsonDocument body;
    body["client_id"]     = CLIENT_ID;
    body["grant_type"]    = "refresh_token";
    body["refresh_token"] = refreshToken_;
    String payload;
    serializeJson(body, payload);

    Http::Header headers[2] = {
        { "Content-Type", "application/json" },
        { "Accept",       "application/json" },
    };

    Http::Response res = Http::post(TOKEN_URL, payload, headers, 2);
    if (!res.ok()) {
        Serial.printf("[openai] token refresh failed: HTTP %d\n", res.status);
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, res.body)) return false;

    const char* access = doc["access_token"] | (const char*)nullptr;
    if (!access || !*access) return false;
    accessToken_ = access;

    // The refresh token rotates: keep the new one or the next refresh fails.
    const char* fresh = doc["refresh_token"] | (const char*)nullptr;
    if (fresh && *fresh) refreshToken_ = fresh;

    readClaims();
    markDirty();
    Serial.println("[openai] access token refreshed");
    return true;
}

FetchResult OpenAiProvider::refresh(time_t now) {
    // Count this as an attempt whether it succeeds or not. The carousel's
    // interval gate keys off hasFetched()/lastFetchMs(); if only a success
    // set them, a bad token or a network hiccup would leave hasFetched()
    // false forever and dueForFetch() would fire on every loop tick instead
    // of backing off - a retry storm, not the configured 5-minute cadence.
    markFetched(now);

    if (accessToken_.isEmpty()) {
        setError("No access token configured.");
        return FetchResult::NotConfigured;
    }

    FetchResult r = fetchUsage(now);

    // A rejected token is worth one automatic retry, but only when the user
    // opted in: refreshing rotates the token and can log the desktop Codex
    // CLI out of the same account.
    if (r == FetchResult::Unauthorized && autoRefresh_ && tryRefreshToken()) {
        r = fetchUsage(now);
    }

    if (r == FetchResult::Unauthorized) {
        setNeedsAuth(true);
        setError("Token rejected. Paste a fresh one.");
        return r;
    }
    if (r != FetchResult::Ok) return r;

    setNeedsAuth(false);
    return FetchResult::Ok;
}

void OpenAiProvider::show(const RenderCtx& ctx) {
    TFT_eSPI& g = ctx.g;

    if (needsAuth()) {
        g.setTextDatum(MC_DATUM);
        g.setTextColor(Ui::COLOR_DANGER, Ui::COLOR_BG);
        g.drawString("Re-authenticate", ctx.x + ctx.w / 2, ctx.y + ctx.h / 2 - 12, 4);
        g.setTextColor(Ui::COLOR_MUTED, Ui::COLOR_BG);
        g.drawString("paste a fresh Codex token", ctx.x + ctx.w / 2,
                     ctx.y + ctx.h / 2 + 14, 2);
        return;
    }

    if (!hasData()) {
        String msg = lastError().isEmpty() ? String("No usage data") : lastError();
        Ui::centeredMessage(g, ctx.x, ctx.y, ctx.w, ctx.h, msg, Ui::COLOR_MUTED);
        return;
    }

    int y = ctx.y + 2;

    // Token lifetime is this module's own business, so it warns here rather
    // than on the device Info screen.
    int days = tokenDaysLeft(ctx.now);
    if (days != INT32_MAX) {
        String txt = days < 0 ? String("token expired")
                              : String("token ") + String(days) + "d";
        uint16_t col = (days < 0) ? Ui::COLOR_DANGER
                     : (days <= 2) ? Ui::COLOR_DANGER
                                   : Ui::COLOR_MUTED;
        g.setTextDatum(TR_DATUM);
        g.setTextColor(col, Ui::COLOR_BG);
        g.drawString(txt, ctx.x + ctx.w - 8, y, 1);
    }

    // Manual early-reset credits, mirrored on the opposite corner from the
    // token badge. Not every account has these - only show it when the
    // response actually carried the field.
    if (resetCreditsAvailable_ >= 0) {
        g.setTextDatum(TL_DATUM);
        g.setTextColor(Ui::COLOR_MUTED, Ui::COLOR_BG);
        g.drawString(String(resetCreditsAvailable_) + " resets", ctx.x + 8, y, 1);
    }

    // Primary and secondary windows get the same treatment as Claude's two
    // windows; the per-model extras are compact rows underneath.
    int rows = (primary_.valid ? 1 : 0) + (secondary_.valid ? 1 : 0);
    int extrasH = extraCount_ > 0 ? (extraCount_ * 14 + 6) : 0;
    int bodyH   = ctx.h - 10 - extrasH;
    int rowH    = rows > 0 ? bodyH / rows : bodyH;
    y += 8;

    const Limit* main[2] = { &primary_, &secondary_ };
    for (int i = 0; i < 2; i++) {
        const Limit& l = *main[i];
        if (!l.valid) continue;
        Ui::Pace p = Ui::pace(l.usedPercent, l.resetsAt, l.windowSeconds, ctx.now);
        String title = l.name.isEmpty() ? windowLabel(l.windowSeconds) : l.name;
        Ui::windowRow(g, ctx.x, y, ctx.w, rowH - 4, title, l.usedPercent,
                      l.resetsAt, ctx.now, p, true);
        y += rowH;
    }

    if (extraCount_ > 0) {
        g.drawFastHLine(8, y - 2, ctx.w - 16, Ui::COLOR_DIVIDER);
        for (int i = 0; i < extraCount_; i++) {
            const Limit& l = extras_[i];
            String name = l.name.isEmpty() ? windowLabel(l.windowSeconds) : l.name;

            g.setTextDatum(TL_DATUM);
            g.setTextColor(Ui::COLOR_MUTED, Ui::COLOR_BG);
            g.drawString(Ui::fit(g, name, 90, 1), ctx.x + 8, y + 3, 1);

            int barX = ctx.x + 104;
            int barW = ctx.w - 104 - 46;
            if (barW > 20) {
                Ui::Pace p = Ui::pace(l.usedPercent, l.resetsAt, l.windowSeconds, ctx.now);
                Ui::bar(g, barX, y + 3, barW, 7, l.usedPercent,
                        p.color == Ui::COLOR_MUTED ? Ui::COLOR_ACCENT : p.color);
            }
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", (int)(l.usedPercent + 0.5));
            g.setTextDatum(TR_DATUM);
            g.setTextColor(Ui::COLOR_TEXT, Ui::COLOR_BG);
            g.drawString(pct, ctx.x + ctx.w - 8, y + 1, 1);

            y += 14;
        }
    }
}

String OpenAiProvider::formHtml() const {
    String h;
    h.reserve(1536);

    h += "<label>Access token";
    h += "<input type=password name=p_accessToken autocomplete=off placeholder=\"";
    h += accessToken_.isEmpty() ? String("eyJhbGciOi...") : maskCredential(accessToken_);
    h += "\"></label>";
    h += "<p class=hint>From <code>~/.codex/auth.json</code>, field "
         "<code>tokens.access_token</code>. Leave blank to keep the stored one.</p>";

    h += "<label>ChatGPT account id";
    h += "<input name=p_accountId value=\"" + htmlEscape(accountId_) + "\" placeholder=\"from the token\"></label>";
    h += "<p class=hint>Leave empty to use the account the token belongs to. Set it to "
         "watch a second workspace under the same login.</p>";

    h += "<label>Plan badge";
    h += "<input name=p_planType value=\"" + htmlEscape(planType_) + "\" placeholder=\"plus / pro / business\"></label>";

    h += "<label>Refresh token (optional)";
    h += "<input type=password name=p_refreshToken autocomplete=off placeholder=\"";
    h += refreshToken_.isEmpty() ? String("not set") : maskCredential(refreshToken_);
    h += "\"></label>";

    h += "<label class=check><input type=checkbox name=p_autoRefresh value=1";
    h += autoRefresh_ ? " checked" : "";
    h += "> Renew the access token automatically</label>";
    h += "<p class=hint><b>Careful:</b> refresh tokens rotate. If this device renews the "
         "token, the Codex CLI on your computer may be signed out of that account and need "
         "<code>codex login</code> again. Left off, the token lasts about 10 days and the "
         "screen warns you before it expires.</p>";
    return h;
}

bool OpenAiProvider::applyForm(const FormSource& form, String& err) {
    String token = form.get("p_accessToken");
    token.trim();
    if (!token.isEmpty()) accessToken_ = token;

    String refresh = form.get("p_refreshToken");
    refresh.trim();
    if (!refresh.isEmpty()) refreshToken_ = refresh;

    String account = form.get("p_accountId");
    account.trim();
    if (account != accountId_) {
        accountId_  = account;
        primary_    = Limit{};
        secondary_  = Limit{};
        extraCount_ = 0;
    }

    planType_ = form.get("p_planType");
    planType_.trim();
    autoRefresh_ = form.has("p_autoRefresh");

    if (accessToken_.isEmpty()) {
        err = "An access token is required.";
        return false;
    }

    readClaims();
    if (!claims_.valid) {
        err = "That does not look like a Codex access token.";
        return false;
    }

    setNeedsAuth(false);
    setError("");
    return true;
}
