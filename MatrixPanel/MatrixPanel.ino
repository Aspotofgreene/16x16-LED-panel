// Matrix Panel firmware for a 16x16 WS2812B matrix on an ESP32-S3.
// Effects, clock, scrolling text and pixel art, controlled from a phone web page.
//
// First boot: join the Wi-Fi hotspot "Matrix-Panel" on your phone, the control
// page opens automatically (or browse to http://192.168.4.1). Enter your home
// Wi-Fi under Settings. After that, open http://matrix.local (or the IP address
// that scrolls across the panel at startup).

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <time.h>
#include "webpage.h"

// ---- Hardware ---------------------------------------------------------------
#define LED_PIN        4      // matrix DIN
#define W              16
#define H              16
#define NUM_LEDS       (W * H)
// Estimated current cap for the LEDs. A full-white 16x16 panel can pull ~15 A,
// far more than a USB-C supply gives. 2000 mA is safe for a 3 A supply.
#define POWER_LIMIT_MA 2000

// Small boards with chip antennas often fail to broadcast at full power.
#define TX_POWER       WIFI_POWER_8_5dBm
#define AP_NAME        "Matrix-Panel"
#define HOSTNAME       "matrix"
#define IMG_SLOTS      8
#define IMG_BYTES      (NUM_LEDS * 3)
#define LIVE_SLOT      255

enum Mode { MODE_EFFECT, MODE_CLOCK, MODE_TEXT, MODE_IMAGE };
enum Effect {
  FX_SOLID, FX_RAINBOW, FX_PLASMA, FX_FIRE, FX_MATRIX, FX_TWINKLE, FX_LIFE,
  FX_AURORA, FX_LAVA, FX_OCEAN, FX_SUNSET, FX_BREATHE,
  FX_STARS, FX_FIREWORKS, FX_RAIN, FX_BOUNCE, FX_SPIRAL, FX_RIPPLES, FX_SNAKE,
  FX_HALLOWEEN, FX_SNOW, FX_CHASE, FX_HEARTS,
  FX_GARDEN, FX_ROCKET, FX_FISH, FX_DAY, FX_CAT, FX_SNOWMAN,  // stories (keep together)
  FX_DREAM,
  FX_HATCH, FX_UFO, FX_SHOWER, FX_VOLCANO, FX_FETCH,  // more stories, added after Dream to keep saved numbers
  FX_RACE, FX_BRICKS,  // retro arcade stories
  FX_PUNCH, FX_WEB, FX_IRON, FX_SKY, FX_BAT, FX_SHIELD, FX_SMASH, FX_SPARK, FX_ANT, FX_THUNDER, FX_ARROW, FX_BARK,  // heroes
  FX_CLOUDS,
  FX_FIREFLIES, FX_KOI, FX_LEAVES, FX_CANDLE, FX_CAMPFIRE, FX_RAINYWIN, FX_MOONRISE, FX_JELLY,
  FX_INK, FX_TIDEPOOL, FX_KALEIDO, FX_GALAXY, FX_SANDGARDEN, FX_HOURGLASS, FX_BREATHEGUIDE,  // more calm
  FX_COUNT
};

// Playlist entries: effect numbers, or one of these.
#define PL_CLOCK       200
#define PL_TEXT        201
#define PL_IMG         210    // + picture slot
#define MAX_PL         64

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
DNSServer dns;
Preferences prefs;

struct Config {
  bool on;
  uint8_t mode, effect, brightness, speed, slot;
  uint32_t color, textColor;
  bool rainbowText, autoCycle, clock24, shuffle;  // autoCycle = playlist playing
  uint16_t cycleSecs;
  String text, tz;
  uint8_t corner;          // where pixel 0 sits: 0 TL, 1 TR, 2 BL, 3 BR
  uint8_t rotate;          // turns the picture clockwise in 90-degree steps (0-3)
  bool vertical, serpentine;
} cfg;

uint32_t fb[H][W];           // frame buffer, 0xRRGGBB, (0,0) = top-left
uint8_t imgBuf[IMG_BYTES];
bool apMode = false;
uint32_t saveAt = 0;
String notice;               // one-off message scrolled over everything (IP etc.)
int noticeRepeats = 0;

float frameDt = 0, tsec = 0;
float textX = W, noticeX = W;
uint32_t lastFrame = 0, lastCycle = 0;
uint8_t lastMode = 255, lastEffect = 255;
bool fxFresh = true;         // true on the first frame of an effect
uint8_t fadeLevel = 255;     // dims the whole panel (kept at full; transitions blend instead)
// Transitions: when the picture changes, the last frame shown melts into the
// new one with a randomly chosen style (crossfade, dissolve, sweep or ripple).
#define TRANS_MS       1600
uint32_t shownFb[H][W];      // what is on the LEDs right now
uint32_t prevFb[H][W];       // the frame we are transitioning away from
uint32_t liveFb[H][W];       // the new item's own frame, kept clean for the effect
uint8_t transN[H][W];        // per-pixel delay for the current transition style
uint32_t transAt = 0;
uint8_t lastSlot = 255;

uint8_t playlist[MAX_PL];
int plLen = 0, plPos = -1;
uint8_t bag[MAX_PL];         // shuffle order: every item plays once before any repeats
int bagLen = 0, bagPos = 0;
bool textPassDone = false;

// ---- Pixel helpers ----------------------------------------------------------
uint16_t ledIndex(int x, int y) {
  for (int r = 0; r < cfg.rotate; r++) {
    int t = x;
    x = W - 1 - y;
    y = t;
  }
  if (cfg.corner & 1) x = W - 1 - x;
  if (cfg.corner & 2) y = H - 1 - y;
  int major = cfg.vertical ? x : y;
  int minor = cfg.vertical ? y : x;
  if (cfg.serpentine && (major & 1)) minor = W - 1 - minor;
  return major * W + minor;
}

inline void setPx(int x, int y, uint32_t c) {
  if (x >= 0 && y >= 0 && x < W && y < H) fb[y][x] = c;
}

void fill(uint32_t c) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = c;
}

uint32_t scale(uint32_t c, uint8_t r, uint8_t g, uint8_t b) {
  return (((c >> 16 & 0xFF) * r >> 8) << 16) | (((c >> 8 & 0xFF) * g >> 8) << 8) | ((c & 0xFF) * b >> 8);
}

void fadeAll(uint8_t r, uint8_t g, uint8_t b) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = scale(fb[y][x], r, g, b);
}

uint32_t hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255) {
  return Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
}

// Push the frame buffer to the LEDs with brightness and the power limit applied.
void pushFrame() {
  uint32_t sum = 0;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t c = fb[y][x];
      sum += (c >> 16 & 0xFF) + (c >> 8 & 0xFF) + (c & 0xFF);
    }
  // Brightness times transition fade, out of 255 * 255.
  uint32_t bri = cfg.on ? cfg.brightness * fadeLevel : 0;
  // Each channel at full is roughly 20 mA.
  uint32_t ma = (uint64_t)sum * 20 * bri / (255UL * 255UL * 255UL);
  if (ma > POWER_LIMIT_MA) bri = bri * POWER_LIMIT_MA / ma;
  const uint32_t FULL = 255UL * 255UL;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t c = fb[y][x];
      strip.setPixelColor(ledIndex(x, y), (c >> 16 & 0xFF) * bri / FULL, (c >> 8 & 0xFF) * bri / FULL,
                          (c & 0xFF) * bri / FULL);
    }
  strip.show();
}

// Adafruit_GFX target that draws text into the frame buffer in full colour.
class FbGfx : public Adafruit_GFX {
 public:
  FbGfx() : Adafruit_GFX(W, H) {}
  uint32_t ink = 0xFFFFFF;
  bool rainbow = false;
  void drawPixel(int16_t x, int16_t y, uint16_t) override {
    setPx(x, y, rainbow ? hsv((uint16_t)(tsec * 6000) + x * 2500 + y * 800) : ink);
  }
} gfx;

// Advances a step counter for effects that move in discrete steps.
int ticks(float &acc, float perSec) {
  acc += frameDt * perSec * cfg.speed / 50.0f;
  int n = (int)acc;
  acc -= n;
  return min(n, 4);
}

// ---- Effects ----------------------------------------------------------------
void fxPlasma() {
  float t = tsec;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float dx = x - 7.5f, dy = y - 7.5f;
      float v = sinf(x * 0.35f + t) + sinf(y * 0.3f - t * 0.8f) + sinf((x + y) * 0.25f + t * 0.6f) +
                sinf(sqrtf(dx * dx + dy * dy) * 0.45f - t * 1.2f);
      fb[y][x] = hsv((uint16_t)((v + 4) * 8192));
    }
}

uint8_t heat[W][H];  // y = 0 is the bottom row
uint32_t heatColor(uint8_t t) {
  uint8_t t192 = t * 191 / 255, ramp = (t192 & 63) << 2;
  if (t192 >= 128) return 0xFFFF00 | ramp;
  if (t192 >= 64) return 0xFF0000 | (ramp << 8);
  return (uint32_t)ramp << 16;
}

void fxFire() {
  static float acc;
  for (int n = ticks(acc, 25); n > 0; n--) {
    for (int x = 0; x < W; x++)
      for (int y = 0; y < H; y++) heat[x][y] = max(0, heat[x][y] - (int)random(0, 30));
    uint8_t next[W][H];
    memcpy(next, heat, sizeof(heat));
    for (int x = 0; x < W; x++)
      for (int y = H - 1; y >= 2; y--) {
        int l = heat[max(x - 1, 0)][y - 1], r = heat[min(x + 1, W - 1)][y - 1];
        next[x][y] = (heat[x][y - 1] * 2 + l + r + heat[x][y - 2] * 2) / 6;
      }
    memcpy(heat, next, sizeof(heat));
    for (int x = 0; x < W; x++)
      if (random(255) < 170) heat[x][random(2)] = min(255, heat[x][0] + (int)random(160, 255));
  }
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++) fb[H - 1 - y][x] = heatColor(heat[x][y]);
}

int8_t drops[W];
void fxMatrix() {
  static float acc;
  for (int n = ticks(acc, 12); n > 0; n--) {
    fadeAll(120, 200, 120);
    for (int x = 0; x < W; x++) {
      if (drops[x] >= 0) {
        setPx(x, drops[x] - 1, 0x00C828);
        setPx(x, drops[x], 0xB4FFB4);
        if (++drops[x] > H + 2) drops[x] = -1;
      } else if (random(100) < 6) {
        drops[x] = 0;
      }
    }
  }
}

void fxTwinkle() {
  static float acc;
  for (int n = ticks(acc, 20); n > 0; n--) {
    fadeAll(225, 225, 225);
    for (int k = 0; k < 2; k++) setPx(random(W), random(H), hsv(random(65536), 200));
  }
}

// ---- More helpers -----------------------------------------------------------
#define CLEAR 0xFFFFFFFF

