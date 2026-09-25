#include "widgets.h"
#include "../util/timefmt.h"

namespace {
double clamp01(double v) {
    if (v < 0) return 0;
    if (v > 1) return 1;
    return v;
}
}  // namespace

Ui::Pace Ui::pace(double utilPercent, time_t resetsAt, long windowSeconds, time_t now) {
    Pace p;
    p.used = clamp01(utilPercent / 100.0);

    if (resetsAt > 0 && now > 0 && windowSeconds > 0) {
        long remaining = (long)(resetsAt - now);
        if (remaining < 0) remaining = 0;
        if (remaining > windowSeconds) remaining = windowSeconds;
        p.elapsed = clamp01((double)(windowSeconds - remaining) / (double)windowSeconds);
    }

    // Right at the start of a window the elapsed fraction is near zero and the
    // ratio explodes; treat that as "no verdict yet" rather than "burning fast".
    if (p.elapsed < 0.02) {
        p.ratio = 0;
        p.label = "";
        p.color = COLOR_MUTED;
        return p;
    }

    p.ratio = p.used / p.elapsed;
    // Same thresholds as the desktop app so the two never disagree.
    if      (p.ratio < 0.75) { p.label = "Well under pace"; p.color = COLOR_OK; }
    else if (p.ratio < 0.95) { p.label = "Under pace";      p.color = COLOR_MINT; }
    else if (p.ratio < 1.10) { p.label = "On pace";         p.color = COLOR_WARN; }
    else if (p.ratio < 1.35) { p.label = "Over pace";       p.color = COLOR_ORANGE; }
    else                     { p.label = "Burning fast";    p.color = COLOR_DANGER; }
    return p;
}

void Ui::bar(TFT_eSPI& g, int x, int y, int w, int h,
             double percent, uint16_t color, double markerPercent) {
    if (w <= 2 || h <= 0) return;
    int radius = h / 2;

    g.fillRoundRect(x, y, w, h, radius, COLOR_BAR_BG);

    double frac = clamp01(percent / 100.0);
    int fillW = (int)(w * frac + 0.5);
    if (fillW > 0) {
        // A rounded rect narrower than its own corner radius renders badly;
        // fall back to a plain rect for the first few pixels.
        if (fillW <= radius * 2) g.fillRect(x, y, fillW, h, color);
        else                     g.fillRoundRect(x, y, fillW, h, radius, color);
    }

    if (markerPercent >= 0) {
        int mx = x + (int)(w * clamp01(markerPercent / 100.0) + 0.5);
        if (mx > x && mx < x + w) {
            g.drawFastVLine(mx, y - 1, h + 2, COLOR_TEXT);
        }
    }
}

void Ui::windowRow(TFT_eSPI& g, int x, int y, int w, int h,
                   const String& title, double percent,
                   time_t resetsAt, time_t now,
                   const Pace& p, bool showPaceLabel) {
    const int pad = 8;
    int left  = x + pad;
    int right = x + w - pad;
    int innerW = right - left;
    if (innerW < 40) return;

    // Title on the left, percentage on the right of the same line.
    g.setTextDatum(TL_DATUM);
    g.setTextColor(COLOR_MUTED, COLOR_BG);
    g.drawString(Ui::fit(g, title, innerW - 70, 2), left, y, 2);

    char pctBuf[12];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(percent + 0.5));
    g.setTextDatum(TR_DATUM);
    g.setTextColor(p.color == COLOR_MUTED ? COLOR_TEXT : p.color, COLOR_BG);
    g.drawString(pctBuf, right, y, 4);

    // Compact metrics when the caller could only spare a short strip, so the
    // reset countdown still fits instead of being dropped.
    const bool roomy = (h >= 56);
    int barY = y + (roomy ? 24 : 19);
    int barH = (h >= 60) ? 12 : 8;
    Ui::bar(g, left, barY, innerW, barH, percent,
            p.color == COLOR_MUTED ? COLOR_ACCENT : p.color,
            p.elapsed > 0 ? p.elapsed * 100.0 : -1);

    int footY = barY + barH + (roomy ? 4 : 2);
    if (footY + (roomy ? 12 : 10) <= y + h) {
        g.setTextDatum(TL_DATUM);
        g.setTextColor(COLOR_MUTED, COLOR_BG);
        if (showPaceLabel && p.label[0] != '\0') {
            g.drawString(p.label, left, footY, 2);
        }
        String reset = TimeFmt::countdown(resetsAt, now);
        if (reset != "--") {
            g.setTextDatum(TR_DATUM);
            g.drawString("resets " + reset, right, footY, 2);
        }
    }
}

int Ui::badgeWidth(TFT_eSPI& g, const String& text) {
    return g.textWidth(text, 1) + 8;
}

int Ui::badge(TFT_eSPI& g, int x, int y, const String& text, uint16_t color) {
    if (text.isEmpty()) return 0;
    int w = badgeWidth(g, text);
    int h = 13;
    g.drawRoundRect(x, y, w, h, 3, color);
    g.setTextDatum(TL_DATUM);
    g.setTextColor(color, COLOR_BG);
    g.drawString(text, x + 4, y + 3, 1);
    return w;
}

void Ui::centeredMessage(TFT_eSPI& g, int x, int y, int w, int h,
                         const String& msg, uint16_t color) {
    g.setTextDatum(MC_DATUM);
    g.setTextColor(color, COLOR_BG);
    g.drawString(Ui::fit(g, msg, w - 12, 2), x + w / 2, y + h / 2, 2);
}

String Ui::fit(TFT_eSPI& g, const String& text, int maxWidth, uint8_t font) {
    if (maxWidth <= 0) return String();
    if (g.textWidth(text, font) <= maxWidth) return text;

    String out = text;
    while (out.length() > 1 && g.textWidth(out + "...", font) > maxWidth) {
        out.remove(out.length() - 1);
    }
    return out + "...";
}
