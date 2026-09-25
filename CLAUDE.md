# CLAUDE.md

Notes for future sessions working on this repo.

## What this is

ESP32-S3 firmware for a LilyGo T-Display S3 that shows subscription usage for
several AI accounts at once. It talks to the vendors directly over HTTPS; there
is no companion app. Structure and board handling are modelled on
`github.com/alberthorta/ClaudeStatsPortable` (MIT), but the core is a rewrite
around a pluggable provider layer.

## Board

- LilyGo T-Display S3, ESP32-S3, 16 MB flash, 8 MB PSRAM.
- ST7789 170x320 parallel 8-bit; the TFT_eSPI pin map lives in `platformio.ini`.
- KEY on GPIO14, BOOT on GPIO0 (both active low).
- **GPIO15 = LCD_POWER_ON must be driven HIGH** or the panel is dark on
  battery while the buttons keep working. Handled in `Display::begin()`.
- Battery on GPIO4 through a 2:1 divider: `analogReadMilliVolts(4) * 2`.
- Uses the stock `default_16MB.csv` partition table, unmodified. A custom
  table that enlarged NVS was tried and tested reliably in a boot loop on
  real hardware (structurally valid partition binary, checksum and all - the
  fault was never actually pinned down) - do not reintroduce one without
  flashing real hardware to prove it boots. Account credentials go on
  LittleFS instead, which sidesteps the whole problem: see Storage below.

## Build

```bash
pio run                       # compile
pio run -t upload             # flash
pio device monitor -b 115200  # logs
```

## The one design rule

**A provider module owns its data, its screen and its settings form.** The
shell hands it a rectangle and a clock and knows nothing else about it.
`src/core/provider.h` is the whole contract. Adding a vendor is a directory
under `src/providers` plus one line in `registry.cpp` - if a change wants to
touch `main.cpp`, `screens.cpp` or `webconfig.cpp` to support a specific
vendor, the abstraction is being bypassed.

Corollaries worth keeping:

- Vendor-specific warnings (a token expiring, a re-auth prompt) are drawn by
  the module inside `show()`. They do not go on the Info screen, which is
  device-level only and deliberately short.
- The shell draws the identity header (dot, vendor, label, badge, `3/6`) so
  every vendor looks consistent. Modules should not repeat it.
- `webconfig.cpp` renders `formHtml()` and feeds `applyForm()`. It never
  learns what a sessionKey or an access token is.

## Fetch gates (`core/carousel.cpp`)

Only `providers[activeIndex]` is ever refreshed. A switch does not fetch:

1. `SWITCH_SETTLE_MS` (5 s) - restarted by every press, so a burst through the
   list sends nothing.
2. `SWITCH_MIN_AGE_MS` (3 min) - after settling, only refetch if the cache is
   older than this.

Outside a switch the cadence is `refreshMin` from device settings (default 5,
range 1-60). A module may ask for something slower via `refreshIntervalMs()`;
it can never ask for faster. `dueForFetch()` mutates switch state, so call it
exactly once per tick.

## Buttons (`main.cpp`)

KEY presses accumulate into a burst that closes `DOUBLE_CLICK_MS` (280 ms)
after the last press: two presses toggle Stats/Info, anything else advances
that many accounts. Do not "fix" the 280 ms latency by acting on the first
press - that is what makes one button carry both actions, and it is why fast
repeated presses skip accounts instead of flipping views.

Sleep tiers follow the upstream design: BOOT 2 s backlight off, idle light
sleep (timer wake refreshes the visible account silently, button wake
restarts because peripherals come back half-initialised), BOOT+KEY 5 s deep
sleep. `keyWakeOnly` / `bootWakeOnly` swallow the press that wakes the screen.

## Data sources

```
GET https://claude.ai/api/organizations               Cookie: sessionKey=...
GET https://claude.ai/api/organizations/{org}/usage   -> five_hour, seven_day
GET https://chatgpt.com/backend-api/wham/usage        Authorization: Bearer ...
                                                      ChatGPT-Account-Id, originator
POST https://auth.openai.com/oauth/token              refresh_token grant
```

Both usage endpoints are private APIs. Parse defensively: a missing field
degrades the screen, it does not fail the fetch. Before changing a parser,
confirm the shape with curl from a computer rather than guessing.

`refresh_token` rotates. Automatic renewal is opt-in per account and off by
default because the device winning a rotation signs the desktop Codex CLI out.

## Storage (`core/account.cpp`, `core/settings.cpp`)

Two backends, split by size:

- **Accounts live on LittleFS** as a single JSON array at `/accounts.json`
  (`Accounts::begin()` mounts it, formatting on first boot - call before
  anything touches the account list). A Codex access token alone is 1-2 KB;
  a dozen of those plus Claude session keys would crowd the stock ~20 KB nvs
  partition and hit NVS's own per-value size ceiling besides. `settings` is
  a real JSON sub-object on disk, round-tripped to/from the opaque string the
  `Provider` interface uses - still never parsed outside the module that
  wrote it.
- **Everything small stays in NVS**, namespace `tokenusage`: WiFi (`w<N>s/p`
  plus `wN`), device settings, and the one-int `actIdx` (active account).

## What not to do

- Do not add a companion app, bridge or local JSONL parsing.
- Do not put vendor details in the shell, or device details in a module.
- Do not fetch for accounts that are not on screen, including "just to warm
  the cache" on the Info screen.
- Do not show a stored credential in full anywhere in the web UI.
- Do not load a CDN stylesheet or font: the captive portal runs with no
  internet at all.
- Do not parse timestamps as local time. TZ is pinned to UTC0 at boot.