inline uint16_t hueOf(float v) { return (uint16_t)(int32_t)v; }
inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return (uint32_t)r << 16 | g << 8 | b; }
inline float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
// Repeatable pseudo-random 0..1 for fixed scenery.
float fwHash(uint32_t i) {
  i = (i ^ 61) ^ (i >> 16);
  i *= 9;
  i ^= i >> 4;
  i *= 0x27d4eb2d;
  i ^= i >> 15;
  return (i & 0xFFFF) / 65535.0f;
}
inline float pmod(float a, float m) {
  float r = fmodf(a, m);
  return r < 0 ? r + m : r;
}
inline float sdt() { return frameDt * cfg.speed / 50.0f; }  // speed-scaled seconds this frame

uint32_t lerpColor(uint32_t a, uint32_t b, float f) {
  f = constrain(f, 0.0f, 1.0f);
  int ar = a >> 16 & 0xFF, ag = a >> 8 & 0xFF, ab = a & 0xFF;
  return rgb(ar + ((int)(b >> 16 & 0xFF) - ar) * f, ag + ((int)(b >> 8 & 0xFF) - ag) * f,
             ab + ((int)(b & 0xFF) - ab) * f);
}

// Draws over the pixel, keeping the brighter value of each channel.
void addPx(int x, int y, uint32_t c) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  uint32_t o = fb[y][x];
  fb[y][x] = max(o & 0xFF0000, c & 0xFF0000) | max(o & 0xFF00, c & 0xFF00) | max(o & 0xFF, c & 0xFF);
}

// Adds light to a pixel without going past full brightness.
void fbAdd(int x, int y, uint32_t c) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  uint32_t o = fb[y][x];
  fb[y][x] = rgb(min(255, (int)(o >> 16 & 255) + (int)(c >> 16 & 255)), min(255, (int)(o >> 8 & 255) + (int)(c >> 8 & 255)),
                 min(255, (int)(o & 255) + (int)(c & 255)));
}
// Adds light at a sub-pixel position, shared over the four nearest pixels.
void addAA(float x, float y, uint32_t c) {
  int x0 = floorf(x), y0 = floorf(y);
  float fx = x - x0, fy = y - y0;
  uint8_t w[4] = {(uint8_t)((1 - fx) * (1 - fy) * 255), (uint8_t)(fx * (1 - fy) * 255), (uint8_t)((1 - fx) * fy * 255), (uint8_t)(fx * fy * 255)};
  for (int i = 0; i < 4; i++) fbAdd(x0 + (i & 1), y0 + (i >> 1), scale(c, w[i], w[i], w[i]));
}


// Sprites are rows of characters; 'X' uses spriteInk, '.' is see-through.
uint32_t spriteInk, candle, eyeWhite, pupil;
uint32_t spritePal(char c) {
  switch (c) {
    case 'X': return spriteInk;
    case 'O': return 0xFF4800;
    case 'G': return 0x208000;
    case 'K': return candle;
    case 'W': return 0xA0B4FF;
    case 'B': return 0x000060;
    case 'M': return 0x100020;
    case 'E': return eyeWhite;
    case 'P': return pupil;
    default:  return CLEAR;
  }
}

void drawSprite(const char *const *rows, int n, int ox, int oy) {
  for (int y = 0; y < n; y++)
    for (int x = 0; rows[y][x]; x++) {
      uint32_t c = spritePal(rows[y][x]);
      if (c != CLEAR) setPx(ox + x, oy + y, c);
    }
}

// ---- Calm effects -----------------------------------------------------------
// Silk: wavy colour bands that slowly turn, with a glossy shine sweeping across.
// Instead of every hue (like Plasma) it uses one colour family at a time,
// moving on to the next family every 14 seconds or so.
void fxRainbow() {
  static const uint32_t THEMES[][4] = {
      {0xFF1040, 0xFF7000, 0xFFD020, 0xC00070},  // sunset: red, orange, gold, magenta
      {0x0030FF, 0x00B0FF, 0x30FFC0, 0x3000B0},  // ocean: blue, sky, aqua, indigo
      {0xFF3090, 0xFFA0C0, 0x9040FF, 0x40D0FF},  // candy: pink, blush, purple, ice blue
      {0x00E030, 0xB0FF00, 0x007850, 0x30FFA0},  // forest: green, lime, pine, mint
      {0xFF2000, 0xFF9000, 0xFFF080, 0x900000},  // ember: red, amber, pale gold, dark red
  };
  const int N = 5;
  float t = tsec, a = t * 0.13f, ca = cosf(a), sa = sinf(a);
  float ft = t / 14, fr = ft - floorf(ft), mix = fr < 0.75f ? 0 : (fr - 0.75f) * 4;
  int i0 = (int)floorf(ft) % N, i1 = (i0 + 1) % N;
  mix = mix * mix * (3 - 2 * mix);
  uint32_t pal[4];
  for (int k = 0; k < 4; k++) pal[k] = lerpColor(THEMES[i0][k], THEMES[i1][k], mix);
  float shine = pmod(t * 0.35f, 6) * 8 - 12;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float px = x - 7.5f, py = y - 7.5f;
      float u = px * ca + py * sa, v = -px * sa + py * ca;
      float w = u + 1.6f * sinf(v * 0.45f + t * 1.1f) + 0.6f * sinf(v * 0.9f - t * 0.7f);
      float p = pmod(w * 0.2f + t * 0.55f, 4);  // position in the 4-colour loop
      int k = (int)p;
      float f = p - k;
      f = f * f * (3 - 2 * f);
      uint32_t c = Adafruit_NeoPixel::gamma32(lerpColor(pal[k & 3], pal[(k + 1) & 3], f));
      float q = (px + py) * 0.7071f - shine, s = expf(-q * q * 0.45f);
      float rip = 0.5f + 0.5f * sinf(w * 0.8f - t * 2);
      uint8_t lv = min(255.0f, 185 + 45 * rip + s * 25);
      fb[y][x] = lerpColor(scale(c, lv, lv, lv), 0xFFFFFF, s * 0.5f);
    }
}

// Game of Life with glowing cells: births bloom in and deaths leave fading trails.
uint8_t lifeAge[W][H], lifeTone[W][H];
float lifeGlow[W][H];
uint16_t lifeGen, lifeHue, lifeStale, lifeLow;
uint32_t lifeHist[6];
void lifeSeed() {
  bool mirror = random(2);
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++) lifeAge[x][y] = random(100) < 33;
  if (mirror)  // a mirrored start grows into symmetric patterns
    for (int x = 0; x < W / 2; x++)
      for (int y = 0; y < H; y++) lifeAge[W - 1 - x][y] = lifeAge[x][y];
  lifeGen = lifeStale = lifeLow = 0;
  lifeHue = random(65536);
  memset(lifeHist, 0, sizeof(lifeHist));
}

void fxLife() {
  static float acc;
  if (fxFresh) memset(lifeGlow, 0, sizeof(lifeGlow));
  for (int n = ticks(acc, 6); n > 0; n--) {
    uint8_t next[W][H];
    int pop = 0;
    for (int x = 0; x < W; x++)
      for (int y = 0; y < H; y++) {
        int nb = 0;
        for (int dx = -1; dx <= 1; dx++)
          for (int dy = -1; dy <= 1; dy++)
            if ((dx || dy) && lifeAge[(x + dx + W) % W][(y + dy + H) % H]) nb++;
        bool alive = lifeAge[x][y], live = alive ? (nb == 2 || nb == 3) : nb == 3;
        next[x][y] = live ? min(255, lifeAge[x][y] + 1) : 0;
        pop += live;
      }
    memcpy(lifeAge, next, sizeof(lifeAge));
    // Spot still lifes and short loops (blinkers etc.) and start again.
    uint32_t h = 2166136261u;
    for (int x = 0; x < W; x++)
      for (int y = 0; y < H; y++) h = (h ^ (lifeAge[x][y] > 0)) * 16777619u;
    bool seen = false;
    for (int i = 0; i < 6; i++) seen |= lifeHist[i] == h;
    for (int i = 5; i > 0; i--) lifeHist[i] = lifeHist[i - 1];
    lifeHist[0] = h;
    lifeStale = seen ? lifeStale + 1 : 0;
    lifeLow = pop < 10 ? lifeLow + 1 : 0;
    if (lifeStale > 18 || lifeLow > 40 || pop < 3 || ++lifeGen > 500) lifeSeed();
  }
  float dt = sdt();
  uint16_t base = lifeHue + (uint16_t)(tsec * 800);
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++) {
      float &g = lifeGlow[x][y];
      uint8_t age = lifeAge[x][y];
      if (age) {
        g = min(1.0f, g + dt * 7);
        lifeTone[x][y] = min((int)age, 30);
      } else {
        g = max(0.0f, g - dt * 1.6f);
      }
      if (g <= 0) fb[y][x] = 0;
      else if (age) fb[y][x] = hsv(base + lifeTone[x][y] * 900, age < 2 ? 110 : 245, g * 255);
      else fb[y][x] = hsv(base + lifeTone[x][y] * 900 + 9000, 255, g * g * 150);
    }
}

// Northern lights: two folding curtains of rays over a starry sky and dark hills.
void fxAurora() {
  float t = tsec;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t c = lerpColor(0x000002, 0x01030A, y / 15.0f), s = x * 31 + y * 7;
      if (y < 11 && fwHash(s) > 0.93f) {
        float tw = 0.5f + 0.5f * sinf(t * (1 + fwHash(s + 99) * 2) + fwHash(s + 7) * 6.28f);
        c = lerpColor(c, 0xFFFFFF, (0.08f + 0.18f * fwHash(s + 3)) * tw);
      }
      fb[y][x] = c;
    }
  for (int L = 0; L < 2; L++) {  // L = 1 is the fainter curtain further back
    float ph = L * 2.7f, base = L ? 5.5f : 8.8f, amp = L ? 0.7f : 1.0f;
    for (int x = 0; x < W; x++) {
      float edge = base + 1.8f * sinf(x * 0.28f + t * (L ? 0.28f : 0.35f) + ph) + 0.9f * sinf(x * 0.61f - t * 0.5f + ph * 2);
      float ray = 0.7f + 0.18f * sinf(x * 1.3f + t * 1.5f + ph) + 0.12f * sinf(x * 2.3f - t * 2.1f);
      float pulse = 0.6f + 0.4f * powf(0.5f + 0.5f * sinf(x * 0.35f - t * 1.2f + ph), 3);
      float len = 2.6f + 1.2f * sinf(x * 0.4f + t * 0.7f + ph);
      for (int y = 0; y < H; y++) {
        float d = edge - y;  // height above the curtain's bright lower edge
        float v = (d >= 0 ? expf(-d / len) * (d < 1 ? 1.25f : 1) : expf(-d * d * 2.2f)) * ray * pulse * amp;
        if (v < 0.03f) continue;
        float up = constrain(d / 5.0f, 0.0f, 1.0f);
        uint32_t col = L ? lerpColor(0x10E0A0, 0x6020C0, up) : lerpColor(0x30FF60, 0xA02090, up * up);
        uint8_t k = 255 * min(1.0f, v * v * 1.2f);
        fbAdd(x, y, scale(col, k, k, k));
      }
    }
  }
  for (int x = 0; x < W; x++) {  // hills in silhouette
    float hh = 13.2f + 1.1f * sinf(x * 0.5f + 1) + 0.5f * sinf(x * 1.3f);
    for (int y = 0; y < H; y++) {
      float cov = constrain(y + 1 - hh, 0.0f, 1.0f);
      if (cov > 0) fb[y][x] = lerpColor(fb[y][x], 0x000000, cov);
    }
  }
}

