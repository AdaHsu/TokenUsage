#include "provider.h"

String maskCredential(const String& secret) {
    if (secret.isEmpty()) return String();
    const int tail = 4;
    if ((int)secret.length() <= tail + 2) return String("...");

    // Keep a recognisable prefix when the secret has one, so the user can
    // tell a Claude cookie from a ChatGPT token at a glance.
    String head;
    int dash = secret.indexOf('-', 3);
    if (dash > 0 && dash <= 10) head = secret.substring(0, dash + 1);
    else                        head = secret.substring(0, 4);

    return head + "..." + secret.substring(secret.length() - tail);
}

String htmlEscape(const String& s) {
    String out;
    out.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}
