#pragma once
#include "../../core/provider.h"
#include "../../util/jwt.h"
#include <ArduinoJson.h>

// Reads the ChatGPT subscription allowance that Codex runs against, using the
// access token the Codex CLI stores in ~/.codex/auth.json.
//
// One instance is one (access token, ChatGPT account id) pair, mirroring the
// Claude module: separate logins each bring their own token, while one login
// covering a personal and a business workspace reuses the token and only
// changes the account id.
//
// The endpoint is not a documented API and can change without warning, so
// every field is read defensively and a missing one degrades the screen
// rather than breaking the fetch.
class OpenAiProvider : public Provider {
public:
    const char* typeId()   const override { return "openai"; }
    const char* typeName() const override { return "Codex"; }
    uint16_t    accent()   const override;
    String      subLabel() const override;

    FetchResult refresh(time_t now) override;
    bool        hasData() const override { return primary_.valid || secondary_.valid; }
    void        show(const RenderCtx& ctx) override;

    void   loadSettings(const String& json) override;
    String saveSettings() const override;
    String formHtml() const override;
    bool   applyForm(const FormSource& form, String& err) override;

    String credentialId()   const override;
    String credentialHint() const override;
    String credentialSecret() const override { return accessToken_; }
    void   setCredential(const String& cred) override;

private:
    struct Limit {
        bool   valid         = false;
        double usedPercent   = 0;
        long   windowSeconds = 0;
        time_t resetsAt      = 0;
        String name;          // only set for the per-model extras
    };

    FetchResult fetchUsage(time_t now);
    bool        tryRefreshToken();     // OAuth refresh, opt-in
    void        readClaims();          // pull account id / expiry out of the JWT
    int         tokenDaysLeft(time_t now) const;
    static bool parseLimit(JsonVariantConst v, Limit& out, time_t now);

    String accessToken_;
    String refreshToken_;
    String accountId_;      // overrides whatever the token claims
    String planType_;
    bool   autoRefresh_ = false;

    Jwt::Claims claims_;
    Limit       primary_;
    Limit       secondary_;
    Limit       extras_[3];
    int         extraCount_ = 0;

    // rate_limit_reset_credits.available_count - how many manual early
    // resets are left. -1 means the field wasn't in the response.
    int resetCreditsAvailable_ = -1;
};