void fxLava() {
  float t = tsec * 0.6f, bx[5], by[5];
  for (int i = 0; i < 5; i++) {
    bx[i] = 7.5f + 5.5f * sinf(t * (0.3f + i * 0.13f) + i * 2.1f);
    by[i] = 7.5f + 6.5f * sinf(t * (0.23f + i * 0.17f) + i * 1.3f);
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float f = 0;
      for (int i = 0; i < 5; i++) {
        float dx = x - bx[i], dy = y - by[i];
        f += 6.0f / (dx * dx + dy * dy + 1);
      }
      fb[y][x] = f < 1 ? lerpColor(0x08000C, 0x400010, f * f) : hsv(hueOf(min(f - 1, 2.0f) * 4500));
    }
}

// The sea from the side: two rolling wave layers with foam, and sunbeams, bubbles and sand below.
float oceanFoam[W];
struct OBub { float x, y, v, ph; } obub[7];
void fxOcean() {
  float dt = sdt(), t = tsec;
  if (fxFresh) {
    for (auto &f : oceanFoam) f = 0;
    for (auto &b : obub) b.y = -9;
  }
  float front[W];
  for (int x = 0; x < W; x++) {
    front[x] = 7.4f + 1.5f * sinf(x * 0.42f - t * 1.4f) + 0.7f * sinf(x * 0.85f + t * 0.8f) + 0.35f * sinf(x * 1.9f - t * 2.2f);
    oceanFoam[x] *= expf(-dt * 1.4f);
    if (front[x] < 6.2f) oceanFoam[x] = max(oceanFoam[x], (6.2f - front[x]) * 1.3f);
  }
  for (int x = 0; x < W; x++) {
    float back = 5.3f + 0.8f * sinf(x * 0.55f - t * 1.1f + 1) + 0.4f * sinf(x * 1.2f + t * 0.9f), fr = front[x];
    for (int y = 0; y < H; y++) {
      uint32_t c = lerpColor(0x000104, 0x030A1C, y / 6.0f);  // sky
      c = lerpColor(c, 0x041A30, constrain(y + 1 - back, 0.0f, 1.0f));  // the wave behind
      float df = y - fr;
      uint32_t water = lerpColor(0x0E6488, 0x010514, df / 6.0f);
      float beam = powf(0.5f + 0.5f * sinf(x * 0.7f + y * 0.45f - t * 0.5f) * sinf(x * 0.31f - t * 0.23f + 1), 4);
      uint8_t bk = 255 * beam * expf(-max(0.0f, df) / 4.5f);
      water = lerpColor(water, 0x3A90A8, bk / 255.0f * 0.4f);
      if (y == 15) water = lerpColor(water, 0x1A1408, 0.6f + 0.25f * sinf(x * 1.3f + 2));  // sand
      c = lerpColor(c, water, constrain(y + 1 - fr, 0.0f, 1.0f));
      if (df > -1 && df < 1.2f) {  // a bright line along the surface, white where it foams
        float e = 1 - fabsf(df - 0.1f) / 1.1f;
        c = lerpColor(c, 0x2A88B0, e * 0.55f);
        c = lerpColor(c, 0xC8E4F0, min(1.0f, oceanFoam[x]) * e);
      }
      fb[y][x] = c;
    }
  }
  for (auto &b : obub) {  // bubbles rise and pop at the surface
    if (b.y < -5) {
      if (random(1000) < 30 * dt * 50) b = {rnd(1, 14), 15.5f, rnd(1.8f, 3.2f), rnd(0, 6.28f)};
      continue;
    }
    b.y -= b.v * dt;
    float bx = b.x + 0.4f * sinf(b.ph + t * 4);
    int ix = constrain((int)(bx + 0.5f), 0, W - 1);
    if (b.y < front[ix] + 0.8f) {
      b.y = -9;
      continue;
    }
    addAA(bx, b.y, 0x305868);
  }
}

void fxSunset() {
  float p = 0.5f - 0.5f * cosf(tsec * 0.12f);  // 0 = sun high, 1 = sun set
  uint32_t top = lerpColor(0x1050C0, 0x100030, p), horizon = lerpColor(0xFFD080, 0xFF2000, p);
  uint32_t sun = lerpColor(0xFFFFA0, 0xFF5000, p), sea = lerpColor(0x103060, 0x000818, p);
  float sunY = 2.5f + p * 12;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (y < 12) {
        uint32_t c = lerpColor(top, horizon, y / 11.0f);
        float dx = x - 7.5f, dy = y - sunY, d = sqrtf(dx * dx + dy * dy);
        fb[y][x] = d < 2.6f ? sun : lerpColor(c, sun, (4.5f - d) / 4.5f * 0.5f);
      } else {
        bool shimmer = fabsf(x - 7.5f) < 3 - (y - 12) * 0.4f && sinf(x * 3 + y * 5 + tsec * 6) > -0.2f;
        fb[y][x] = shimmer && sunY < 12 ? lerpColor(sea, sun, 0.55f) : sea;
      }
    }
}

void fxBreathe() {
  float v = 0.5f - 0.5f * cosf(tsec * 1.5f);
  uint8_t s = (0.04f + 0.96f * v * v) * 255;
  fill(scale(cfg.color, s, s, s));
}

// ---- Lively effects ---------------------------------------------------------
// Warp flight: stars streak past, with a faint nebula and the odd hyperspace jump.
struct Star { float x, y, z; uint32_t tint; } stars[36];
float starClock;
void starSpawn(int i, float z) {  // by index: Arduino can't prototype Star&
  static const uint32_t TINTS[] = {0xFFFFFF, 0xC0D8FF, 0xFFE8C0, 0xA0C0FF, 0xFFFFFF};
  stars[i] = {rnd(-1, 1), rnd(-1, 1), z, TINTS[random(5)]};
}
void fxStars() {
  float dt = sdt();
  if (fxFresh) {
    for (int i = 0; i < 36; i++) starSpawn(i, rnd(0.1f, 1));
    starClock = 0;
  }
  starClock += dt;
  float ph = fmodf(starClock, 18), boost = ph > 14 ? sinf((ph - 14) / 4 * PI) : 0;
  float v = 0.35f + 0.12f * sinf(starClock * 0.4f) + 2.2f * boost * boost;
  float cx = 7.5f + 1.5f * sinf(starClock * 0.21f), cy = 7.5f + 1.2f * sinf(starClock * 0.17f + 1);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float n = sinf(x * 0.35f + starClock * 0.15f) + sinf(y * 0.3f - starClock * 0.11f) + sinf((x - y) * 0.22f + starClock * 0.07f);
      uint32_t c = n > 0.6f ? hsv(48000 + n * 4000, 220, (n - 0.6f) * 9) : 0;
      fb[y][x] = boost > 0 ? lerpColor(c, 0x000A20, boost * 0.8f) : c;
    }
  for (int i = 0; i < 36; i++) {
    Star &s = stars[i];
    s.z -= v * dt;
    if (s.z <= 0.04f) {
      starSpawn(i, 1);
      continue;
    }
    float hx = cx + s.x / s.z * 7, hy = cy + s.y / s.z * 7;
    if (hx < -1 || hx > W || hy < -1 || hy > H) {
      starSpawn(i, rnd(0.8f, 1));
      continue;
    }
    float tz = s.z + v * 0.06f + 0.005f, tx = cx + s.x / tz * 7, ty = cy + s.y / tz * 7;
    float b = powf(min(1.0f, (1 - s.z) * 1.2f), 2.2f);
    int steps = max(1, (int)ceilf(sqrtf((hx - tx) * (hx - tx) + (hy - ty) * (hy - ty)) * 2));
    for (int j = 0; j <= steps; j++) {
      float f = (float)j / steps;
      uint8_t k = 255 * b * (0.25f + 0.75f * f) * (steps > 1 ? 0.7f : 1);
      addAA(tx + (hx - tx) * f, ty + (hy - ty) * f, scale(s.tint, k, k, k));
    }
  }
}


// ---- Fireworks: rockets with glowing trails over a sleeping town -----------
enum { SP_ROCKET, SP_STAR, SP_WILLOW, SP_CRACKLE, SP_EMBER };
struct Spark { float x, y, vx, vy, life, decay; uint16_t hue; uint8_t sat, kind; } sparks[128];
uint8_t fwTrail[H][W][3];  // glowing light that fades a little every frame
float fwNext, fwFlash, fwFlashX, fwFlashY;
uint32_t fwFlashC;
int fwVolley;

// Adds light at a sub-pixel position, shared over the four nearest pixels.
void fwAdd(float x, float y, uint32_t c) {
  int x0 = floorf(x), y0 = floorf(y);
  float fx = x - x0, fy = y - y0;
  float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
  int ch[3] = {(int)(c >> 16 & 255), (int)(c >> 8 & 255), (int)(c & 255)};
  for (int i = 0; i < 4; i++) {
    int px = x0 + (i & 1), py = y0 + (i >> 1);
    if (px < 0 || px >= W || py < 0 || py >= H) continue;
    uint8_t *p = fwTrail[py][px];
    float wi = w[i];
    for (int k = 0; k < 3; k++) p[k] = min(255, p[k] + (int)(ch[k] * wi));
  }
}
// A free slot in sparks[] (Arduino's prototype generator can't handle Spark* here).
int fwFree() {
  for (int i = 0; i < 128; i++)
    if (sparks[i].life <= 0) return i;
  return -1;
}
void fwLaunch() {
  int i = fwFree();
  if (i >= 0) sparks[i] = {rnd(3, 12), (float)H, rnd(-1.2f, 1.2f), -rnd(13, 15.5f), 1, 0, (uint16_t)random(65536), 255, SP_ROCKET};
}
// Bursts: peony (filled ball), ring, golden willow, or two-colour; some crackle.
void fwBurst(float rx, float ry, float rvx, uint16_t rhue) {
  int type = random(100);
  bool ring = type >= 40 && type < 60, willow = type >= 60 && type < 80, twoTone = type >= 80;
  bool crackle = !willow && random(4) == 0;
  int n = ring ? 22 : willow ? 26 : 30;
  for (int i = 0; i < n; i++) {
    int k = fwFree();
    if (k < 0) break;
    Spark *s = &sparks[k];
    float a = ring ? i * 2 * PI / n : rnd(0, 2 * PI);
    float sp = ring ? 7.5f + rnd(-0.2f, 0.2f) : sqrtf(rnd(0.05f, 1)) * (willow ? 6 : 8.5f);
    uint16_t hue = rhue + random(-1500, 1500);
    uint8_t sat = 230, kind = crackle ? SP_CRACKLE : SP_STAR;
    float decay = rnd(0.7f, 0.95f);
    if (willow) hue = 7000 + random(-800, 800), sat = 170, kind = SP_WILLOW, decay = rnd(0.35f, 0.5f);
    if (twoTone && (i & 1)) hue += 32768;
    *s = {rx, ry, cosf(a) * sp + rvx * 0.3f, sinf(a) * sp - 0.5f, 1, decay, hue, sat, kind};
  }
  fwFlash = 1, fwFlashX = rx, fwFlashY = ry;
  fwFlashC = willow ? 0xFFC060 : hsv(rhue, 120, 255);
}

