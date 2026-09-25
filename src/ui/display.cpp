#include "display.h"
#include "../hal/board.h"

namespace {
TFT_eSPI    tft;
TFT_eSprite sprite       = TFT_eSprite(&tft);
bool        spriteReady  = false;
bool        spriteFailed = false;  // do not retry a doomed allocation every frame
int         currentRot   = 1;
bool        blOn         = true;

void destroySprite() {
    if (spriteReady) sprite.deleteSprite();
    spriteReady = false;
}

void createSprite() {
    destroySprite();
    if (spriteFailed) return;

    sprite.setColorDepth(16);
    sprite.createSprite(tft.width(), tft.height());
    spriteReady = sprite.created();
    if (!spriteReady) {
        // Out of RAM for a full-screen buffer: fall back to drawing straight
        // to the panel. It flickers, but the device stays usable.
        spriteFailed = true;
        Serial.println("[display] sprite allocation failed, drawing direct");
    }
}
}  // namespace

void Display::begin() {
    // GPIO15 is the LCD power rail. USB feeds it directly, but on battery the
    // MCU has to drive it or the panel stays dark while the buttons still work.
    pinMode(Board::PIN_LCD_POWER, OUTPUT);
    digitalWrite(Board::PIN_LCD_POWER, HIGH);

    tft.init();
    currentRot = 1;
    tft.setRotation(currentRot);
    tft.fillScreen(Ui::COLOR_BG);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    blOn = true;

    createSprite();
}

void Display::setRotation(int r) {
    currentRot = r & 3;
    tft.setRotation(currentRot);
    tft.fillScreen(Ui::COLOR_BG);
    createSprite();
}

int Display::rotation() { return currentRot; }

void Display::setBacklight(bool on) {
    blOn = on;
    digitalWrite(TFT_BL, on ? HIGH : LOW);
}

bool Display::backlightOn() { return blOn; }

void Display::panelSleep(bool in) {
    // 0x10 = SLPIN, 0x11 = SLPOUT. The datasheet wants 5 ms before the next
    // command either way.
    tft.writecommand(in ? 0x10 : 0x11);
    delay(5);
}

int  Display::width()     { return tft.width(); }
int  Display::height()    { return tft.height(); }
bool Display::landscape() { return tft.width() > tft.height(); }

TFT_eSPI& Display::canvas() {
    return spriteReady ? (TFT_eSPI&)sprite : tft;
}

void Display::beginFrame(uint16_t color) {
    if (!spriteReady) createSprite();
    if (spriteReady) sprite.fillSprite(color);
    else             tft.fillScreen(color);
}

void Display::endFrame() {
    if (spriteReady) sprite.pushSprite(0, 0);
}

TFT_eSPI& Display::beginDirect() {
    destroySprite();
    digitalWrite(TFT_BL, HIGH);  // these screens must be visible even from tier 1
    blOn = true;
    tft.fillScreen(Ui::COLOR_BG);
    return tft;
}

void Display::endDirect() {
    createSprite();
}
