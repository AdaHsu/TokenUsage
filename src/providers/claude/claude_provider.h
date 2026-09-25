#pragma once
#include "../../core/provider.h"

// Reads subscription usage straight from claude.ai.
//
// One instance is one (sessionKey, orgId) pair. Both axes are free to grow:
// several separate Claude logins each get their own sessionKey, and a single
// login that owns a personal org plus one or more Teams orgs gets one
// instance per org sharing that key.
class ClaudeProvider : public Provider {
public:
    const char* typeId()   const override { return "claude"; }
    const char* typeName() const override { return "Claude"; }
    uint16_t    accent()   const override;
    String      subLabel() const override { return kind_; }

    FetchResult refresh(time_t now) override;
    bool        hasData() const override { return usage_.valid; }
    void        show(const RenderCtx& ctx) override;

    void   loadSettings(const String& json) override;
    String saveSettings() const override;
    String formHtml() const override;
    bool   applyForm(const FormSource& form, String& err) override;

    bool supportsDiscovery() const override { return true; }
    bool discover(std::vector<Workspace>& out, String& err) override;
    void selectWorkspace(const Workspace& w) override;

    String credentialId()   const override;
    String credentialHint() const override;
    String credentialSecret() const override { return sessionKey_; }
    void   setCredential(const String& cred) override;

private:
    struct Window {
        bool   valid       = false;
        double utilization = 0;    // 0..100
        time_t resetsAt    = 0;    // UTC epoch
    };
    struct Usage {
        bool   valid = false;
        Window fiveHour;
        Window sevenDay;
    };

    FetchResult fetchOrgId();
    void        drawWindows(const RenderCtx& ctx);

    String sessionKey_;
    String orgId_;
    String orgName_;
    String kind_;      // PRO / MAX / TEAMS - best effort, from org capabilities
    Usage  usage_;
};