void fxFireworks() {
  float dt = sdt();
  if (fxFresh) {
    for (auto &s : sparks) s.life = 0;
    memset(fwTrail, 0, sizeof(fwTrail));
    fwNext = 0.3f, fwFlash = 0, fwVolley = 0;
  }
  // Trails fade at the same rate whatever the frame rate.
  uint16_t keep = 256 * powf(0.00002f, dt);
  for (auto &row : fwTrail)
    for (auto &p : row)
      for (auto &v : p) v = v * keep >> 8;
  // Launch rockets, with the odd quick-fire finale.
  fwNext -= dt;
  if (fwNext <= 0) {
    fwLaunch();
    if (fwVolley > 0) fwVolley--, fwNext = rnd(0.15f, 0.3f);
    else if (random(9) == 0) fwVolley = 4 + random(4), fwNext = 0.25f;
    else fwNext = rnd(0.7f, 1.8f);
  }
  for (auto &s : sparks) {
    if (s.life <= 0) continue;
    if (s.kind == SP_ROCKET) {
      s.vy += 9 * dt, s.x += s.vx * dt, s.y += s.vy * dt;
      fwAdd(s.x, s.y, 0xC08850);
      int e = random(4) == 0 ? fwFree() : -1;
      if (e >= 0) sparks[e] = {s.x + rnd(-0.3f, 0.3f), s.y + 0.5f, rnd(-0.6f, 0.6f), rnd(0.5f, 2), 1, 3.5f, 6000, 255, SP_EMBER};
      if (s.vy >= -1.5f) {  // top of the climb
        s.life = 0;
        fwBurst(s.x, s.y, s.vx, s.hue);
      }
      continue;
    }
    bool willow = s.kind == SP_WILLOW;
    float drag = willow ? 1.6f : 1.0f, grav = willow ? 4 : s.kind == SP_EMBER ? 6 : 5.5f;
    s.vx -= s.vx * drag * dt, s.vy -= s.vy * drag * dt, s.vy += grav * dt;
    s.x += s.vx * dt, s.y += s.vy * dt;
    s.life -= s.decay * dt;
    if (s.life <= 0 || s.y > H + 1) {
      s.life = 0;
      continue;
    }
    float L = s.life;
    uint32_t c;
    if (s.kind == SP_EMBER) c = hsv(6000, 255, 90 * L);
    else if (willow) c = hsv(s.hue - (uint16_t)((1 - L) * 4000), s.sat, 220 * min(1.0f, L * 1.3f));
    else {
      c = hsv(s.hue, s.sat, 220 * min(1.0f, L * 1.6f));
      if (L > 0.92f) c = lerpColor(c, 0xFFFFFF, (L - 0.92f) / 0.08f);  // white-hot at first
      if (s.kind == SP_CRACKLE && L < 0.45f) c = random(3) ? 0 : 0xFFFFE0;  // glittering crackle
    }
    fwAdd(s.x, s.y, c);
  }
  // Night sky (lit up briefly by each burst), the glowing light, then the town.
  fwFlash = max(0.0f, fwFlash - dt * 5);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t bg = lerpColor(0x000008, 0x060A1C, y / 15.0f);
      if (fwFlash > 0) {
        float dx = x - fwFlashX, dy = y - fwFlashY;
        float near = max(0.0f, 1 - sqrtf(dx * dx + dy * dy) / 5);
        bg = lerpColor(bg, fwFlashC, fwFlash * (0.04f + 0.35f * near * near));
      }
      uint8_t *p = fwTrail[y][x];
      fb[y][x] = rgb(min(255, (int)(bg >> 16 & 255) + p[0]), min(255, (int)(bg >> 8 & 255) + p[1]), min(255, (int)(bg & 255) + p[2]));
    }
  for (int i = 0; i < 9; i++) {
    uint8_t v = 16 + 14 * sinf(tsec * (1 + fwHash(i)) * 2 + i);
    addPx(fwHash(i * 7 + 1) * 16, fwHash(i * 7 + 2) * 10, v * 0x010101u);
  }
  for (int x = 0; x < W; x++) {
    int h = 1 + (int)(fwHash(x / 2 + 300) * 3);
    for (int y = H - h; y < H; y++)
      fb[y][x] = (x + y) % 3 == 0 && fwHash(x * 16 + y) > 0.55f ? 0x281804 : 0x020206;
  }
}

// A stormy night: two layers of wind-blown rain, splashes and lightning.
struct Drop { float x, y, v; bool near; } rain[26];
struct Splash { float x, age; } splashes[10];
struct Droplet { float x, y, vx, vy, life; } droplets[16];
float boltTimer, boltAge;
int8_t boltX[H], boltBranchY, boltBranchDir;
int splashNext, dropletNext;
void rainSpawn(int i, bool top) {  // by index: Arduino can't prototype Drop&
  Drop &d = rain[i];
  d.near = random(100) < 35;
  d.x = rnd(-4, 16), d.y = top ? rnd(-6, -1) : rnd(-6, 14);
  d.v = d.near ? rnd(15, 19) : rnd(8, 11);
}
float rainFlash() {
  float a = boltAge;
  if (a < 0.07f) return 1;
  if (a < 0.14f) return 0.15f;
  if (a < 0.24f) return 0.9f;
  return a < 1 ? 0.45f * (1 - (a - 0.24f) / 0.76f) : 0;
}
void fxRain() {
  float dt = sdt();
  if (fxFresh) {
    for (int i = 0; i < 26; i++) rainSpawn(i, false);
    for (auto &s : splashes) s.age = 9;
    for (auto &p : droplets) p.life = 0;
    boltTimer = rnd(4, 9), boltAge = 99;
  }
  float wind = 0.18f + 0.14f * sinf(tsec * 0.35f) + 0.06f * sinf(tsec * 1.3f);
  // Lightning: a jagged bolt with a side branch, and a double flash.
  boltTimer -= dt, boltAge += dt;
  if (boltTimer <= 0) {
    boltTimer = rnd(6, 14), boltAge = 0;
    int x = random(3, 13);
    for (int y = 0; y < H; y++) boltX[y] = x, x = constrain(x + (int)random(-1, 2), 1, 14);
    boltBranchY = random(4, 8), boltBranchDir = random(2) ? 1 : -1;
  }
  float flash = rainFlash();
  // Sky with drifting clouds, and a puddle along the bottom.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t c;
      if (y >= 14) c = lerpColor(0x010103, 0x04060C, 0.5f + 0.5f * sinf(x * 1.3f + tsec * 2 + y));
      else {
        c = lerpColor(0x000002, 0x020308, y / 13.0f);
        if (y < 5) {
          float n = sinf(x * 0.6f + tsec * 0.4f) + sinf(x * 0.27f - tsec * 0.25f + y);
          c = lerpColor(c, 0x0A0C14, max(0.0f, (n + 1) / 3) * (1 - y / 5.0f));
        }
      }
      fb[y][x] = flash > 0 ? lerpColor(c, 0x707890, flash * (y < 14 ? 0.6f : 0.35f)) : c;
    }
  // Rain: far drops are dim and slow and stop short, near ones hit the puddle.
  for (int i = 0; i < 26; i++) {
    Drop &d = rain[i];
    d.y += d.v * dt, d.x += d.v * wind * dt;
    if (d.x >= 17) d.x -= 21;
    if (d.y >= (d.near ? 14 : 12.5f)) {
      if (d.near && d.x >= 0 && d.x < W) {
        splashes[splashNext++ % 10] = {d.x, 0};
        for (int j = 0; j < 2; j++) droplets[dropletNext++ % 16] = {d.x, 13.6f, rnd(-3, 3), -rnd(5, 8), 1};
      }
      rainSpawn(i, true);
      continue;
    }
    float len = d.near ? 2.6f : 1.4f;
    uint32_t col = lerpColor(d.near ? 0x4A6CA8 : 0x1A2440, 0xC0C8E0, flash * 0.6f);
    for (int j = 0; j <= 4; j++) {
      float f = j / 4.0f;
      uint8_t k = 255 * (0.2f + 0.8f * f) * 0.45f;
      addAA(d.x - wind * len * (1 - f), d.y - len * (1 - f), scale(col, k, k, k));
    }
  }
  for (auto &s : splashes) {
    if (s.age > 0.45f) continue;
    s.age += dt;
    float rx = 0.6f + s.age * 5, k = max(0.0f, 1 - s.age / 0.45f);
    uint32_t c = scale(0x406090, k * 255, k * 255, k * 255);
    addAA(s.x - rx, 14, c), addAA(s.x + rx, 14, c);
    addAA(s.x - rx * 0.7f, 15, c), addAA(s.x + rx * 0.7f, 15, c);
    if (s.age < 0.08f) addAA(s.x, 14, 0x8090C0);
  }
  for (auto &p : droplets) {
    if (p.life <= 0) continue;
    p.vy += 30 * dt, p.x += p.vx * dt, p.y += p.vy * dt, p.life -= dt * 2.5f;
    if (p.y > 15 || p.life <= 0) p.life = 0;
    else addAA(p.x, p.y, scale(0x6080B0, p.life * 255, p.life * 255, p.life * 255));
  }
  if (flash > 0.5f) {
    for (int y = 0; y < 14; y++) fbAdd(boltX[y], y, 0xE0E0FF);
    for (int i = 1; i <= 4; i++) fbAdd(boltX[boltBranchY] + boltBranchDir * i, boltBranchY + i, 0x8080C0);
  }
}

struct Ball { float x, y, vx, vy; uint16_t hue; } balls[4];
void fxBounce() {
  if (fxFresh)
    for (int i = 0; i < 4; i++) {
      float vx = rnd(2.5f, 6) * (i & 1 ? -1 : 1);
      balls[i] = {rnd(1, 13), rnd(0, 6), vx, 0, (uint16_t)(i * 16384)};
    }
  fadeAll(120, 120, 120);
  float dt = sdt();
  for (auto &b : balls) {
    b.vy += 35 * dt;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    if (b.x < 0) b.x = 0, b.vx = -b.vx;
    if (b.x > W - 2) b.x = W - 2, b.vx = -b.vx;
    if (b.y > H - 2) {
      b.y = H - 2;
      b.vy = -b.vy * 0.85f;
      if (fabsf(b.vy) < 6) b.vy = -rnd(16, 22);  // keep them bouncing
    }
    uint32_t c = hsv(b.hue);
    for (int i = 0; i < 4; i++) setPx(b.x + (i & 1), b.y + (i >> 1), c);
  }
}

void fxSpiral() {
  float t = tsec;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float dx = x - 7.5f, dy = y - 7.5f, d = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
      float v = 0.5f + 0.5f * sinf(3 * a + d * 0.9f - t * 3);
      fb[y][x] = hsv(hueOf(a * 10430 + d * 3000 - t * 6000), 255, v * 255);
    }
}

// Raindrops on a pond: overlapping wave rings on a shaded water surface.
struct Ripple { float x, y, age, amp; } ripples[8];
float rippleNext;
void fxRipples() {
  float dt = sdt();
  if (fxFresh) {
    for (auto &r : ripples) r.age = 99;
    rippleNext = 0;
  }
  rippleNext -= dt;
  if (rippleNext <= 0) {
    int old = 0;
    for (int i = 1; i < 8; i++)
      if (ripples[i].age > ripples[old].age) old = i;
    ripples[old] = {rnd(1, 14), rnd(1, 14), 0, rnd(0.7f, 1.2f)};
    rippleNext = rnd(0.25f, 1.3f);
  }
  // Water height: each drop sends out a short train of waves that fades.
  static float h[H + 2][W + 2];
  for (int y = -1; y <= H; y++)
    for (int x = -1; x <= W; x++) {
      float sum = 0;
      for (auto &r : ripples) {
        if (r.age > 6) continue;
        float dx = x - r.x, dy = y - r.y, d = sqrtf(dx * dx + dy * dy), u = d - 5 * r.age;
        if (u > 1.5f || u < -7) continue;
        float e = expf(-(u + 1.5f) * (u + 1.5f) / 5);
        sum += r.amp * e * cosf(u * 2) * expf(-r.age * 0.7f) / (1 + d * 0.15f);
      }
      h[y + 1][x + 1] = sum;
    }
  for (auto &r : ripples) r.age += dt;
  // Shade by height and slope (light from the top-left), with glints.
  float hueBase = 34000 + 5000 * sinf(tsec * 0.05f);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float hc = h[y + 1][x + 1], slope = h[y][x] - h[y + 2][x + 2];
      float shade = constrain(0.35f + hc * 0.35f + slope * 0.5f, 0.0f, 1.0f);
      uint32_t c = hsv(hueBase + hc * 3000, 230, 25 + shade * 150);
      float spec = slope - 0.35f;
      if (spec > 0) c = lerpColor(c, 0xFFFFFF, min(1.0f, spec * 1.5f));
      fb[y][x] = c;
    }
  for (auto &r : ripples)
    if (r.age < 0.18f) {
      float k = 1 - r.age / 0.18f;
      addAA(r.x, r.y, scale(0xFFFFFF, k * 255, k * 255, k * 255));
    }
}

// A snake that plays itself: heads for the food, avoiding walls and its body.
int8_t snakeX[256], snakeY[256];
int snakeLen, foodX, foodY, snakeDir, snakeDead;
bool snakeHits(int x, int y) {
  if (x < 0 || y < 0 || x >= W || y >= H) return true;
  for (int i = 0; i < snakeLen - 1; i++)  // the tail moves away, so skip it
    if (snakeX[i] == x && snakeY[i] == y) return true;
  return false;
}

void placeFood() {
  do {
    foodX = random(W);
    foodY = random(H);
  } while (snakeHits(foodX, foodY));
}

void snakeReset() {
  snakeLen = 3;
  for (int i = 0; i < snakeLen; i++) snakeX[i] = 7 - i, snakeY[i] = 8;
  snakeDir = 0;
  snakeDead = 0;
  placeFood();
}

void fxSnake() {
  static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
  static float acc;
  if (fxFresh) snakeReset();
  for (int n = ticks(acc, 10); n > 0; n--) {
    if (snakeDead) {
      if (--snakeDead == 0) snakeReset();
      continue;
    }
    int best = -1;
    float bestScore = 1e9;
    for (int d = 0; d < 4; d++) {
      if (d == (snakeDir + 2) % 4) continue;
      int nx = snakeX[0] + DX[d], ny = snakeY[0] + DY[d];
      if (snakeHits(nx, ny)) continue;
      int free = 0;
      for (int k = 0; k < 4; k++) free += !snakeHits(nx + DX[k], ny + DY[k]);
      float score = abs(nx - foodX) + abs(ny - foodY) + (free <= 1 ? 50 : 0) + rnd(0, 0.5f);
      if (score < bestScore) bestScore = score, best = d;
    }
    if (best < 0 || snakeLen > 80) {
      snakeDead = 15;  // flash, then start over
      continue;
    }
    snakeDir = best;
    int nx = snakeX[0] + DX[best], ny = snakeY[0] + DY[best];
    bool grow = nx == foodX && ny == foodY;
    for (int i = snakeLen - (grow ? 0 : 1); i > 0; i--) snakeX[i] = snakeX[i - 1], snakeY[i] = snakeY[i - 1];
    snakeX[0] = nx;
    snakeY[0] = ny;
    if (grow) {
      snakeLen++;
      placeFood();
    }
  }
  fill(0);
  uint8_t pulse = 150 + 100 * sinf(tsec * 8);
  setPx(foodX, foodY, rgb(pulse, 0, 0));
  for (int i = snakeLen - 1; i >= 0; i--) {
    uint32_t c = i == 0 ? 0xC0FFC0 : hsv(hueOf(21000 + i * 400), 255, max(80, 255 - i * 3));
    if (snakeDead) c = snakeDead & 2 ? 0xFF0000 : 0x200000;
    setPx(snakeX[i], snakeY[i], c);
  }
}

// ---- Seasonal effects -------------------------------------------------------
const char *const PUMPKIN[] = {
  ".......GG.......", "......GG........", "...OOOOOOOOOO...", "..OOOOOOOOOOOO..", ".OOOOOOOOOOOOOO.",
  ".OOKKKOOOOKKKOO.", ".OOOKKOOOOKKOOO.", "OOOOOOOKKOOOOOOO", "OOOOOOOOOOOOOOOO", "OOKOOOOOOOOOOKOO",
  "OOKKKOKKKKOKKKOO", ".OOKKKKKKKKKKOO.", ".OOOOKKOOKKOOOO.", "..OOOOOOOOOOOO..", "...OOOOOOOOOO...",
};
const char *const GHOST[] = {
  "......WWWW......", "....WWWWWWWW....", "...WWWWWWWWWW...", "..WWWWWWWWWWWW..", "..WWBBWWWWBBWW..",
  "..WWBBWWWWBBWW..", "..WWWWWWWWWWWW..", "..WWWWWMMWWWWW..", "..WWWWWMMWWWWW..", "..WWWWWWWWWWWW..",
  "..WWWWWWWWWWWW..", "..WWWWWWWWWWWW..", "..W.WW.WW.WW.W..",
};
const char *const BAT_UP[] = {"X.........X", "XX.......XX", "XXX.X.X.XXX", ".XXXXXXXXX.", "....XXX...."};
const char *const BAT_DOWN[] = {"....X.X....", "...XXXXX...", ".XXXXXXXXX.", "XXX.XXX.XXX", "X...XXX...X"};

void fxHalloween() {
  static float flicker = 1;
  int scene = (int)(tsec / 10) % 3;
  if (scene == 0) {  // jack-o'-lantern with a flickering candle inside
    fill(0);
    flicker = constrain(flicker + rnd(-0.15f, 0.15f), 0.45f, 1.0f);
    uint8_t f = flicker * 255;
    candle = scale(0xFFB020, f, f, f);
    drawSprite(PUMPKIN, 15, 0, 0);
  } else if (scene == 1) {  // floating ghost
    fill(0x040010);
    drawSprite(GHOST, 13, 0, 1 + (int)roundf(1.5f * sinf(tsec * 2.5f)));
  } else {  // bat flying past the moon
    fill(0x060014);
    for (int y = 0; y < 7; y++)
      for (int x = 8; x < W; x++) {
        float dx = x - 11.5f, dy = y - 3.5f;
        if (dx * dx + dy * dy < 7) setPx(x, y, 0x807040);
      }
    spriteInk = 0x9030E0;
    int bx = 16 - (int)fmodf(tsec * 5, 30), by = 6 + (int)roundf(2.5f * sinf(tsec * 3));
    drawSprite((int)(tsec * 6) % 2 ? BAT_UP : BAT_DOWN, 5, bx, by);
  }
}

struct Flake { float x, y, v; } flakes[24];
uint8_t snowPile[W];
void fxSnow() {
  static bool melting;
  if (fxFresh) {
    for (auto &f : flakes) f = {rnd(0, W - 0.01f), rnd(-16, 0), rnd(2, 4)};
    memset(snowPile, 0, sizeof(snowPile));
    melting = false;
  }
  fill(0);
  // Tree with a star and twinkling lights.
  for (int y = 2; y <= 12; y++)
    for (int x = 0; x < W; x++)
      if (fabsf(x - 7.5f) <= (y - 2) * 0.5f + 0.5f) {
        if ((x * 7 + y * 3) % 5 == 0)
          setPx(x, y, hsv(hueOf(x * 9000 + y * 5000 + tsec * 2000), 200, 128 + 127 * sinf(tsec * 4 + x * 1.7f + y * 2.3f)));
        else
          setPx(x, y, 0x005010);
      }
  setPx(7, 13, 0x401800); setPx(8, 13, 0x401800);
  setPx(7, 14, 0x401800); setPx(8, 14, 0x401800);
  uint8_t star = 180 + 75 * sinf(tsec * 3);
  setPx(7, 1, rgb(star, star * 0.8f, 0)); setPx(8, 1, rgb(star, star * 0.8f, 0));
  // Snow piling up, then melting away.
  int total = 0;
  for (int x = 0; x < W; x++) total += snowPile[x];
  if (total > W * 3) melting = true;
  if (melting && rnd(0, 1) < sdt() * 8) {
    int x = random(W);
    if (snowPile[x]) snowPile[x]--;
  }
  if (melting && total == 0) melting = false;
  for (int x = 0; x < W; x++)
    for (int k = 0; k < snowPile[x]; k++) setPx(x, H - 1 - k, 0xA0A0B0);
  for (int i = 0; i < 24; i++) {
    Flake &f = flakes[i];
    f.y += f.v * sdt();
    f.x = fmodf(f.x + sinf(tsec * 2 + i) * sdt() * 0.8f + W, W);
    int fx = f.x;
    if (f.y >= H - 1 - snowPile[fx]) {
      if (!melting && snowPile[fx] < 4) snowPile[fx]++;
      f = {rnd(0, W - 0.01f), rnd(-4, 0), rnd(2, 4)};
    } else if (f.y >= 0) {
      setPx(fx, f.y, 0xD0D0E0);
    }
  }
}

const char *const CHASE_GHOST[2][7] = {
  {"..XXX..", ".XXXXX.", "XEPXEPX", "XXXXXXX", "XXXXXXX", "XXXXXXX", "X.X.X.X"},
  {"..XXX..", ".XXXXX.", "XEPXEPX", "XXXXXXX", "XXXXXXX", "XXXXXXX", ".X.X.X."},
};

void drawChomper(float cx, int dir) {
  float mouth = 0.1f + 0.7f * fabsf(sinf(tsec * 9));
  for (int y = 4; y < 12; y++)
    for (int x = (int)cx - 4; x <= (int)cx + 4; x++) {
      float dx = x - cx, dy = y - 7.5f;
      if (dx * dx + dy * dy > 14 || fabsf(atan2f(dy, dx * dir)) < mouth) continue;
      setPx(x, y, 0xFFD800);
    }
}

// A chomper runs right with a ghost behind it, then chases a scared ghost back.
void fxChase() {
  const float L = 44;
  float pos = fmodf(tsec * 7, 2 * L);
  int frame = (int)(tsec * 6) % 2;
  fill(0);
  if (pos < L) {
    float cx = pos - 6;
    for (int x = 1; x < W; x += 3)
      if (x > cx + 1) setPx(x, 7, 0x806040), setPx(x, 8, 0x806040);
    drawChomper(cx, 1);
    spriteInk = 0xFF0000, eyeWhite = 0xFFFFFF, pupil = 0x2020FF;
    drawSprite(CHASE_GHOST[frame], 7, (int)cx - 14, 4);
  } else {
    float gx = W + 2 - (pos - L);
    spriteInk = 0x2030FF, eyeWhite = pupil = 0xFFC0C0;
    drawSprite(CHASE_GHOST[frame], 7, (int)gx, 4);
    drawChomper(gx + 15, -1);
  }
}

const char *const HEART[] = {".X.X.", "XXXXX", "XXXXX", ".XXX.", "..X.."};
struct Floater { float x, y, v, phase; uint32_t color; } hearts[5];
void fxHearts() {
  static const uint32_t COLORS[] = {0xFF1040, 0xFF60A0, 0xC020FF, 0xFF0010};
  if (fxFresh)
    for (auto &h : hearts) h = {rnd(0, 11), rnd(0, 20), rnd(2, 4), rnd(0, 6.28f), COLORS[random(4)]};
  fill(0);
  for (auto &h : hearts) {
    h.y -= h.v * sdt();
    if (h.y < -5) h = {rnd(0, 11), H + rnd(0, 6), rnd(2, 4), rnd(0, 6.28f), COLORS[random(4)]};
    spriteInk = h.color;
    drawSprite(HEART, 5, (int)roundf(h.x + 1.2f * sinf(tsec * 2 + h.phase)), (int)roundf(h.y));
  }
}

// ---- Clouds: soft, puffy clouds drifting across a slowly changing sky -----
// Each cloud is a few overlapping puffs; far clouds are smaller, fainter and
// slower than near ones, which gives a gentle sense of depth.
struct Cloud { float x, y, s, v, a, px[4], py[4], pr[4]; };
Cloud clouds[5];  // 0-2 far, 3-4 near
float cloudSky = 0;

void cloudNew(int i, bool anywhere) {
  Cloud &c = clouds[i];
  bool nearLayer = i >= 3;
  c.s = nearLayer ? rnd(0.8f, 1.0f) : rnd(0.5f, 0.65f);
  c.v = nearLayer ? rnd(0.7f, 1.0f) : rnd(0.3f, 0.45f);
  c.a = nearLayer ? 0.95f : 0.7f;
  c.y = nearLayer ? rnd(6, 12) : rnd(1.5f, 8);
  c.x = anywhere ? (i % 3) * 6.5f + rnd(-2, 2) - 2 : -6 * c.s - rnd(0, 10);
  for (int k = 0; k < 4; k++) {
    c.px[k] = ((k - 1.5f) * 2.2f + rnd(-0.5f, 0.5f)) * c.s;
    c.py[k] = (rnd(-0.4f, 0.3f) - (k == 1 || k == 2 ? 1.1f : 0)) * c.s;
    c.pr[k] = (k == 1 || k == 2 ? rnd(2.1f, 2.7f) : rnd(1.5f, 2)) * c.s;
  }
}

void fxClouds() {
  // Sky moods, each with its own cloud lighting and sun: top, horizon,
  // sunlit cloud, cloud shadow, sun.
  static const uint32_t SKY[][5] = {
      {0x0838B0, 0x58A8F0, 0xFFFFFF, 0x7890B8, 0xFFF4C0},  // clear day
      {0x203888, 0xF0A050, 0xFFE0A8, 0x7A5878, 0xFFC060},  // golden hour
      {0x241450, 0xE07090, 0xFFC0D0, 0x5A3C70, 0xFF9070},  // pink dusk
      {0x2858B8, 0xD0C0E0, 0xFFF8F0, 0x8888B0, 0xFFF0D0},  // soft morning
  };
  const int MOODS = 4;
  float dt = sdt();
  if (fxFresh) {
    for (int i = 0; i < 5; i++) cloudNew(i, true);
    cloudSky = esp_random() % MOODS;
  }
  // Each mood holds for a while, then melts into the next.
  cloudSky = fmodf(cloudSky + dt / 24, MOODS);
  int m0 = (int)cloudSky, m1 = (m0 + 1) % MOODS;
  float f = constrain((cloudSky - m0 - 0.7f) / 0.3f, 0.0f, 1.0f);
  f = f * f * (3 - 2 * f);
  uint32_t mood[5];
  for (int k = 0; k < 5; k++) mood[k] = lerpColor(SKY[m0][k], SKY[m1][k], f);

  for (int y = 0; y < H; y++) {
    uint32_t c = lerpColor(mood[0], mood[1], y / 15.0f);
    for (int x = 0; x < W; x++) fb[y][x] = c;
  }
  // A soft sun with a glow, behind the clouds.
  for (int y = 0; y < 9; y++)
    for (int x = 7; x < W; x++) {
      float d = sqrtf((x - 12.0f) * (x - 12.0f) + (y - 3.5f) * (y - 3.5f));
      if (d < 1.6f) fb[y][x] = mood[4];
      else if (d < 5) fb[y][x] = lerpColor(fb[y][x], mood[4], 0.45f * (1 - (d - 1.6f) / 3.4f));
    }

  for (int i = 0; i < 5; i++) {
    Cloud &c = clouds[i];
    c.x += c.v * dt;
    if (c.x - 6 * c.s > W) cloudNew(i, false);
    float cy = c.y + 0.25f * sinf(tsec * 0.4f + i * 1.7f);
    int x0 = max(0, (int)(c.x - 6 * c.s)), x1 = min(W - 1, (int)(c.x + 6 * c.s) + 1);
    int y0 = max(0, (int)(cy - 4 * c.s)), y1 = min(H - 1, (int)(cy + 3 * c.s) + 1);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        float d = 0;
        for (int k = 0; k < 4; k++) {
          float dx = x - (c.x + c.px[k]), dy = (y - (cy + c.py[k])) * 1.25f;
          d = max(d, 1 - sqrtf(dx * dx + dy * dy) / c.pr[k]);
        }
        if (d <= 0) continue;
        float al = min(1.0f, d * 2.4f);
        al = al * al * (3 - 2 * al) * c.a;
        // Lit from above: bright tops, shadowed undersides.
        float h = constrain((y - (cy - 2.6f * c.s)) / (4.2f * c.s), 0.0f, 1.0f);
        uint32_t col = lerpColor(mood[2], mood[3], h * h);
        if (i < 3) col = lerpColor(col, mood[1], 0.3f);  // far clouds fade into the haze
        fb[y][x] = lerpColor(fb[y][x], col, al);
      }
  }
  // Gamma for deeper, richer sky colours on the LEDs.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = Adafruit_NeoPixel::gamma32(fb[y][x]);
}

#include "stories.h"
#include "calm.h"

// Starts a transition away from whatever is on the LEDs now.
void startTransition(uint32_t now) {
  memcpy(prevFb, shownFb, sizeof(fb));  // includes any transition still in progress
  transAt = now ? now : 1;
  int style = esp_random() % 5;
  bool rev = esp_random() % 2;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float n;
      switch (style) {
        case 0: n = 0; break;                                                   // crossfade
        case 1: n = (esp_random() & 0xFF) / 255.0f; break;                      // sparkly dissolve
        case 2: n = (x + y) / 30.0f; break;                                     // diagonal sweep
        case 3: n = sqrtf((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f)) / 10.7f; break;  // ripple from the middle
        default: n = x / 15.0f; break;                                          // side to side
      }
      if (rev && style >= 2) n = 1 - n;
      transN[y][x] = n * 255;
    }
}

void resetEffect() {
  fill(0);
  memset(heat, 0, sizeof(heat));
  memset(drops, -1, sizeof(drops));
  lifeSeed();
  fxFresh = true;
}

void drawEffect() {
  switch (cfg.effect) {
    case FX_SOLID:     fill(cfg.color); break;
    case FX_RAINBOW:   fxRainbow(); break;
    case FX_PLASMA:    fxPlasma(); break;
    case FX_FIRE:      fxFire(); break;
    case FX_MATRIX:    fxMatrix(); break;
    case FX_TWINKLE:   fxTwinkle(); break;
    case FX_LIFE:      fxLife(); break;
    case FX_AURORA:    fxAurora(); break;
    case FX_LAVA:      fxLava(); break;
    case FX_OCEAN:     fxOcean(); break;
    case FX_SUNSET:    fxSunset(); break;
    case FX_BREATHE:   fxBreathe(); break;
    case FX_STARS:     fxStars(); break;
    case FX_FIREWORKS: fxFireworks(); break;
    case FX_RAIN:      fxRain(); break;
    case FX_BOUNCE:    fxBounce(); break;
    case FX_SPIRAL:    fxSpiral(); break;
    case FX_RIPPLES:   fxRipples(); break;
    case FX_SNAKE:     fxSnake(); break;
    case FX_HALLOWEEN: fxHalloween(); break;
    case FX_SNOW:      fxSnow(); break;
    case FX_CHASE:     fxChase(); break;
    case FX_HEARTS:    fxHearts(); break;
    case FX_GARDEN:    fxGarden(); break;
    case FX_ROCKET:    fxRocket(); break;
    case FX_FISH:      fxFish(); break;
    case FX_DAY:       fxDay(); break;
    case FX_CAT:       fxCat(); break;
    case FX_SNOWMAN:   fxSnowman(); break;
    case FX_DREAM:     fxDream(); break;
    case FX_HATCH:     fxHatch(); break;
    case FX_UFO:       fxUfo(); break;
    case FX_SHOWER:    fxShower(); break;
    case FX_VOLCANO:   fxVolcano(); break;
    case FX_FETCH:     fxFetch(); break;
    case FX_RACE:      fxRace(); break;
    case FX_BRICKS:    fxBricks(); break;
    case FX_PUNCH:     fxPunch(); break;
    case FX_WEB:       fxWeb(); break;
    case FX_IRON:      fxIron(); break;
    case FX_SKY:       fxSky(); break;
    case FX_BAT:       fxBat(); break;
    case FX_SHIELD:    fxShield(); break;
    case FX_SMASH:     fxSmash(); break;
    case FX_SPARK:     fxSpark(); break;
    case FX_ANT:       fxAnt(); break;
    case FX_THUNDER:   fxThunder(); break;
    case FX_ARROW:     fxArrow(); break;
    case FX_BARK:      fxBark(); break;
    case FX_CLOUDS:    fxClouds(); break;
    case FX_FIREFLIES: fxFireflies(); break;
    case FX_KOI: fxKoi(); break;
    case FX_LEAVES: fxLeaves(); break;
    case FX_CANDLE: fxCandle(); break;
    case FX_CAMPFIRE: fxCampfire(); break;
    case FX_RAINYWIN: fxRainyWindow(); break;
    case FX_MOONRISE: fxMoonrise(); break;
    case FX_JELLY: fxJellyfish(); break;
    case FX_INK: fxInk(); break;
    case FX_TIDEPOOL: fxTidePool(); break;
    case FX_KALEIDO: fxKaleido(); break;
    case FX_GALAXY: fxGalaxy(); break;
    case FX_SANDGARDEN: fxSandGarden(); break;
    case FX_HOURGLASS: fxHourglass(); break;
    case FX_BREATHEGUIDE: fxBreatheGuide(); break;
  }
  fxFresh = false;
}

// ---- Text, clock, image -----------------------------------------------------
// Draws a line of text; short text is centred, long text scrolls.
// Returns true each time a scroll pass completes.
bool drawScroll(const String &s, float &x, uint32_t ink, bool rainbow) {
  gfx.ink = ink;
  gfx.rainbow = rainbow;
  int w = s.length() * 6 - 1;
  if (w <= W) {
    gfx.setCursor((W - w) / 2, 4);
    gfx.print(s);
    return true;
  }
  gfx.setCursor((int)x, 4);
  gfx.print(s);
  x -= frameDt * (4 + cfg.speed * 0.25f);
  if (x < -w) {
    x = W;
    return true;
  }
  return false;
}

void drawClock() {
  gfx.ink = cfg.textColor;
  gfx.rainbow = cfg.rainbowText;
  struct tm ti;
  if (!getLocalTime(&ti, 0)) {
    gfx.setCursor(2, 0);
    gfx.print("--");
    gfx.setCursor(2, 9);
    gfx.print("--");
    return;
  }
  int h = ti.tm_hour;
  if (!cfg.clock24) h = h % 12 ? h % 12 : 12;
  char buf[4];
  snprintf(buf, sizeof(buf), cfg.clock24 ? "%02d" : "%2d", h);
  gfx.setCursor(2, 0);
  gfx.print(buf);
  snprintf(buf, sizeof(buf), "%02d", ti.tm_min);
  gfx.setCursor(2, 9);
  gfx.print(buf);
  // Seconds: a small dot travelling across the gap between the rows.
  uint32_t dot = cfg.rainbowText ? 0x606060 : scale(cfg.textColor, 100, 100, 100);
  int sx = ti.tm_sec * W / 60;
  setPx(sx, 7, dot);
  setPx(sx, 8, dot);
}

void drawImage() {
  for (int i = 0; i < NUM_LEDS; i++)
    fb[i / W][i % W] = (uint32_t)imgBuf[i * 3] << 16 | imgBuf[i * 3 + 1] << 8 | imgBuf[i * 3 + 2];
}

String slotKey(int slot) { return "img" + String(slot); }

bool loadImage(int slot) {
  if (slot < 0 || slot >= IMG_SLOTS || !prefs.isKey(slotKey(slot).c_str())) return false;
  prefs.getBytes(slotKey(slot).c_str(), imgBuf, IMG_BYTES);
  return true;
}

// ---- Playlist ---------------------------------------------------------------
bool playItem(uint8_t item) {
  textPassDone = false;
  if (item < FX_COUNT) {
    cfg.mode = MODE_EFFECT;
    cfg.effect = item;
  } else if (item == PL_CLOCK) {
    cfg.mode = MODE_CLOCK;
  } else if (item == PL_TEXT) {
    cfg.mode = MODE_TEXT;
    textX = W;
  } else if (item >= PL_IMG && item < PL_IMG + IMG_SLOTS && loadImage(item - PL_IMG)) {
    cfg.mode = MODE_IMAGE;
    cfg.slot = item - PL_IMG;
  } else {
    return false;  // e.g. an empty picture slot
  }
  return true;
}

void nextItem() {
  lastCycle = millis();
  for (int tries = 0; tries < plLen; tries++) {
    if (cfg.shuffle && plLen > 1) {
      // Deal from a shuffled deck so nothing comes round again until
      // everything else has had a turn.
      if (bagPos >= bagLen || bagLen != plLen) {
        bagLen = plLen, bagPos = 0;
        for (int i = 0; i < bagLen; i++) bag[i] = i;
        for (int i = bagLen - 1; i > 0; i--) {
          int j = esp_random() % (i + 1);
          uint8_t tmp = bag[i];
          bag[i] = bag[j], bag[j] = tmp;
        }
        if (bag[0] == plPos) {  // don't repeat across the reshuffle either
          uint8_t tmp = bag[0];
          bag[0] = bag[bagLen - 1], bag[bagLen - 1] = tmp;
        }
      }
      plPos = bag[bagPos++];
    } else {
      plPos = (plPos + 1) % plLen;
    }
    if (playItem(playlist[plPos])) return;
  }
}

void defaultPlaylist() {
  plLen = 0;
  for (int i = 1; i < FX_COUNT; i++) playlist[plLen++] = i;
  playlist[plLen++] = PL_CLOCK;
}

// ---- Settings storage -------------------------------------------------------
void loadCfg() {
  cfg.on = prefs.getBool("on", true);
  cfg.mode = prefs.getUChar("mode", MODE_EFFECT);
  cfg.effect = prefs.getUChar("fx", FX_RAINBOW) % FX_COUNT;
  cfg.brightness = prefs.getUChar("bri", 80);
  cfg.speed = prefs.getUChar("speed", 50);
  cfg.slot = prefs.getUChar("slot", 0);
  cfg.color = prefs.getULong("color", 0xFF8030);
  cfg.textColor = prefs.getULong("tcolor", 0x00A0FF);
  cfg.rainbowText = prefs.getBool("rainbow", true);
  cfg.autoCycle = prefs.getBool("auto", false);
  cfg.cycleSecs = prefs.getUShort("cycle", 30);
  cfg.clock24 = prefs.getBool("h24", false);
  cfg.text = prefs.getString("text", "Hello!");
  cfg.tz = prefs.getString("tz", "EST5EDT,M3.2.0,M11.1.0");
  cfg.corner = prefs.getUChar("corner", 0) & 3;
  cfg.rotate = prefs.getUChar("rot", 0) & 3;
  cfg.vertical = prefs.getBool("vert", false);
  cfg.serpentine = prefs.getBool("serp", true);
  cfg.shuffle = prefs.getBool("shuffle", false);
  if (prefs.isKey("pl")) plLen = prefs.getBytes("pl", playlist, MAX_PL);
  else defaultPlaylist();
  if (cfg.mode == MODE_IMAGE && !loadImage(cfg.slot)) cfg.mode = MODE_EFFECT;
}

void saveCfg() {
  prefs.putBool("on", cfg.on);
  prefs.putUChar("mode", cfg.mode);
  prefs.putUChar("fx", cfg.effect);
  prefs.putUChar("bri", cfg.brightness);
  prefs.putUChar("speed", cfg.speed);
  prefs.putUChar("slot", cfg.slot);
  prefs.putULong("color", cfg.color);
  prefs.putULong("tcolor", cfg.textColor);
  prefs.putBool("rainbow", cfg.rainbowText);
  prefs.putBool("auto", cfg.autoCycle);
  prefs.putUShort("cycle", cfg.cycleSecs);
  prefs.putBool("h24", cfg.clock24);
  prefs.putString("text", cfg.text);
  prefs.putString("tz", cfg.tz);
  prefs.putUChar("corner", cfg.corner);
  prefs.putUChar("rot", cfg.rotate);
  prefs.putBool("vert", cfg.vertical);
  prefs.putBool("serp", cfg.serpentine);
  prefs.putBool("shuffle", cfg.shuffle);
  prefs.putBytes("pl", playlist, plLen);
}

// ---- Web API ----------------------------------------------------------------
String hex6(uint32_t c) {
  char b[7];
  snprintf(b, sizeof(b), "%06X", (unsigned)(c & 0xFFFFFF));
  return b;
}

String jsonStr(const String &s) {
  String o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\', o += c;
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o + "\"";
}

String netInfo() {
  if (apMode) return "Setup hotspot \"" AP_NAME "\" at 192.168.4.1. Enter your Wi-Fi below.";
  if (WiFi.status() == WL_CONNECTED)
    return "Connected to " + WiFi.SSID() + ": http://" + WiFi.localIP().toString() + " or http://" HOSTNAME ".local";
  return "Connecting to " + prefs.getString("ssid", "") + "...";
}

void sendState() {
  struct tm ti;
  char tbuf[16] = "";
  if (getLocalTime(&ti, 0)) strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &ti);
  String j = "{";
  j += "\"on\":" + String(cfg.on);
  j += ",\"mode\":" + String(cfg.mode);
  j += ",\"effect\":" + String(cfg.effect);
  j += ",\"bri\":" + String(cfg.brightness);
  j += ",\"speed\":" + String(cfg.speed);
  j += ",\"slot\":" + String(cfg.slot);
  j += ",\"color\":\"#" + hex6(cfg.color) + "\"";
  j += ",\"tcolor\":\"#" + hex6(cfg.textColor) + "\"";
  j += ",\"rainbow\":" + String(cfg.rainbowText);
  j += ",\"auto\":" + String(cfg.autoCycle);
  j += ",\"cycle\":" + String(cfg.cycleSecs);
  j += ",\"h24\":" + String(cfg.clock24);
  j += ",\"text\":" + jsonStr(cfg.text);
  j += ",\"tz\":" + jsonStr(cfg.tz);
  j += ",\"corner\":" + String(cfg.corner);
  j += ",\"rot\":" + String(cfg.rotate);
  j += ",\"vertical\":" + String(cfg.vertical);
  j += ",\"serp\":" + String(cfg.serpentine);
  j += ",\"shuffle\":" + String(cfg.shuffle);
  j += ",\"pl\":[";
  for (int i = 0; i < plLen; i++) j += (i ? "," : "") + String(playlist[i]);
  j += "]";
  int slots = 0;
  for (int i = 0; i < IMG_SLOTS; i++)
    if (prefs.isKey(slotKey(i).c_str())) slots |= 1 << i;
  j += ",\"slots\":" + String(slots);
  j += ",\"time\":" + jsonStr(tbuf);
  j += ",\"net\":" + jsonStr(netInfo());
  j += "}";
  server.send(200, "application/json", j);
}

int argInt(const char *name, int lo, int hi) { return constrain(server.arg(name).toInt(), lo, hi); }
bool argBool(const char *name) { return server.arg(name) == "1" || server.arg(name) == "true"; }
uint32_t argColor(const char *name) {
  String s = server.arg(name);
  s.replace("#", "");
  return strtoul(s.c_str(), nullptr, 16) & 0xFFFFFF;
}

void handleSet() {
  if (server.hasArg("on")) cfg.on = argBool("on");
  if (server.hasArg("mode")) cfg.mode = argInt("mode", 0, 3);
  if (server.hasArg("effect")) cfg.effect = argInt("effect", 0, FX_COUNT - 1);
  if (server.hasArg("bri")) cfg.brightness = argInt("bri", 1, 255);
  if (server.hasArg("speed")) cfg.speed = argInt("speed", 1, 100);
  if (server.hasArg("color")) cfg.color = argColor("color");
  if (server.hasArg("tcolor")) cfg.textColor = argColor("tcolor");
  if (server.hasArg("rainbow")) cfg.rainbowText = argBool("rainbow");
  if (server.hasArg("shuffle")) cfg.shuffle = argBool("shuffle");
  if (server.hasArg("cycle")) cfg.cycleSecs = argInt("cycle", 5, 3600);
  if (server.hasArg("h24")) cfg.clock24 = argBool("h24");
  if (server.hasArg("corner")) cfg.corner = argInt("corner", 0, 3);
  if (server.hasArg("rot")) cfg.rotate = argInt("rot", 0, 3);
  if (server.hasArg("vertical")) cfg.vertical = argBool("vertical");
  if (server.hasArg("serp")) cfg.serpentine = argBool("serp");
  if (server.hasArg("text")) {
    cfg.text = server.arg("text").substring(0, 200);
    textX = W;
  }
  if (server.hasArg("tz")) {
    cfg.tz = server.arg("tz").substring(0, 60);
    setenv("TZ", cfg.tz.c_str(), 1);
    tzset();
  }
  if (server.hasArg("epoch")) {
    // The phone's clock, used when there's no internet time (e.g. hotspot mode).
    struct tm ti;
    if (!getLocalTime(&ti, 0)) {
      timeval tv = {(time_t)server.arg("epoch").toInt(), 0};
      settimeofday(&tv, nullptr);
    }
  }
  if (server.hasArg("slot")) {
    int slot = argInt("slot", 0, IMG_SLOTS - 1);
    if (loadImage(slot)) cfg.slot = slot;
  }
  if (server.hasArg("pl")) {
    String s = server.arg("pl");
    plLen = 0, bagLen = 0;
    for (int i = 0; i < (int)s.length() && plLen < MAX_PL;) {
      int end = s.indexOf(',', i);
      if (end < 0) end = s.length();
      int v = s.substring(i, end).toInt();
      if (end > i && (v < FX_COUNT || v == PL_CLOCK || v == PL_TEXT || (v >= PL_IMG && v < PL_IMG + IMG_SLOTS)))
        playlist[plLen++] = v;
      i = end + 1;
    }
  }
  if (server.hasArg("auto")) {
    cfg.autoCycle = argBool("auto");
    if (cfg.autoCycle) nextItem();
  } else if (server.hasArg("mode") || server.hasArg("effect") || server.hasArg("slot")) {
    cfg.autoCycle = false;  // picking something by hand pauses the playlist
  }
  saveAt = millis() + 2000;
  sendState();
}

bool parseImage(const String &d) {
  if (d.length() != IMG_BYTES * 2) return false;
  for (int i = 0; i < IMG_BYTES; i++) {
    char b[3] = {d[i * 2], d[i * 2 + 1], 0};
    imgBuf[i] = strtoul(b, nullptr, 16);
  }
  return true;
}

void handlePixels() {
  if (!parseImage(server.arg("d"))) return server.send(400, "text/plain", "bad image");
  cfg.autoCycle = false;
  cfg.mode = MODE_IMAGE;
  cfg.slot = LIVE_SLOT;
  sendState();
}

void handleSave() {
  int slot = argInt("slot", 0, IMG_SLOTS - 1);
  if (!parseImage(server.arg("d"))) return server.send(400, "text/plain", "bad image");
  prefs.putBytes(slotKey(slot).c_str(), imgBuf, IMG_BYTES);
  cfg.autoCycle = false;
  cfg.mode = MODE_IMAGE;
  cfg.slot = slot;
  saveAt = millis() + 2000;
  sendState();
}

// What the panel is showing right now, as 256 hex colours (for previews).
void handleFrame() {
  String out;
  out.reserve(NUM_LEDS * 6);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) out += hex6(shownFb[y][x]);
  server.send(200, "text/plain", out);
}

void handleImage() {
  uint8_t buf[IMG_BYTES];
  String key = slotKey(argInt("slot", 0, IMG_SLOTS - 1));
  if (!prefs.isKey(key.c_str())) return server.send(200, "text/plain", "");
  prefs.getBytes(key.c_str(), buf, IMG_BYTES);
  String out;
  out.reserve(IMG_BYTES * 2);
  char b[3];
  for (int i = 0; i < IMG_BYTES; i++) {
    snprintf(b, sizeof(b), "%02x", buf[i]);
    out += b;
  }
  server.send(200, "text/plain", out);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).isEmpty() || j.indexOf(jsonStr(WiFi.SSID(i))) >= 0) continue;
    if (j.length() > 1) j += ",";
    j += jsonStr(WiFi.SSID(i));
  }
  WiFi.scanDelete();
  server.send(200, "application/json", j + "]");
}

void handleWifi() {
  prefs.putString("ssid", server.arg("ssid"));
  prefs.putString("pass", server.arg("pass"));
  server.send(200, "text/plain", "ok");
  delay(500);
  ESP.restart();
}

void setupServer() {
  server.on("/", [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/state", sendState);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/pixels", HTTP_POST, handlePixels);
  server.on("/api/save", HTTP_POST, handleSave);
  server.on("/api/image", handleImage);
  server.on("/api/frame", handleFrame);
  server.on("/api/scan", handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  // Anything else (phone captive-portal checks included) goes to the control page.
  server.onNotFound([] {
    server.sendHeader("Location", "http://" + (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
}

// ---- Wi-Fi ------------------------------------------------------------------
void showNotice(const String &s, int repeats) {
  notice = s;
  noticeRepeats = repeats;
  noticeX = W;
}

void startWifi() {
  String ssid = prefs.getString("ssid", "");
  WiFi.setHostname(HOSTNAME);
  if (!ssid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(TX_POWER);
    WiFi.begin(ssid.c_str(), prefs.getString("pass", "").c_str());
    Serial.printf("Connecting to %s", ssid.c_str());
    // Spinner around the border while connecting (up to 15 s).
    for (int i = 0; i < 300 && WiFi.status() != WL_CONNECTED; i++) {
      fill(0);
      int p = i % 60, x, y;
      if (p < 15) x = p, y = 0;
      else if (p < 30) x = 15, y = p - 15;
      else if (p < 45) x = 45 - p, y = 15;
      else x = 0, y = 60 - p;
      setPx(x, y, 0x0060FF);
      pushFrame();
      delay(50);
    }
    if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      Serial.printf("\nConnected: http://%s  http://%s.local\n", ip.c_str(), HOSTNAME);
      MDNS.begin(HOSTNAME);
      MDNS.addService("http", "tcp", 80);
      configTzTime(cfg.tz.c_str(), "pool.ntp.org", "time.nist.gov");
      showNotice(ip, 2);
      return;
    }
    Serial.println("\nWi-Fi failed, starting setup hotspot");
  }
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(TX_POWER);
  WiFi.softAP(AP_NAME, nullptr, 6);
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("Setup hotspot \"%s\" at http://%s\n", AP_NAME, WiFi.softAPIP().toString().c_str());
  showNotice("WiFi: " AP_NAME, 1000);
}

// ---- Main -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never stall the animation when nothing is reading the USB port
  prefs.begin("matrix", false);
  loadCfg();
  setenv("TZ", cfg.tz.c_str(), 1);
  tzset();
  strip.begin();
  gfx.setTextWrap(false);
  resetEffect();
  pushFrame();
#ifdef SELFTEST
  // Runs every effect for 500 frames at top speed and reports timing.
  uint8_t speed = cfg.speed;
  cfg.speed = 100;
  frameDt = 0.02f;
  for (int i = 0; i < FX_COUNT; i++) {
    cfg.effect = i;
    resetEffect();
    uint32_t t0 = micros();
    for (int f = 0; f < 500; f++) {
      tsec += frameDt * 2;
      drawEffect();
    }
    Serial.printf("[selftest] effect %d ok, %lu us/frame\n", i, (micros() - t0) / 500);
  }
  cfg.speed = speed;
  cfg.effect = prefs.getUChar("fx", FX_RAINBOW) % FX_COUNT;
#endif
  startWifi();
  setupServer();
  lastFrame = lastCycle = millis();
  if (cfg.autoCycle) nextItem();
}

void loop() {
  server.handleClient();
  if (apMode) dns.processNextRequest();
  if (saveAt && millis() > saveAt) {
    saveCfg();
    saveAt = 0;
  }

  uint32_t now = millis();
  static uint32_t lastStatus;
  if (now - lastStatus > 5000) {
    lastStatus = now;
    if (apMode)
      Serial.printf("[status] hotspot \"%s\" at %s, %d phone(s) connected\n", AP_NAME,
                    WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
    else
      Serial.printf("[status] wifi %s, ip %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                    WiFi.localIP().toString().c_str());
  }
  if (now - lastFrame < 16) return;
  frameDt = (now - lastFrame) / 1000.0f;
  lastFrame = now;
  tsec += frameDt * cfg.speed / 50.0f;
  if (tsec > 10000) tsec -= 10000;  // keep float maths precise on long runs

  // Messages and stories play to the end before the playlist moves on.
  bool midway = (cfg.mode == MODE_TEXT && !textPassDone) || (cfg.mode == MODE_EFFECT && isStory(cfg.effect) && !storyDone);
  if (cfg.autoCycle && now - lastCycle > cfg.cycleSecs * 1000UL && !midway) nextItem();
  if (cfg.mode != lastMode || cfg.effect != lastEffect || (cfg.mode == MODE_IMAGE && cfg.slot != lastSlot)) {
    if (lastMode != 255) startTransition(now);
    resetEffect();
    lastMode = cfg.mode;
    lastEffect = cfg.effect;
    lastSlot = cfg.slot;
  }

  if (!notice.isEmpty()) {
    // In setup mode the hotspot name keeps scrolling until a phone connects.
    if (apMode && WiFi.softAPgetStationNum() > 0) notice = "";
    fill(0);
    if (drawScroll(notice, noticeX, 0xFFFFFF, false) && --noticeRepeats <= 0) notice = "";
    memcpy(shownFb, fb, sizeof(fb));
    pushFrame();
    return;
  }

  switch (cfg.mode) {
    case MODE_EFFECT: drawEffect(); break;
    case MODE_CLOCK:  fill(0); drawClock(); break;
    case MODE_TEXT:
      fill(0);
      if (drawScroll(cfg.text, textX, cfg.textColor, cfg.rainbowText)) textPassDone = true;
      break;
    case MODE_IMAGE:  drawImage(); break;
  }
  // Blend the old picture into the new one, without disturbing the effect's
  // own frame (many effects build on the previous frame).
  float tp = transAt ? (now - transAt) / (float)TRANS_MS : 1;
  if (tp < 1) {
    memcpy(liveFb, fb, sizeof(fb));
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float f = constrain((tp * 1.6f - transN[y][x] / 255.0f * 0.6f), 0.0f, 1.0f);
        fb[y][x] = lerpColor(prevFb[y][x], fb[y][x], f * f * (3 - 2 * f));
      }
  } else transAt = 0;
  memcpy(shownFb, fb, sizeof(fb));
  pushFrame();
  if (tp < 1) memcpy(fb, liveFb, sizeof(fb));
}
