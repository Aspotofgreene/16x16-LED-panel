// Little wordless stories and the ever-changing "Dream" effect.
// Included from MatrixPanel.ino after the pixel and sprite helpers.

// ---- Story helpers ----------------------------------------------------------
float storyT;          // seconds into the current story (speed-scaled)
bool storyDone;        // the story has played through at least once

// Advances the story clock; returns true on the first frame of each run.
bool storyStart(float len) {
  bool start = fxFresh;
  if (fxFresh) storyT = 0, storyDone = false;
  else storyT += sdt();
  if (storyT >= len) {
    storyT = 0;
    storyDone = start = true;
  }
  return start;
}

// 0..1 progress of the story clock between a and b.
inline float seg(float a, float b) { return constrain((storyT - a) / (b - a), 0.0f, 1.0f); }
inline float ease(float f) { return f * f * (3 - 2 * f); }
inline float lerpf(float a, float b, float f) { return a + (b - a) * f; }

// Fades in at the start of a story and out at the end.
void storyFade(float len) {
  float f = min(1.0f, min(storyT / 0.7f, (len - storyT) / 0.8f));
  if (f < 1) {
    uint8_t v = max(0.0f, f) * 255;
    fadeAll(v, v, v);
  }
}

void rect(int x0, int y0, int x1, int y1, uint32_t c) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) setPx(x, y, c);
}

void ellipse(float cx, float cy, float rx, float ry, uint32_t c) {
  if (rx <= 0 || ry <= 0) return;
  for (int y = floorf(cy - ry); y <= ceilf(cy + ry); y++)
    for (int x = floorf(cx - rx); x <= ceilf(cx + rx); x++) {
      float dx = (x - cx) / rx, dy = (y - cy) / ry;
      if (dx * dx + dy * dy <= 1) setPx(x, y, c);
    }
}
inline void disc(float cx, float cy, float r, uint32_t c) { ellipse(cx, cy, r, r, c); }

void skyGradient(uint32_t top, uint32_t low, int rows) {
  for (int y = 0; y < rows; y++) {
    uint32_t c = lerpColor(top, low, y / (float)(rows - 1));
    for (int x = 0; x < W; x++) fb[y][x] = c;
  }
}

// Sprites with their own palette; rows must all be the same length.
struct Ink { char k; uint32_t c; };
void drawArt(const char *const *rows, int n, float fx, float fy, const Ink *pal, bool flip = false) {
  int ox = roundf(fx), oy = roundf(fy);
  for (int y = 0; y < n; y++) {
    int len = strlen(rows[y]);
    for (int x = 0; x < len; x++) {
      char ch = rows[y][flip ? len - 1 - x : x];
      for (const Ink *p = pal; p->k; p++)
        if (p->k == ch) {
          setPx(ox + x, oy + y, p->c);
          break;
        }
    }
  }
}

// ---- Garden: a seed is rained on, grows, blooms, meets a bee, then fades ---
void fxGarden() {
  const float LEN = 30;
  static float drops[6][2];
  if (storyStart(LEN))
    for (auto &d : drops) d[0] = rnd(5, 11), d[1] = rnd(-4, 3);
  float t = storyT;
  float sunset = seg(22, 27);
  skyGradient(lerpColor(lerpColor(0x000818, 0x0838A0, seg(0, 6)), 0x300850, sunset),
              lerpColor(lerpColor(0x001030, 0x4080D0, seg(0, 6)), 0xE04010, sunset), 13);
  // Sun comes up after the rain and sets at the end.
  if (t > 6) disc(13, lerpf(15, 2.5f, ease(seg(6, 10))) + 13 * ease(seg(23, 28.5f)), 1.6f, 0xFFC000);
  // Rain cloud drifts in, rains, and drifts away.
  float cx = -9 + 13 * ease(seg(2.5f, 4)) + 15 * ease(seg(6.5f, 8.5f));
  if (t > 4 && t < 6.6f)
    for (auto &d : drops) {
      d[1] += 11 * sdt();
      if (d[1] > 12.5f) d[0] = rnd(cx + 1, cx + 7), d[1] = 3;
      setPx(d[0], d[1], 0x3070FF);
    }
  if (t > 2.5f && t < 9) {
    ellipse(cx + 4, 2.2f, 4, 1.3f, 0x606070);
    ellipse(cx + 2.5f, 1.2f, 2, 1.2f, 0x707080);
    ellipse(cx + 5.5f, 1, 2, 1.1f, 0x808090);
  }
  // Soil.
  for (int y = 13; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = (x * 3 + y * 5) % 7 ? 0x281000 : 0x401800;
  // The seed falls and plants itself.
  if (t < 2.6f) setPx(7, lerpf(-1, 12.6f, ease(seg(0.3f, 2.2f))), 0xC08040);
  // Stem and leaves.
  const uint32_t GREEN = 0x10A020;
  float grow = ease(seg(8.5f, 14));
  if (grow > 0)
    for (int y = 12 - (int)roundf(grow * 7); y <= 12; y++) setPx(7, y, GREEN);
  if (t > 11) setPx(6, 10, GREEN), setPx(5, 9, GREEN);
  if (t > 12.5f) setPx(8, 8, GREEN), setPx(9, 7, GREEN);
  // The bud opens into a flower; petals drop one by one at the end.
  static const int8_t PETALS[][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1},
                                     {-1, 1}, {1, 1},  {0, -2}, {-2, 0}, {2, 0}};
  float open = ease(seg(14, 16.5f));
  if (t > 13.5f && open == 0) setPx(7, 4, 0x40C020);
  if (open > 0) {
    for (int i = 0; i < 11; i++) {
      if (open < (i < 4 ? 0.2f : i < 8 ? 0.5f : 0.85f)) continue;
      float px = 7 + PETALS[i][0], py = 4 + PETALS[i][1];
      float dt = t - (22 + i * 0.55f);
      if (dt > 0) {
        py = min(12.0f, py + dt * 2.2f);
        if (py < 12) px += 1.2f * sinf(dt * 3 + i);
      }
      setPx(roundf(px), roundf(py), i < 8 ? 0xFF2070 : 0xFF60A0);
    }
    if (t < 27.5f) setPx(7, 4, 0xFFD000);
  }
  // A bee visits the flower.
  if (t > 16.5f && t < 22.6f) {
    float bx = lerpf(-2, 9.5f, ease(seg(16.5f, 18.5f))) + 9 * ease(seg(20.5f, 22.5f));
    float by = 3 + 0.8f * sinf(t * 6) - 3 * ease(seg(20.5f, 22.5f));
    setPx(roundf(bx), roundf(by), 0xFFD000);
    setPx(roundf(bx) - 1, roundf(by), 0x302000);
    setPx(roundf(bx), roundf(by) - 1, (int)(t * 20) % 2 ? 0xB0B0FF : 0x505090);
    if (t > 19.2f && t < 20.4f) {  // a little heart
      setPx(9, 0, 0xFF1040), setPx(11, 0, 0xFF1040);
      setPx(9, 1, 0xFF1040), setPx(10, 1, 0xFF1040), setPx(11, 1, 0xFF1040);
      setPx(10, 2, 0xFF1040);
    }
  }
  storyFade(LEN);
}

// ---- Rocket: countdown, launch, a trip past a planet, landing on the moon --
const char *const ROCKET[] = {"..R..", ".RRR.", ".WWW.", ".WBW.", ".WWW.", ".WWW.", "RWWWR", "R...R"};

void fxRocket() {
  const float LEN = 26;
  static float scroll;
  static uint8_t starX[20], starY[20];
  if (storyStart(LEN)) {
    scroll = 0;
    for (int i = 0; i < 20; i++) starX[i] = random(W), starY[i] = random(H);
  }
  float t = storyT;
  // How fast the stars stream past: speeds up after launch, stops for landing.
  float speed = t < 5.9f ? 0 : t < 15 ? min(14.0f, (t - 5.9f) * 8) : 14 * (1 - seg(15, 18.5f));
  scroll += speed * sdt();
  fill(0x000010);
  for (int i = 0; i < 20; i++) {
    uint8_t v = 60 + 60 * sinf(tsec * 3 + i * 1.7f);
    setPx(starX[i], (int)(starY[i] + scroll * 0.5f) % H, rgb(v, v, v + 20));
  }
  // A ringed planet passes by in space.
  if (t > 9.5f && t < 15) {
    float py = -4 + (t - 9.5f) * 4.5f;
    disc(3, py, 2.4f, 0xA06020);
    for (int x = -1; x <= 7; x++) setPx(x, roundf(py + (x - 3) * 0.3f), 0xE0C080);
  }
  // Earth in the moon's sky.
  if (t > 16) {
    uint8_t v = 255 * seg(16, 18);
    disc(3, 3, 1.8f, scale(0x1040C0, v, v, v));
    setPx(2, 3, scale(0x20A040, v, v, v)), setPx(3, 2, scale(0x20A040, v, v, v));
  }
  // Launch pad, tower and countdown lights scroll away as we climb.
  int g = roundf(scroll);
  if (g < 4) {
    rect(0, 14 + g, W - 1, 15 + g, 0x303030);
    for (int y = 3; y <= 13; y++) setPx(12, y + g, y % 2 ? 0x505050 : 0x303030), setPx(13, y + g, 0x404040);
    for (int i = 0; i < 3; i++) {
      uint32_t c = t > 3.2f ? 0x00FF00 : t > 0.8f + i * 0.8f ? 0xFF0000 : 0x200000;
      setPx(14, 4 + i * 2 + g, c);
    }
  }
  // The moon rises to meet the rocket.
  float moonTop = lerpf(17, 13, ease(seg(15, 18.5f)));
  if (t > 15) {
    rect(0, moonTop, W - 1, H - 1, 0x505050);
    setPx(2, moonTop + 1, 0x282828), setPx(3, moonTop + 1, 0x282828), setPx(12, moonTop + 2, 0x282828);
  }
  // Where the rocket is on screen.
  float ry = t < 4.5f ? 6 : max(3.0f, 6 - 1.5f * (t - 4.5f) * (t - 4.5f));
  if (t > 15) ry = lerpf(3, moonTop - 8, ease(seg(15, 18.5f)));
  bool firing = (t > 3.5f && t < 15) || (t > 15 && t < 18.3f);
  if (firing) {
    int len = t < 4.5f ? 1 : t > 15 ? 1 + (int)(t * 10) % 2 : 2 + (int)(t * 13) % 3;
    for (int i = 0; i < len; i++) {
      setPx(7, ry + 8 + i, i == 0 ? 0xFFFF60 : 0xFF6000);
      if (i < len - 1) setPx(6 + (int)(t * 17 + i) % 3, ry + 8 + i, 0xFF2000);
    }
  }
  // Smoke billows across the pad at liftoff.
  if (t > 3.5f && t < 8) {
    float s = seg(3.5f, 6);
    uint8_t v = 120 * (1 - seg(6, 8));
    for (int i = 0; i < 6; i++)
      disc(7 + (i - 2.5f) * 2.5f * s, 13.5f + g, 0.8f + s, rgb(v, v, v));
  }
  static const Ink RKT[] = {{'R', 0xE01010}, {'W', 0xC0C0C0}, {'B', 0x2060FF}, {0, 0}};
  static const Ink OPEN[] = {{'R', 0xE01010}, {'W', 0xC0C0C0}, {'B', 0x000000}, {0, 0}};
  drawArt(ROCKET, 8, 5, ry, t > 19 ? OPEN : RKT);
  // An astronaut hops out and plants a flag.
  if (t > 19.5f) {
    float ax = lerpf(7, 10, ease(seg(19.5f, 20.7f)));
    if (t > 21.8f) ax = 9.5f + 0.8f * sinf((t - 21.8f) * 2);
    float hop = fabsf(sinf(t * 5)) * 1.5f;
    int ay = roundf(moonTop - 2 - hop);
    setPx(roundf(ax), ay, 0xFFFFFF);
    setPx(roundf(ax), ay + 1, 0xA0A0A0);
    float pole = ease(seg(20.7f, 21.6f)) * 4;
    for (int i = 1; i <= roundf(pole); i++) setPx(12, moonTop - i, 0x808080);
    if (pole > 3.5f) {
      setPx(13, moonTop - 4, 0xFF0000), setPx(14, moonTop - 4, 0xFFFFFF);
      setPx(13, moonTop - 3, 0xFFFFFF), setPx(14, moonTop - 3, 0x2040FF);
    }
  }
  storyFade(LEN);
}

// ---- Fish: little fish chased by bigger fish and a shark, then sweet revenge
const char *const SMALL_FISH[] = {"O.OO.", ".OOKO", "O.OO."};
const char *const BIG_FISH[] = {"...GG..", "G.GGGG.", ".GGGGKG", "G.GGGGG", "...GG.."};
const char *const SHARK[] = {
  "......S......", ".....SSS.....", "S...SSSSSSS..", "SS.SSSSSSSKSS", "S...LLLLLTLT.", "......L......",
};
const char *const SCHOOL[] = {
  "O....OOOOO...", "OO.OOOOOOOOO.", ".OOOOOOOOEOOO", ".OOOOOOOOOOOO", "OO.OOOOOOOOO.", "O....OOOOO...",
};

void fxFish() {
  const float LEN = 21;
  static float bub[5][2];
  if (storyStart(LEN))
    for (auto &b : bub) b[0] = rnd(0, W - 0.01f), b[1] = rnd(0, H);
  float t = storyT;
  skyGradient(0x002050, 0x000818, H);
  for (int x = 0; x < W; x++) setPx(x, 15, (x * 5) % 3 ? 0x302008 : 0x403010);
  // Swaying seaweed.
  for (int i = 0; i < 2; i++) {
    int base = i ? 13 : 2;
    for (int y = 0; y < 6; y++) setPx(base + roundf(0.8f * sinf(tsec * 2 + y * 0.7f + i * 2) * y / 5), 14 - y, 0x107010);
  }
  for (auto &b : bub) {
    b[1] -= 3 * sdt();
    b[0] += 0.4f * sinf(tsec * 4 + b[1]) * sdt();
    if (b[1] < -1) b[0] = rnd(0, W - 0.01f), b[1] = H;
    setPx(b[0], b[1], 0x305080);
  }
  static const Ink FISH[] = {{'O', 0xFF6000}, {'K', 0x000000}, {'G', 0x30C040}, {0, 0}};
  static const Ink SHK[] = {{'S', 0x506070}, {'L', 0x8090A0}, {'K', 0xFF0000}, {'T', 0xFFFFFF}, {0, 0}};
  float bob = sinf(tsec * 3);
  if (t < 11.5f) {  // The chase, left to right.
    drawArt(SMALL_FISH, 3, -5 + 3.4f * t, 7 + bob, FISH);
    if (t > 2) drawArt(BIG_FISH, 5, -8 + 4 * (t - 2), 6 - bob, FISH);
    if (t > 5) drawArt(SHARK, 6, -14 + 5 * (t - 5), 4 + 0.5f * bob, SHK);
  } else {  // Everyone flees from a huge school of little fish.
    drawArt(SHARK, 6, 17 - 9 * (t - 11.5f), 3 + 0.5f * bob, SHK, true);
    drawArt(BIG_FISH, 5, 17 - 8 * (t - 12.3f), 9 - bob, FISH, true);
    int sx = roundf(17 - 5 * (t - 13.5f)), sy = roundf(5 + bob);
    for (int y = 0; y < 6; y++)
      for (int x = 0; x < 13; x++) {
        char ch = SCHOOL[y][12 - x];
        if (ch == 'O') setPx(sx + x, sy + y, hsv(hueOf(5000 + 1500 * sinf(tsec * 6 + x * 1.3f + y * 2.1f)), 230, 220 + 35 * sinf(tsec * 9 + x + y)));
        else if (ch == 'E') setPx(sx + x, sy + y, 0x000000);
      }
  }
  storyFade(LEN);
}

// ---- Day and night: sun, clouds, sunset, stars, a shooting star (loops) -----
void fxDay() {
  const float LEN = 32;
  storyStart(LEN);
  float t = storyT;
  // Sky keyframes: time, top colour, horizon colour.
  static const struct { float t; uint32_t top, low; } KEYS[] = {
    {0, 0x200830, 0xD04010}, {3, 0x0838A0, 0x4080D0}, {13, 0x0838A0, 0x4080D0}, {16, 0x200840, 0xE03008},
    {18.5f, 0x000008, 0x000418}, {29.5f, 0x000008, 0x000418}, {32, 0x200830, 0xD04010},
  };
  int k = 0;
  while (k < 5 && t >= KEYS[k + 1].t) k++;
  float f = (t - KEYS[k].t) / (KEYS[k + 1].t - KEYS[k].t);
  skyGradient(lerpColor(KEYS[k].top, KEYS[k + 1].top, f), lerpColor(KEYS[k].low, KEYS[k + 1].low, f), H);
  float daylight = t < 16 ? min(1.0f, 0.3f + t / 3) : max(0.25f, 1 - (t - 16) / 2.5f);
  if (t > 29.5f) daylight = 0.25f + 0.75f * seg(29.5f, 32);
  float night = seg(16, 18.5f) * (1 - seg(29.5f, 31.5f));
  // Stars.
  for (int i = 0; i < 18; i++) {
    int x = (i * 7 + 3) % W, y = (i * 5 + i / 3) % 10;
    uint8_t v = night * (90 + 80 * sinf(tsec * 2.5f + i * 2.3f));
    if (v > 10) setPx(x, y, rgb(v, v, v));
  }
  // Shooting star.
  if (t > 23 && t < 24) {
    float p = t - 23;
    for (int i = 0; i < 4; i++) addPx(14 - p * 9 + i, 1 + p * 4 - i * 0.45f, scale(0xFFFFFF, 255 - i * 60, 255 - i * 60, 255 - i * 60));
  }
  // Sun by day, crescent moon by night, both arcing across the sky.
  if (t < 16.5f) {
    float p = t / 16;
    float sx = lerpf(-1, 17, p), sy = 12 - 10 * sinf(PI * p);
    disc(sx, sy, 1.5f, lerpColor(0xFF5000, 0xFFE040, min(1.0f, sinf(PI * p) * 2)));
  } else {
    float p = (t - 16) / 16;
    float mx = lerpf(-1, 17, p), my = 12 - 10 * sinf(PI * p);
    for (int y = my - 2; y <= my + 2; y++)
      for (int x = mx - 2; x <= mx + 2; x++) {
        float a = (x - mx) * (x - mx) + (y - my) * (y - my);
        float b = (x - mx - 1) * (x - mx - 1) + (y - my + 0.6f) * (y - my + 0.6f);
        if (a <= 2.4f && b > 1.6f) setPx(x, y, 0xC0B080);
      }
  }
  // Clouds drift by during the day.
  if (daylight > 0.3f) {
    uint8_t v = 110 * (daylight - 0.3f) / 0.7f;
    for (int i = 0; i < 2; i++) {
      float cx = fmodf(tsec * (1 + i * 0.4f) + i * 13, 26) - 5;
      ellipse(cx, 3 + i * 2.5f, 2.5f, 0.9f, rgb(v, v, v + 10));
    }
  }
  // Rolling hills, a tree and a house whose window lights up at night.
  uint8_t d = 255 * daylight;
  for (int x = 0; x < W; x++) {
    int top = 12 + roundf(1.2f * sinf(x * 0.45f));
    for (int y = top; y < H; y++) setPx(x, y, scale(y == top ? 0x30A020 : 0x186010, d, d, d));
  }
  setPx(3, 11, scale(0x402000, d, d, d)), setPx(3, 10, scale(0x402000, d, d, d));
  disc(3, 8.5f, 1.6f, scale(0x107018, d, d, d));
  rect(10, 9, 13, 12, scale(0x806050, d, d, d));
  rect(10, 8, 13, 8, scale(0x801010, d, d, d));
  rect(11, 7, 12, 7, scale(0x801010, d, d, d));
  setPx(12, 11, scale(0x301000, d, d, d)), setPx(12, 12, scale(0x301000, d, d, d));
  setPx(11, 10, night > 0.3f ? 0xFFB030 : scale(0x102040, d, d, d));
  // Fireflies at night.
  for (int i = 0; i < 3; i++) {
    float v = night * max(0.0f, sinf(tsec * (1.3f + i * 0.4f) + i * 2));
    if (v > 0.05f) setPx(roundf(5 + i * 2.5f + 1.5f * sinf(tsec * 0.7f + i)), roundf(9 + sinf(tsec + i * 3)), rgb(150 * v, 200 * v, 0));
  }
}

// ---- Cat and mouse: the cat pounces, misses, and gets dizzy ----------------
const char *const CAT[] = {"C.C......", "CCC.....C", "CEC.....C", "CCCCCCCC.", ".CCCCCCC.", ".C.C.C.C."};
const char *const MOUSE[] = {".MM..", "MMMMT"};
const char *const CHEESE[] = {".YY", "YhY"};

void fxCat() {
  const float LEN = 20;
  storyStart(LEN);
  float t = storyT;
  fill(0x140A1C);
  rect(0, 9, W - 1, 9, 0x241020);
  for (int x = 0; x < W; x++) setPx(x, 14, x % 4 ? 0x301808 : 0x200E04), setPx(x, 15, 0x281408);
  rect(0, 11, 1, 13, 0x000000);  // mouse hole
  setPx(0, 10, 0x000000);
  static const Ink CI[] = {{'C', 0xE07010}, {'E', 0x20FF20}, {0, 0}};
  static const Ink MI[] = {{'M', 0x8888A0}, {'T', 0xFF7090}, {0, 0}};
  static const Ink YI[] = {{'Y', 0xFFC000}, {'h', 0x806000}, {0, 0}};
  // Mouse: out of the hole, over to the cheese, then a dash home with it.
  float mx = -5;
  bool mouseRight = true, carrying = false;
  if (t < 9.3f) mx = lerpf(-5, 1, ease(seg(0.5f, 1.5f))) + (t > 3 && (int)(t * 3) % 2);  // sniffing
  if (t > 9.3f && t < 10) mx = lerpf(1, -6, seg(9.3f, 9.8f)), mouseRight = false, carrying = true;
  if (t > 12.5f && t < 15) mx = lerpf(-6, -2, ease(seg(12.5f, 13.3f))) - 4 * ease(seg(14.2f, 15)), carrying = true;
  if (t > 16.8f) mx = lerpf(-6, 3, ease(seg(16.8f, 18))), carrying = true;
  float my = 12 - (t > 18.2f ? fabsf(sinf(t * 7)) * 1.5f : 0);
  if (t < 9.3f) drawArt(CHEESE, 2, 6, 12, YI);
  drawArt(MOUSE, 2, mx, my, MI, mouseRight);
  if (carrying) drawArt(CHEESE, 2, mouseRight ? mx + 4 : mx - 2, my - 1, YI);
  // Cat: sneaks in, wiggles, pounces, gets dizzy, slinks off.
  float cx = 17, cy = 8;
  bool catRight = false;
  if (t > 3.5f) cx = lerpf(17, 9, seg(3.5f, 8));
  if (t > 8 && t < 9) cx = 9 + ((int)(t * 14) % 2);
  if (t >= 9) {
    float p = ease(seg(9, 10));
    cx = lerpf(9, 2, p);
    cy = 8 - 5 * sinf(PI * p);
  }
  if (t > 15) catRight = true, cx = 2 + 4.5f * (t - 15.5f > 0 ? t - 15.5f : 0);
  drawArt(CAT, 6, cx, cy, CI, catRight);
  if (t > 10 && t < 15) {  // dizzy stars
    for (int i = 0; i < 3; i++) {
      float a = t * 5 + i * 2.1f;
      setPx(roundf(3 + 2.2f * cosf(a)), roundf(6 + 0.9f * sinf(a)), 0xFFE020);
    }
  }
  storyFade(LEN);
}

// ---- Snowman: rolled, stacked, dressed, waves, then melts in the sun --------
void fxSnowman() {
  const float LEN = 28;
  static float flakes[12][2];
  if (storyStart(LEN))
    for (auto &f : flakes) f[0] = rnd(0, W - 0.01f), f[1] = rnd(-H, 0);
  float t = storyT;
  float sunny = seg(13.5f, 15.5f) * (1 - seg(23.5f, 25.5f));
  skyGradient(lerpColor(0x000818, 0x0838A0, sunny), lerpColor(0x081028, 0x4080D0, sunny), 13);
  if (sunny > 0) disc(13, lerpf(15, 2.5f, sunny), 1.6f, 0xFFD020);
  rect(0, 13, W - 1, H - 1, 0x606070);
  // Melting squashes everything down into a puddle.
  float m = 1 - 0.88f * ease(seg(17, 24));
  float roll = ease(seg(0, 3));
  float rb = lerpf(0.8f, 2.6f, roll), rbx = rb * (1 + 0.5f * (1 - m)), rby = max(0.6f, rb * m);
  float bx = lerpf(-3, 7, roll), by = 12.9f - rby;
  uint32_t snow = lerpColor(0xC0C0D0, 0x8090C0, 1 - m);
  ellipse(bx, by, rbx, rby, snow);
  float midY = by - rby - 1.9f * m + 0.4f;
  float midDrop = ease(seg(3, 4.5f)), headDrop = ease(seg(5, 6.5f));
  if (midDrop > 0) ellipse(7, lerpf(-3, midY, midDrop), 1.9f * (1 + 0.5f * (1 - m)), 1.9f * m, snow);
  float headY = midY - 1.9f * m - 1.5f * m + 0.4f;
  if (headDrop > 0) ellipse(7, lerpf(-3, headY, headDrop), 1.5f * (1 + 0.5f * (1 - m)), 1.5f * m, snow);
  if (t > 7) {
    bool upright = m > 0.45f;
    // Coal eyes and buttons, carrot nose, stick arms.
    int hy = roundf(headY), my = roundf(midY);
    if (upright) {
      setPx(6, hy - 1, 0x101010), setPx(8, hy - 1, 0x101010);
      setPx(7, hy, 0xFF6000), setPx(8, hy, 0xFF6000);
      setPx(7, my, 0x101010), setPx(7, my + 2, 0x101010);
      setPx(4, my, 0x502000), setPx(3, my - 1, 0x502000), setPx(2, my - 2, 0x502000);
      bool wave = t > 9 && t < 14 && (int)(t * 4) % 2;
      setPx(10, my, 0x502000), setPx(11, my - 1, 0x502000), setPx(12, wave ? my - 1 : my - 2, 0x502000);
    } else {  // everything lying in the puddle
      setPx(3, 12, 0x502000), setPx(4, 12, 0x502000), setPx(10, 12, 0x502000), setPx(11, 12, 0x502000);
      setPx(9, 12, 0xFF6000), setPx(6, 12, 0x101010), setPx(12, 12, 0x101010);
    }
    // The hat drops on last.
    float hatY = upright ? lerpf(-3, headY - 1.5f * m - 0.6f, ease(seg(7.5f, 8.5f))) : 11.3f;
    int hy2 = roundf(hatY);
    rect(6, hy2 - 1, 8, hy2 - 1, 0x402060);
    rect(5, hy2, 9, hy2, 0x402060);
  }
  // Snow falls before the sun comes out and again after it sets.
  if (t < 14 || t > 24.5f)
    for (auto &f : flakes) {
      f[1] += 3 * sdt();
      f[0] = fmodf(f[0] + 0.6f * sinf(tsec * 2 + f[1]) * sdt() + W, W);
      if (f[1] > 13) f[0] = rnd(0, W - 0.01f), f[1] = -1;
      if (f[1] >= 0) setPx(f[0], f[1], 0xD0D0E0);
    }
  storyFade(LEN);
}

// ---- Hatching: an egg wobbles, cracks, a chick pops out and follows mum ----
const char *const HEN[] = {".R....", "WKW..W", "YWWWWW", ".WWWW.", "..Y.Y."};
const char *const CHICK[] = {".KC.", "BCCC", ".CCC"};
const char *const CHICK_BLINK[] = {".CC.", "BCCC", ".CCC"};

void fxHatch() {
  const float LEN = 23;
  storyStart(LEN);
  float t = storyT;
  skyGradient(0x102050, 0x5090D0, 12);
  rect(0, 12, W - 1, H - 1, 0x187018);
  static const Ink HI[] = {{'R', 0xE01010}, {'W', 0xD0D0D0}, {'K', 0x101010}, {'Y', 0xFF9000}, {0, 0}};
  static const Ink CI[] = {{'C', 0xFFE000}, {'K', 0x101010}, {'B', 0xFF8000}, {0, 0}};
  // The egg wobbles in short bursts, harder as hatching gets closer.
  float wob = 0;
  if (t > 2 && t < 9.5f && fmodf(t, 1.6f) < 0.5f) wob = sinf(t * 30) * (0.4f + 0.06f * t);
  float ex = 7.5f + wob;
  const uint32_t SHELL = 0xE0E0C8, CRACK = 0x505050;
  bool open = t > 9.5f;
  // Lower half of the egg (the whole egg until it opens).
  for (int y = 6; y <= 13; y++)
    for (int x = 4; x <= 11; x++) {
      float dx = (x - ex) / 2.4f, dy = (y - 9.5f) / 3.1f;
      if (dx * dx + dy * dy <= 1 && (!open || y >= 9)) setPx(x, y, SHELL);
    }
  // The chick sits in the bottom half once the top pops off.
  bool hopped = t > 16.5f;
  if (open && !hopped) drawArt(fmodf(t, 2.5f) < 0.15f ? CHICK_BLINK : CHICK, 3, 6, 7 - ease(seg(9.5f, 10.2f)), CI);
  if (!open) {
    if (t > 5) setPx(roundf(ex) - 1, 9, CRACK), setPx(roundf(ex), 8, CRACK);
    if (t > 6.5f) setPx(roundf(ex) + 1, 9, CRACK), setPx(roundf(ex) - 2, 8, CRACK);
    if (t > 8) setPx(roundf(ex) + 2, 8, CRACK);
  } else if (t < 11.5f) {  // the top of the shell flies off
    float p = seg(9.5f, 11.5f);
    ellipse(7.5f - 7 * p, 7 - 8 * sinf(PI * p * 0.8f), 2, 1.2f, SHELL);
  }
  // Nest of straw in front of the egg.
  for (int y = 12; y <= 14; y++)
    for (int x = 2; x <= 13; x++) {
      float dx = (x - 7.5f) / 5.2f, dy = (y - 13) / 1.6f;
      if (dx * dx + dy * dy <= 1) setPx(x, y, (x + y * 2) % 3 ? 0x806020 : 0xB08830);
    }
  // Mum arrives, a heart, then the chick hops out and they walk off together.
  float hx = lerpf(17, 9.5f, ease(seg(12.5f, 15))) - 16 * seg(18, 22.5f);
  float walk = (t > 12.5f && t < 15) || t > 18 ? (int)(t * 5) % 2 : 0;
  if (t > 12.5f) drawArt(HEN, 5, hx, 7 - walk, HI);
  if (t > 15 && t < 16.5f) {
    for (int i = 0; i < 2; i++) setPx(8 + i * 2, 2, 0xFF2050);
    for (int x = 8; x <= 10; x++) setPx(x, 3, 0xFF2050);
    setPx(9, 4, 0xFF2050);
  }
  if (hopped) {
    float cx = lerpf(6, 4, seg(16.5f, 17.3f)) - 16 * seg(18.6f, 22.8f);
    float cy = lerpf(6, 9, seg(16.5f, 17.3f)) - 2.5f * sinf(PI * seg(16.5f, 17.3f)) - (t > 18.6f ? (int)(t * 6) % 2 : 0);
    drawArt(CHICK, 3, cx, cy, CI);
  }
  storyFade(LEN);
}

// ---- UFO: a flying saucer beams up a cow, then gives it back -------------
const char *const COW[] = {"......K.", "WWKWWWWW", "WKKWWWWP", "WWWWKW..", "W.W..W.W"};
const char *const QUESTION[] = {"XX.", "..X", ".X.", "...", ".X."};

void fxUfo() {
  const float LEN = 22;
  storyStart(LEN);
  float t = storyT;
  fill(0x000010);
  for (int i = 0; i < 14; i++) {
    uint8_t v = 50 + 50 * sinf(tsec * 2 + i * 1.9f);
    setPx((i * 11 + 2) % W, (i * 7) % 9, rgb(v, v, v + 20));
  }
  rect(0, 13, W - 1, H - 1, 0x0A3010);
  for (int x = 0; x < W; x += 3) setPx(x, 12, 0x104818);  // tufts of grass
  float ux = lerpf(-8, 7.5f, ease(seg(1.5f, 4.5f))), uy = 2 + 0.5f * sinf(tsec * 2);
  if (t > 10 && t < 12.5f) ux += 0.6f * sinf(t * 40);  // shaking while the cow is aboard
  if (t > 16.5f) ux += 14 * ease(seg(16.5f, 17.6f)), uy -= 8 * ease(seg(16.5f, 17.6f));
  // Tractor beam.
  float beam = seg(5, 5.5f) * (1 - seg(15.5f, 16));
  if (beam > 0)
    for (int y = uy + 1; y < 13; y++) {
      float half = 1.5f + (y - uy) * 0.3f;
      for (int x = ux - half; x <= ux + half; x++) addPx(x, y, scale(0x0C3014, beam * 255, beam * 255, beam * 255));
    }
  // The cow: grazing, floating up, inside the saucer, floating down (now with an antenna).
  float cy = 8;
  if (t > 6) cy = lerpf(8, 1.5f, ease(seg(6, 10)));
  if (t > 12.5f) cy = lerpf(1.5f, 8, ease(seg(12.5f, 15.5f)));
  bool aboard = t > 10 && t < 12.5f;
  bool lookBack = t > 17.5f && (int)(t * 1.2f) % 2;
  static const Ink CW[] = {{'W', 0xC0C0C0}, {'K', 0x181818}, {'P', 0xFF80A0}, {0, 0}};
  if (!aboard) {
    float graze = t < 5 ? (int)(t * 2) % 2 : 0;
    drawArt(COW, 5, 4, cy, CW, lookBack);
    if (t < 5 && graze) setPx(11, cy + 3, 0xC0C0C0);  // head dips to the grass
    if (t > 12.5f) setPx(lookBack ? 5 : 10, cy - 1, (int)(t * 4) % 2 ? 0x40FF40 : 0x108010);
  }
  // Saucer: glass dome, metal disc, chasing lights.
  ellipse(ux, uy - 1, 1.6f, 1.2f, 0x3080C0);
  ellipse(ux, uy + 0.4f, 4.5f, 1, 0x707880);
  static const uint32_t LIGHTS[] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00A0FF};
  int spin = (int)(t * (aboard ? 20 : 6));
  for (int i = 0; i < 3; i++) setPx(roundf(ux - 3 + i * 3), roundf(uy + 0.4f), LIGHTS[(spin + i) % 4]);
  static const Ink WHITE[] = {{'X', 0xFFFFFF}, {0, 0}};
  if (t > 18.2f && t < 21) drawArt(QUESTION, 5, lookBack ? 1 : 12, 1, WHITE);
  storyFade(LEN);
}

// ---- Rainbow: a walk in the rain, an umbrella, then the sun and a rainbow --
const char *const PERSON[] = {".H.", "SSS", ".S.", "L.L"};
const char *const PERSON_STEP[] = {".H.", "SSS", ".S.", ".L."};

void fxShower() {
  const float LEN = 23;
  static float drops[14][2];
  if (storyStart(LEN))
    for (auto &d : drops) d[0] = rnd(0, W - 0.01f), d[1] = rnd(-H, 0);
  float t = storyT;
  float clear = seg(12, 15);
  skyGradient(lerpColor(0x101420, 0x0838A0, clear), lerpColor(0x202838, 0x4080D0, clear), 14);
  if (clear > 0) disc(1.5f, lerpf(-3, 2, clear), 1.6f, 0xFFD020);
  // Rainbow sweeps in from the left.
  float reveal = ease(seg(13, 16));
  static const uint32_t BANDS[] = {0xC00000, 0xC05000, 0xA0A000, 0x00A000, 0x0040C0, 0x6000A0};
  if (reveal > 0)
    for (int y = 0; y < 14; y++)
      for (int x = 0; x < W; x++) {
        float dx = x - 7.5f, dy = y - 16, d = sqrtf(dx * dx + dy * dy);
        int band = (int)(13.5f - d);
        if (band >= 0 && band < 6 && atan2f(-dy, dx) >= PI * (1 - reveal)) setPx(x, y, BANDS[band]);
      }
  // Clouds darken for the rain, then drift away.
  float drift = 18 * ease(seg(12, 15));
  uint32_t cloud = lerpColor(0x606878, 0x303440, seg(2, 4));
  ellipse(4 + drift, 1, 4, 1.6f, cloud);
  ellipse(11 + drift, 1.5f, 4.5f, 1.8f, cloud);
  rect(0, 14, W - 1, H - 1, 0x186018);
  // Puddles fill up in the rain.
  float puddle = seg(4, 9) * (1 - seg(16, 20));
  if (puddle > 0)
    for (int x : {3, 4, 11, 12, 13}) setPx(x, 14, lerpColor(0x186018, 0x3060C0, puddle));
  // Our walker.
  float px = lerpf(-3, 6, seg(0, 5));
  bool walking = t < 5;
  float jump = t > 16.5f ? fabsf(sinf((t - 16.5f) * 5)) * 2 : 0;
  int py = roundf(10 - jump);
  static const Ink WALKER[] = {{'H', 0xF0B080}, {'S', 0x2060E0}, {'L', 0x403020}, {0, 0}};
  drawArt(walking && (int)(t * 5) % 2 ? PERSON_STEP : PERSON, 4, px, py, WALKER);
  if (t > 16.5f) setPx(px, py, 0x2060E0), setPx(px + 2, py, 0x2060E0);  // arms up!
  // Umbrella opens when the rain starts and closes when it stops.
  float umb = seg(3.6f, 4.2f) * (1 - seg(15, 15.5f));
  int ux = roundf(px) + 1;
  if (umb > 0) {
    int w = roundf(umb * 2);
    for (int x = ux - w; x <= ux + w; x++) setPx(x, py - 2, 0xE01010);
    for (int x = ux - w + 1; x <= ux + w - 1; x++) setPx(x, py - 3, 0xE01010);
    setPx(ux, py - 1, 0x606060);
  }
  // Rain, which runs off the umbrella.
  if (t > 3 && t < 12.5f)
    for (auto &d : drops) {
      d[1] += 12 * sdt();
      int dx = d[0], dy = d[1];
      bool hitsUmbrella = umb > 0.5f && abs(dx - ux) <= 2 && dy >= py - 3;
      if (dy >= 14 || hitsUmbrella) d[0] = rnd(0, W - 0.01f), d[1] = rnd(-4, 0);
      else if (dy >= 2) setPx(dx, dy, 0x4070D0);
    }
  storyFade(LEN);
}

// ---- Volcano: smoke, an eruption, lava cools, and the island turns green ---
void fxVolcano() {
  const float LEN = 22;
  static struct { float x, y, vx, vy; } lava[20];
  if (storyStart(LEN))
    for (auto &l : lava) l = {9, 20, 0, 0};
  float t = storyT;
  float erupt = seg(6, 7) * (1 - seg(13, 16));
  skyGradient(lerpColor(0x0838A0, 0x200408, erupt), lerpColor(0x4080D0, 0x902008, erupt), 14);
  // Smoke puffs rise from the crater, then a big dark cloud.
  if (t < 7)
    for (int i = 0; i < 3; i++) {
      float p = fmodf(t * 0.6f + i / 3.0f, 1);
      uint8_t v = 90 * (1 - p) * seg(1, 2);
      disc(9 + 1.5f * sinf(t + i * 2), 4 - 7 * p, 0.6f + 1.2f * p, rgb(v, v, v));
    }
  if (erupt > 0) {
    uint8_t v = 60 * erupt;
    ellipse(9, 1, 6 * erupt, 2, rgb(v, v * 0.8f, v * 0.8f));
  }
  // Sea and sand.
  for (int y = 14; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = (x + y + (int)(tsec * 3)) % 5 ? 0x0838A0 : 0x3070D0;
  rect(0, 13, W - 1, 13, 0x907040);
  // The volcano itself, glowing at the top before it blows.
  for (int y = 5; y <= 12; y++) {
    float half = (y - 5) * 0.7f + 0.6f;
    for (int x = ceilf(9 - half); x <= floorf(9 + half); x++) setPx(x, y, 0x4A2A18);
  }
  uint8_t glow = 255 * max(seg(4.5f, 6), erupt);
  setPx(9, 5, scale(0xFF3000, glow, glow, glow));
  // Lava runs down both slopes, then cools to dark rock, then plants grow.
  float flow = 8 * ease(seg(6.5f, 10));
  uint32_t lavaC = lerpColor(t > 6.5f ? (int)(t * 8) % 2 ? 0xFF6000 : 0xFF3000 : 0, 0x282020, seg(12, 16));
  for (int i = 0; i < flow; i++) {
    int y = 5 + i;
    float half = i * 0.7f + 0.6f;
    setPx(roundf(9 - half), y, lavaC), setPx(roundf(9 + half), y, lavaC);
  }
  float green = seg(16, 19);
  static const int8_t SPROUTS[][2] = {{5, 12}, {7, 11}, {8, 9}, {10, 10}, {12, 12}, {13, 11}};
  for (int i = 0; i < 6; i++)
    if (green > i / 6.0f) setPx(SPROUTS[i][0], SPROUTS[i][1], 0x20A020);
  // Palm tree on the beach.
  setPx(1, 12, 0x603010), setPx(1, 11, 0x603010), setPx(2, 10, 0x603010);
  for (int x = 0; x <= 3; x++) setPx(x, 9, 0x109020);
  setPx(2, 8, 0x109020), setPx(0, 10, 0x109020), setPx(3, 10, 0x109020);
  // Lava bombs.
  for (auto &l : lava) {
    if (l.y > 13 && t > 6 && t < 11.5f) l = {9, 5, rnd(-4, 4), rnd(-11, -6)};
    if (l.y > 13) continue;
    l.vy += 14 * sdt();
    l.x += l.vx * sdt(), l.y += l.vy * sdt();
    setPx(roundf(l.x), roundf(l.y), l.vy < 0 ? 0xFFD040 : 0xFF5000);
  }
  // A bird flies by once all is calm.
  if (t > 17 && t < 21.5f) {
    float bx = lerpf(-2, 17, seg(17, 21.5f)), by = 3 + sinf(t * 2);
    bool up = (int)(t * 6) % 2;
    setPx(bx, by, 0xE0E0E0), setPx(bx - 1, by - up, 0xE0E0E0), setPx(bx + 1, by - up, 0xE0E0E0);
  }
  storyFade(LEN);
}

// ---- Fetch: a dog chases a ball, brings it back, then makes a flying catch -
const char *const DOG[] = {"....HE.", ".BBBHHN", ".BBBB..", ".B..B.."};
const char *const DOG_RUN[] = {"....HE.", ".BBBHHN", ".BBBB..", "..BB..."};

void fxFetch() {
  const float LEN = 21;
  storyStart(LEN);
  float t = storyT;
  skyGradient(0x0838A0, 0x4080D0, 14);
  ellipse(fmodf(tsec * 0.8f, 24) - 4, 2, 3, 1, 0x707078);
  rect(0, 14, W - 1, H - 1, 0x208020);
  static const Ink KID[] = {{'H', 0xF0B080}, {'S', 0xE03030}, {'L', 0x303070}, {0, 0}};
  static const Ink DI[] = {{'B', 0xA06030}, {'H', 0xB07040}, {'E', 0x101010}, {'N', 0x101010}, {0, 0}};
  // The kid, who throws and later jumps for joy.
  float kidJump = t > 17 ? fabsf(sinf((t - 17) * 6)) * 1.5f : 0;
  int ky = roundf(10 - kidJump);
  drawArt(PERSON, 4, 1, ky, KID);
  bool armUp = (t > 1.6f && t < 2.2f) || (t > 12.6f && t < 13.2f) || t > 17;
  if (armUp) setPx(3, ky - 1, 0xF0B080);
  // Where the dog is and which way it faces.
  float dx = 6, dy = 10;
  bool left = false, running = false;
  if (t > 2.4f && t < 4.6f) dx = lerpf(6, 18, seg(2.4f, 4.6f)), running = true;
  else if (t >= 4.6f && t < 6.5f) dx = 30;  // off chasing the ball
  else if (t >= 6.5f && t < 9.5f) dx = lerpf(17, 5, seg(6.5f, 9.5f)), left = running = true;
  else if (t >= 9.5f && t < 12.5f) dx = 5, left = true;
  else if (t >= 12.5f && t < 16) {  // second throw: a running jump for the catch
    dx = lerpf(5, 6, seg(13.5f, 14.5f));
    dy = 10 - 5 * sinf(PI * seg(14.4f, 15.6f));
    running = t > 13.5f && t < 14.5f;
  } else if (t >= 16) dx = lerpf(6, 5, seg(16, 17)), left = true;
  if (dx < 20) {
    drawArt(running && (int)(t * 8) % 2 ? DOG_RUN : DOG, 4, dx, dy, DI, left);
    bool wag = (int)(t * 10) % 2;
    setPx(left ? dx + 6 : dx, dy + (wag ? 0 : 1), 0xA06030);
  }
  // The ball: thrown, carried back, dropped, thrown high, caught.
  int nose = left ? roundf(dx) : roundf(dx) + 6;
  uint32_t BALL = 0xFF2020;
  if (t < 1.6f) setPx(3, 10, BALL);
  else if (t < 3.6f) {
    float p = seg(2, 3.6f);
    setPx(lerpf(3, 18, p), 9 - 6 * sinf(PI * p), BALL);
  } else if (t > 6.5f && t < 9.5f) setPx(nose, dy + 2, BALL);
  else if (t >= 9.5f && t < 12.3f) setPx(4, 13, BALL);
  else if (t >= 12.3f && t < 13) setPx(3, ky - 1, BALL);
  else if (t >= 13 && t < 15) {
    float p = seg(13, 15);
    setPx(lerpf(3, 11.5f, p), lerpf(9, 5, p) - 7 * sinf(PI * p), BALL);
  } else if (t >= 15) setPx(nose, dy + 2, BALL);
  // A happy heart between them.
  if (t > 10 && t < 12) {
    setPx(3, 6, 0xFF2050), setPx(5, 6, 0xFF2050);
    for (int x = 3; x <= 5; x++) setPx(x, 7, 0xFF2050);
    setPx(4, 8, 0xFF2050);
  }
  storyFade(LEN);
}

// ============================================================================
// Retro arcade stories: little "attract mode" games that play themselves.
// ============================================================================

// A 3x5 pixel font for scores and short words. Each glyph is five rows of
// three bits, written as octal digits (top row first).
uint16_t tinyGlyph(char ch) {
  static const uint16_t DIG[10] = {075557, 026227, 071747, 071717, 055711,
                                   074717, 074757, 071122, 075757, 075717};
  if (ch >= '0' && ch <= '9') return DIG[ch - '0'];
  switch (ch) {
    case 'W': return 055775;
    case 'I': return 072227;
    case 'N': return 057755;
    case 'G': return 074557;
    case 'O': return 075557;
    case '!': return 022202;
    case 'P': return 075744;
  }
  return 0;
}
void tinyText(const char *s, int x, int y, uint32_t c) {
  for (; *s; s++, x += 4) {
    uint16_t g = tinyGlyph(*s);
    for (int r = 0; r < 5; r++)
      for (int k = 0; k < 3; k++)
        if (g >> (3 * (4 - r) + 2 - k) & 1) setPx(x + k, y + r, c);
  }
}

// A compact 12x5 "WIN", centred so it stays clear of the edges.
void winText(int y, uint32_t c) {
  static const char *const ROWS[] = {"#...#.#.#..#", "#...#.#.##.#", "#.#.#.#.#.##", "#.#.#.#.#..#", ".#.#..#.#..#"};
  for (int r = 0; r < 5; r++)
    for (int x = 0; x < 12; x++)
      if (ROWS[r][x] == '#') setPx(2 + x, y + r, c);
}

// ---- Turbo Road: countdown, weave through traffic, take the trophy --------
const char *const RACE_CAR[] = {"YRY", "RCR", "RRR", "RRR"};
const char *const RACE_TRAFFIC[] = {"BBB", "BCB", "BBB", "RBR"};
const char *const RACE_TROPHY[] = {"YYYYY", "YYYYY", ".YYY.", "..Y..", ".YYY."};

float raceDist(float t) {
  if (t < 3) return 0;
  if (t < 5) return 3.5f * (t - 3) * (t - 3);
  if (t < 17) return 14 + 14 * (t - 5);
  float u = min(t - 17, 2.0f);
  return 182 + 14 * u - 3.5f * u * u;
}

void fxRace() {
  const float LEN = 22;
  storyStart(LEN);
  float t = storyT, d = raceDist(t);
  // Grass with trees, kerbs, road and lane dashes, all scrolling down.
  for (int y = 0; y < H; y++) {
    float wy = y - d;
    uint32_t grass = (int)floorf(pmod(wy, 8) / 4) ? 0x1A8A1A : 0x22A022;
    for (int x = 0; x < W; x++) setPx(x, y, grass);
    bool red = pmod(wy, 4) < 2;
    setPx(3, y, red ? 0xE02020 : 0xE0E0E0), setPx(12, y, red ? 0xE02020 : 0xE0E0E0);
    for (int x = 4; x <= 11; x++) setPx(x, y, 0x26262E);
    if (pmod(wy, 6) < 3) setPx(7, y, 0x808080), setPx(8, y, 0x808080);
    if (pmod(wy, 9) < 2) setPx(0, y, 0x0A4A0A), setPx(1, y, 0x0A5A0A);
    if (pmod(wy + 4, 9) < 2) setPx(14, y, 0x0A5A0A), setPx(15, y, 0x0A4A0A);
  }
  // Start and finish lines.
  for (int line = 0; line < 2; line++) {
    int ly = roundf(line ? 10 - (182 - d) : 8 + d);
    for (int r = 0; r < 2; r++)
      for (int x = 3; x <= 12; x++) setPx(x, ly + r, (x + r) % 2 ? 0xFFFFFF : 0x101010);
  }
  // Traffic: each car is overtaken at its pass time.
  struct Rival { float pass; int8_t x; uint32_t c; };
  static const Rival RIVALS[] = {{3, 4, 0xFFD020},    {7, 4, 0x2060FF},      {9.5f, 9, 0x20D0C0},
                                 {11.5f, 4, 0xFF60C0}, {13.5f, 9, 0xF0F0F0}, {15.3f, 4, 0xFF8020}};
  for (auto &r : RIVALS) {
    float pos = raceDist(r.pass) - 6 * (r.pass - 3) + 6 * max(0.0f, t - 3);
    float y = 10 - (pos - d);
    if (y < -5 || y > 16) continue;
    Ink pal[] = {{'B', r.c}, {'C', 0x60A0FF}, {'R', 0xFF0000}, {0, 0}};
    drawArt(RACE_TRAFFIC, 4, r.x, y, pal);
  }
  // The player's car changes lane between overtakes.
  struct Swerve { float a, b; int8_t to; };
  static const Swerve SWERVES[] = {{8.05f, 8.45f, 4}, {10.3f, 10.7f, 9}, {12.3f, 12.7f, 4}, {14.2f, 14.5f, 9}};
  float px = 9;
  for (auto &s : SWERVES) {
    if (t >= s.b) px = s.to;
    else if (t > s.a) px = lerpf(px, s.to, ease((t - s.a) / (s.b - s.a)));
  }
  static const Ink CARI[] = {{'R', 0xFF2020}, {'Y', 0xFFFF80}, {'C', 0x60A0FF}, {0, 0}};
  int shake = t < 3 && (int)(t * 14) % 2;
  drawArt(RACE_CAR, 4, px, 10 + shake, CARI);
  if (t < 3.6f && (int)(t * 10) % 3) setPx(roundf(px) + 1, 14 + (int)(t * 7) % 2, 0x606060);  // exhaust
  // Countdown lights.
  if (t < 3) {
    char buf[2] = {char('3' - (int)t), 0};
    tinyText(buf, 6, 2, t < 2 ? 0xFF2020 : 0xFFD020);
  } else if (t < 4) tinyText("GO", 4, 2, 0x30FF30);
  // Confetti and the trophy after the finish.
  if (t > 17) {
    for (int i = 0; i < 18; i++) {
      float y = -2 - fwHash(i) * 12 + (t - 17) * (5 + fwHash(i + 40) * 4);
      float x = fwHash(i + 80) * 16 + sinf(t * 3 + i) * 0.8f;
      setPx(roundf(x), roundf(y), hsv(fwHash(i + 120) * 65535));
    }
  }
  if (t > 18.5f) {
    Ink cup[] = {{'Y', (int)(t * 4) % 4 ? 0xFFB000 : 0xFFF0A0}, {0, 0}};
    drawArt(RACE_TROPHY, 5, 5, lerpf(-6, 2, ease(seg(18.5f, 19.3f))), cup);
  }
  storyFade(LEN);
}

// ---- Brick Buster: paddle and ball, with a multi-ball power-up -------------
struct BkBall { float x, y, vx, vy; bool on; } bkBalls[3];
struct BkBit { float x, y, vx, vy, age; uint32_t c; } bkBits[24];
uint8_t bkBricks[4][7];
int bkLeft, bkHit, bkNextBit;
float bkPaddle, bkCapX, bkCapY, bkClear, bkServe, bkCascade;
bool bkCapOn, bkCapUsed;
const uint32_t BK_COLORS[4] = {0xFF3030, 0xFF9020, 0xFFE020, 0x30E040};

void bkBurst(float x, float y, uint32_t c, int n) {
  for (int i = 0; i < n; i++) {
    BkBit &b = bkBits[bkNextBit++ % 24];
    b = {x, y, rnd(-4, 4), rnd(-4, 1), 0, c};
  }
}
void bkBreak(int r, int c) {
  bkBricks[r][c] = 0;
  bkLeft--, bkHit++;
  bkBurst(2 + c * 2, 2 + r, BK_COLORS[r], 3);
  if (bkHit == 6 && !bkCapUsed) bkCapOn = true, bkCapUsed = true, bkCapX = 2 + c * 2, bkCapY = 2 + r;
}
bool bkBrickAt(int x, int y) {
  return y >= 2 && y <= 5 && x >= 1 && x <= 14 && bkBricks[y - 2][(x - 1) / 2];
}
void bkServeBall() {
  bkBalls[0] = {bkPaddle + 2, 13, 0, 0, true};
  bkBalls[1].on = bkBalls[2].on = false;
  bkServe = storyT + 1.2f;
}
void bkLaunch(BkBall &b, float angle) {
  b.vx = 10 * sinf(angle), b.vy = -10 * cosf(angle);
}

void fxBricks() {
  const float LEN = 30;
  if (storyStart(LEN)) {
    memset(bkBricks, 1, sizeof(bkBricks));
    bkLeft = 28, bkHit = 0, bkPaddle = 6, bkClear = -1, bkCascade = 0;
    bkCapOn = bkCapUsed = false;
    for (auto &b : bkBits) b.age = 9;
    bkServeBall();
  }
  float dt = min(sdt(), 0.05f);
  float t = storyT;
  bool won = bkClear >= 0;
  // Running out of time: the remaining bricks pop one after another.
  if (!won && t > LEN - 8 && bkLeft > 0) {
    bkCascade += dt;
    while (bkCascade > 0.12f && bkLeft > 0) {
      bkCascade -= 0.12f;
      for (int i = 0; i < 28; i++)
        if (bkBricks[i / 7][i % 7]) {
          bkBreak(i / 7, i % 7);
          break;
        }
    }
  }
  if (!won && bkLeft == 0) {
    bkClear = t, won = true;
    if (storyT < LEN - 5.5f) storyT = t = LEN - 5.5f;
    for (auto &b : bkBalls) b.on = false;
    bkCapOn = false;
  }
  // Balls.
  if (!won) {
    if (bkServe > 0 && t < bkServe) bkBalls[0].x = bkPaddle + 2, bkBalls[0].y = 13;
    else if (bkServe > 0) bkLaunch(bkBalls[0], rnd(-0.5f, 0.5f)), bkServe = 0;
    for (int s = 0; s < 4; s++)
      for (auto &b : bkBalls) {
        if (!b.on || bkServe > 0) continue;
        float h = dt / 4, nx = b.x + b.vx * h, ny = b.y + b.vy * h;
        if (nx < 1) nx = 2 - nx, b.vx = fabsf(b.vx);
        if (nx >= 15) nx = 29.98f - nx, b.vx = -fabsf(b.vx);
        if (ny < 1) ny = 2 - ny, b.vy = fabsf(b.vy);
        if (bkBrickAt(b.x, ny)) bkBreak((int)ny - 2, ((int)b.x - 1) / 2), b.vy = -b.vy, ny = b.y;
        else if (bkBrickAt(nx, b.y)) bkBreak((int)b.y - 2, ((int)nx - 1) / 2), b.vx = -b.vx, nx = b.x;
        else if (bkBrickAt(nx, ny)) bkBreak((int)ny - 2, ((int)nx - 1) / 2), b.vx = -b.vx, b.vy = -b.vy, nx = b.x, ny = b.y;
        if (b.vy > 0 && ny >= 14 && b.y < 14 && nx >= bkPaddle - 0.6f && nx <= bkPaddle + 4.6f) {
          float off = constrain((nx - (bkPaddle + 2)) / 2.5f + rnd(-0.15f, 0.15f), -1.0f, 1.0f);
          bkLaunch(b, off);
          ny = 13.99f;
        }
        b.x = nx, b.y = ny;
        if (b.y > 16) b.on = false;
      }
    if (!bkBalls[0].on && !bkBalls[1].on && !bkBalls[2].on) bkServeBall();
  }
  // The power-up capsule: catch it for three balls.
  if (bkCapOn) {
    bkCapY += dt * 5;
    if (bkCapY >= 13.5f && bkCapX >= bkPaddle - 1 && bkCapX <= bkPaddle + 4) {
      bkCapOn = false;
      for (int i = 0; i < 3; i++)
        if (!bkBalls[i].on) bkBalls[i] = {bkPaddle + 2, 13, 0, 0, true}, bkLaunch(bkBalls[i], i == 1 ? -0.6f : 0.6f);
      bkBurst(bkPaddle + 2, 13, 0xFF40FF, 5);
    } else if (bkCapY > 16) bkCapOn = false;
  }
  // The paddle plays itself: follow the lowest falling ball, or the capsule.
  float target = bkPaddle + 2;
  float lowest = -1;
  for (auto &b : bkBalls)
    if (b.on && b.vy > 0 && b.y > lowest) lowest = b.y, target = b.x + 1.2f * sinf(t * 0.9f);
  if (bkCapOn && lowest < 8) target = bkCapX;
  if (bkServe > 0) target = 7.5f + 3 * sinf(t * 2);
  if (won) target = 7.5f;
  float step = 16 * dt;
  bkPaddle += constrain(target - 2 - bkPaddle, -step, step);
  bkPaddle = constrain(bkPaddle, 1.0f, 11.0f);
  // Draw.
  fill(0);
  for (int x = 0; x < W; x++) setPx(x, 0, 0x404058);
  for (int y = 0; y < H; y++) setPx(0, y, 0x404058), setPx(15, y, 0x404058);
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 7; c++)
      if (bkBricks[r][c]) setPx(1 + c * 2, 2 + r, BK_COLORS[r]), setPx(2 + c * 2, 2 + r, scale(BK_COLORS[r], 170, 170, 170));
  for (auto &b : bkBits) {
    if (b.age > 0.7f) continue;
    b.age += dt, b.vy += 20 * dt, b.x += b.vx * dt, b.y += b.vy * dt;
    setPx(roundf(b.x), roundf(b.y), lerpColor(b.c, 0, b.age / 0.7f));
  }
  if (bkCapOn) {
    uint32_t c = (int)(t * 8) % 2 ? 0xFF40FF : 0xFFFFFF;
    setPx(roundf(bkCapX) - 1, roundf(bkCapY), c), setPx(roundf(bkCapX), roundf(bkCapY), c);
  }
  for (auto &b : bkBalls)
    if (b.on) setPx(roundf(b.x - 0.5f), roundf(b.y - 0.5f), 0xFFFFFF);
  int p = roundf(bkPaddle);
  for (int x = 0; x < 4; x++) setPx(p + x, 14, won ? hsv((uint16_t)(t * 30000) + x * 8000) : (x == 0 || x == 3 ? 0x8080FF : 0x20C0FF));
  // Victory fireworks.
  if (won) {
    if (rnd(0, 1) < dt * 10) bkBurst(rnd(3, 13), rnd(2, 9), hsv(random(65536)), 4);
    if ((int)(t * 3) % 2 == 0) winText(6, 0xFFE020);
  }
  storyFade(LEN);
}

// ---- One Punch: a giant monster attacks the city; a bald hero in a yellow
// suit and cape strolls up, waits, and ends it with a single punch ----------
const char *const HERO[] = {"..HHH..", ".HHHHH.", ".HEHEH.", "..HHH..", ".CYYYC.",
                            "CYYYYYC", "CRBBBRC", "C.YYY.C", "..Y.Y..", "..R.R.."};
const char *const HERO_STEP[] = {"..HHH..", ".HHHHH.", ".HEHEH.", "..HHH..", ".CYYYC.",
                                 "CYYYYYC", "CRBBBRC", "C.YYY.C", ".Y...Y.", ".R...R."};
const char *const HERO_PUNCH[] = {"..HHH..", ".HHHHH.", ".HEHEH.", "..HHH..", ".CYYYC.",
                                  "CYYYYYY", "CRBBB.C", "C.YYY.C", ".Y...Y.", ".R...R."};
const char *const BEAST[] = {"H.......H", ".HMMMMMH.", ".MMMMMMM.", "MMRMMMRMM", "MMMMMMMMM", "MMMMMMMMM",
                             "MWMWMWMWM", "AMMMMMMMA", "AMMMMMMMA", ".MMM.MMM.", ".MM...MM."};
const char *const BEAST_ROAR[] = {"H.......H", ".HMMMMMH.", ".MMMMMMM.", "MMRMMMRMM", "MMMMMMMMM", "MWMWMWMWM",
                                  "MDDDDDDDM", "AMWMWMWMA", "AMMMMMMMA", ".MMM.MMM.", ".MM...MM."};

void fxPunch() {
  const float LEN = 18.5f;
  static struct { float x, y, vx, vy; uint32_t c; } bits[22];
  static bool blown;
  if (storyStart(LEN)) {
    for (auto &b : bits) b.y = 99;
    blown = false;
  }
  float t = storyT, dt = sdt();
  // Dusk sky, drifting clouds (with a hole blasted through them after the punch).
  skyGradient(0x100420, 0x902C10, 14);
  float hole = 6.5f * ease(seg(10.15f, 11.2f));
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < W; x++) {
      float d = sinf(x * 0.55f + tsec * 0.3f + y * 0.8f) + sinf(x * 0.23f - tsec * 0.17f + y * 1.7f) + 1.2f - y * 0.45f;
      float hx = x - 12, hy = (y - 1) * 1.3f, r = sqrtf(hx * hx + hy * hy);
      if (r < hole) {  // clear blue sky through the hole
        fb[y][x] = lerpColor(0x2050C0, 0x5090E0, y / 4.0f);
        continue;
      }
      if (d < 0) continue;
      float rim = hole > 0 && r < hole + 1.3f ? 0.7f * (1 - seg(12, 15)) : 0;
      fb[y][x] = lerpColor(lerpColor(fb[y][x], 0x3A2238, min(1.0f, d * 0.9f)), 0xFFC080, rim);
    }
  // City skyline with lit windows, and the street.
  for (int x = 0; x < W; x++) {
    int top = 6 + (int)(fwHash(x / 3 + 50) * 4);
    for (int y = top; y < 14; y++)
      fb[y][x] = x % 3 != 2 && y > top && fwHash(x * 17 + y * 5) > 0.84f ? 0x483008 : 0x0A0510;
  }
  rect(0, 14, W - 1, 15, 0x18141A);
  for (int x = 0; x < W; x += 4) setPx(x + 1, 15, 0x403840);

  // The monster stomps in, roars, raises its fists... and is gone in one hit.
  static const Ink BI[] = {{'H', 0xE0D0B0}, {'M', 0x7020A0}, {'R', 0xFF2000}, {'W', 0xFFFFFF},
                           {'D', 0x300000}, {'A', 0x5A1880}, {0, 0}};
  int sx = 0, sy = 0;
  if (t < 10.12f) {
    float mx = lerpf(17, 7, seg(0.3f, 3));
    bool stomp = t < 3 && fmodf(t, 0.5f) < 0.08f;
    bool roar = (t > 3.3f && t < 4.8f) || (t > 8.6f && t < 10.12f);
    float lift = t > 8.6f ? 1 : 0;
    drawArt(roar ? BEAST_ROAR : BEAST, 11, mx, 3 - lift, BI);
    if (t > 8.6f)  // fists up
      for (int i = 0; i < 2; i++) setPx(mx - 1, 3 + i, 0x5A1880), setPx(mx + 9, 3 + i, 0x5A1880);
    if (stomp) sy = 1;
    if (roar && t < 4.8f) sx = (int)(t * 30) % 3 - 1;
  }
  // The hero walks in, waits ("..."), punches, then wanders off.
  static const Ink HI[] = {{'H', 0xF0B080}, {'E', 0x000000}, {'Y', 0xF0D000}, {'C', 0xE8E8E8},
                           {'R', 0xE01010}, {'B', 0x202020}, {0, 0}};
  if (t > 4.6f) {
    float hx = 0;
    bool walking = false, left = false;
    if (t < 7.5f) hx = lerpf(-8, 0, seg(4.6f, 7.5f)), walking = true;
    else if (t > 9.7f && t < 14.3f) hx = 1;
    else if (t >= 14.3f) hx = lerpf(1, -9, seg(14.3f, 18)), walking = left = true;
    bool punching = t > 9.9f && t < 11.6f;
    const char *const *art = punching ? HERO_PUNCH : walking && (int)(t * 5) % 2 ? HERO_STEP : HERO;
    drawArt(art, 10, hx, 4, HI, left);
    int ix = roundf(hx);
    if (punching) setPx(ix + 7, 9, 0xF0D000), setPx(ix + 8, 9, 0xE01010);
    // Cape flaps in the wind after the punch.
    if (t > 10.1f && t < 14.3f && (int)(t * 8) % 2) setPx(ix - 1, 10, 0xE8E8E8), setPx(ix - 1, 11, 0xE8E8E8);
    // "..." while the monster rages.
    for (int i = 0; i < 3; i++)
      if (t > 7.9f + i * 0.5f && t < 9.7f) setPx(ix + 1 + i * 2, 2, 0xFFFFFF);
  }
  // PUNCH: a flash, speed lines, and the monster blown to bits.
  if (t > 10.1f && !blown)
    for (auto &b : bits) {
      static const uint32_t BC[] = {0x7020A0, 0x5A1880, 0xE0D0B0, 0x9040C0};
      blown = true;
      b = {rnd(8, 15), rnd(3, 13), rnd(2.5f, 9), rnd(-11, -3), BC[random(4)]};
    }
  for (auto &b : bits) {
    if (b.y > 50) continue;
    b.vy += 5 * dt, b.x += b.vx * dt, b.y += b.vy * dt;
    if (b.x > W + 1 || b.y > H + 1) b.y = 99;
    else setPx(roundf(b.x), roundf(b.y), b.c);
  }
  if (t > 10.1f && t < 10.9f) {
    float p = seg(10.1f, 10.9f);
    for (int a = 0; a < 10; a++) {
      float ang = a * 0.628f + 0.3f, r0 = 1 + p * 6, r1 = r0 + 2.5f;
      for (float r = r0; r < r1; r += 0.5f) fbAdd(roundf(9 + cosf(ang) * r), roundf(9 + sinf(ang) * r), scale(0xFFF0A0, 200 * (1 - p), 200 * (1 - p), 200 * (1 - p)));
    }
    sx = (int)(t * 40) % 3 - 1, sy = (int)(t * 23) % 3 - 1;
  }
  // The monster ends up a twinkle in the sky.
  if (t > 12.2f && t < 13.6f) {
    float k = sinf(PI * seg(12.2f, 13.6f));
    uint32_t c = scale(0xFFFFFF, 255 * k, 255 * k, 255 * k);
    fbAdd(13, 1, c), fbAdd(12, 1, scale(c, 120, 120, 120)), fbAdd(14, 1, scale(c, 120, 120, 120));
    fbAdd(13, 0, scale(c, 120, 120, 120)), fbAdd(13, 2, scale(c, 120, 120, 120));
  }
  // Screen shake.
  if (sx || sy) {
    static uint32_t tmp[H][W];
    memcpy(tmp, fb, sizeof(tmp));
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        int ox = x - sx, oy = y - sy;
        fb[y][x] = ox >= 0 && oy >= 0 && ox < W && oy < H ? tmp[oy][ox] : 0;
      }
  }
  if (t > 10.05f && t < 10.5f) {  // white flash
    float f = t < 10.18f ? 1 : 1 - seg(10.18f, 10.5f);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) fb[y][x] = lerpColor(fb[y][x], 0xFFFFFF, f);
  }
  storyFade(LEN);
}

// ---- Web Swing: a red-and-blue masked hero swings across the night city,
// clings to a wall, webs up a fleeing thief, and swings off -----------------
const char *const SPIDEY[] = {".RRR.", "RWRWR", ".RRR.", "BRKRB", "BRRRB", ".B.B.", ".R.R."};
const char *const THIEF[] = {".KKK.", ".KSK.", ".GGG.", "GGGGG", ".GGG.", ".G.G.", ".K.K."};
const char *const THIEF_RUN[] = {".KKK.", ".KSK.", ".GGG.", "GGGGG", ".GGG.", "G...G", "K...K"};

// A web strand from (x0,y0) to (x1,y1).
void webLine(float x0, float y0, float x1, float y1) {
  float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  int n = max(1, (int)(len * 2));
  for (int i = 0; i <= n; i++) addAA(lerpf(x0, x1, (float)i / n), lerpf(y0, y1, (float)i / n), 0x707078);
}

void fxWeb() {
  const float LEN = 16;
  storyStart(LEN);
  float t = storyT;
  // Night sky, moon, far skyline, two buildings and the street.
  skyGradient(0x02020A, 0x0A1030, 15);
  disc(2.5f, 2.5f, 1.6f, 0xB0B090);
  for (int x = 0; x < W; x++) {
    int top = 9 + (int)(fwHash(x / 2 + 7) * 3);
    for (int y = top; y < 15; y++) fb[y][x] = 0x0A0A1A;
  }
  for (int y = 6; y < 15; y++)
    for (int x = 0; x < 3; x++) fb[y][x] = x < 2 && y > 6 && fwHash(x * 13 + y * 7) > 0.6f ? 0x504018 : 0x101020;
  for (int y = 0; y < 15; y++)
    for (int x = 13; x < W; x++) fb[y][x] = x > 13 && y % 2 && fwHash(x * 11 + y * 3) > 0.55f ? 0x504018 : 0x0E0E1E;
  rect(0, 15, W - 1, 15, 0x18181E);
  // Police lights once the thief is caught.
  if (t > 9) {
    uint32_t c = (int)(t * 4) % 2 ? 0x500000 : 0x000060;
    for (int y = 11; y < 16; y++) fbAdd(0, y, c), fbAdd(1, y, scale(c, 120, 120, 120));
    fbAdd(2, 15, scale(c, 80, 80, 80)), fbAdd(3, 15, scale(c, 50, 50, 50));
  }
  static const Ink SI[] = {{'R', 0xE01018}, {'W', 0xFFFFFF}, {'B', 0x1838C8}, {'K', 0x100000}, {0, 0}};
  static const Ink TI[] = {{'K', 0x181818}, {'S', 0xC09070}, {'G', 0x585868}, {0, 0}};
  // The thief runs in with a bag of loot.
  float thx = lerpf(-6, 4, seg(3.6f, 6.4f));
  bool caught = t > 6.9f;
  if (!caught) {
    bool run = t < 6.4f && (int)(t * 8) % 2;
    drawArt(run ? THIEF_RUN : THIEF, 7, thx, 8, TI);
    rect(roundf(thx) + 5, 11, roundf(thx) + 6, 12, 0xA08040);
    setPx(roundf(thx) + 5, 11, 0xFFD040);
  } else {
    // Caught: wrapped in web, then hoisted up to dangle from a strand.
    float lift = 4 * ease(seg(8, 9.5f));
    float cx = 6 + (t > 9.5f ? 0.7f * sinf((t - 9.5f) * 2.2f) * expf(-(t - 9.5f) * 0.15f) : 0), cy = 11 - lift;
    if (t > 8) webLine(6, -1, cx, cy - 3);
    float wrap = seg(6.9f, 7.8f);
    drawArt(THIEF, 7, cx - 2, 8 - lift, TI);
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 5; x++)
        if (y >= 7 - wrap * 6) {
          int px = roundf(cx - 2) + x, py = roundf(8 - lift) + y;
          if (THIEF[y][x] != '.' || (y > 1 && (x == 1 || x == 3))) setPx(px, py, (x + py) % 2 ? 0xC8C8D0 : 0x8C8C98);
        }
    rect(8, 13, 9, 14, 0xA08040), setPx(8, 13, 0xFFD040);  // the dropped loot
  }
  // The hero: swing in, cling to the wall, web the thief, swing away.
  float hx, hy;  // hero's top hand
  bool left = false, swinging = true;
  if (t < 2) hx = -20, hy = 0;
  else if (t < 4.2f) {
    float a = lerpf(-1.3f, 0.75f, ease(seg(2, 4.2f)));
    hx = 4 + 11 * sinf(a), hy = -7 + 11 * cosf(a);
    webLine(4, -7, hx, hy);
  } else if (t < 11) {
    hx = 12, hy = 2 + 0.3f * sinf(t * 1.5f), swinging = false, left = true;
  } else {
    float a = lerpf(-0.2f, 1.5f, ease(seg(11, 13)));
    hx = 14 + 11 * sinf(a), hy = -8 + 11 * cosf(a);
    if (t < 13) webLine(14, -8, hx, hy);
  }
  if (hx > -8 && hx < 22) {
    float px = left ? hx - 4 : hx - 4, py = hy;
    drawArt(SPIDEY, 7, px, py + 1, SI, left);
    int ix = roundf(px), iy = roundf(py);
    if (swinging) setPx(ix + 4, iy, 0xE01018), setPx(ix + 4, iy + 1, 0x1838C8);
    // Web shot at the thief.
    if (t > 6.4f && t < 8) {
      float p = seg(6.4f, 6.9f);
      webLine(ix, iy + 4, lerpf(ix, 6, p), lerpf(iy + 4, 10, p));
    }
  }
  storyFade(LEN);
}

// ---- Iron Flight: a red-and-gold armored hero jets in, lands hard, blasts
// two attack drones (hand repulsor, then chest beam) and flies off ----------
const char *const ARMOR[] = {".RRR.", "RWGWR", ".GGG.", "RRCRR", "GRRRG", ".RGR.", ".R.R.", ".G.G."};
const char *const DRONE[] = {"K...K", "KKKKK", "KKEKK", ".K.K."};
struct Boom { float x, y, vx, vy, life; uint8_t smoke; } booms[48];
void boomAdd(float x, float y, float vx, float vy, float life, uint8_t smoke) {
  int best = 0;
  for (int i = 1; i < 48; i++)
    if (booms[i].life < booms[best].life) best = i;
  booms[best] = {x, y, vx, vy, life, smoke};
}
void boomBurst(float x, float y) {
  for (int i = 0; i < 20; i++) {
    float a = rnd(0, 6.28f), v = rnd(2, 7);
    boomAdd(x, y, cosf(a) * v, sinf(a) * v - 1.5f, rnd(0.5f, 1.1f), 0);
  }
  for (int i = 0; i < 6; i++) boomAdd(x + rnd(-1, 1), y + rnd(-1, 1), rnd(-0.6f, 0.6f), rnd(-1.6f, -0.6f), rnd(1.5f, 2.5f), 1);
}

void fxIron() {
  const float LEN = 12.8f;
  static bool blewA, blewB;
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    blewA = blewB = false;
  }
  float t = storyT, dt = sdt();
  // Dusk over the city.
  skyGradient(0x0C1440, 0xC05028, 15);
  for (int x = 0; x < W; x++) {
    int top = 9 + (int)(fwHash(x / 2 + 3) * 4);
    for (int y = top; y < 15; y++) fb[y][x] = x % 2 && y > top && fwHash(x * 7 + y * 13) > 0.8f ? 0x584010 : 0x120A16;
  }
  rect(0, 15, W - 1, 15, 0x1C141C);

  // Where the hero is.
  float hx = 3, hy = 7;
  bool jets = false;
  if (t < 3.5f) {
    float p = seg(1, 3.5f), e = 1 - (1 - p) * (1 - p);
    hx = lerpf(-6, 3, e), hy = lerpf(-9, 7, e), jets = true;
  } else if (t > 9.6f) {
    float p = seg(9.6f, 11.6f);
    hx = lerpf(3, 13, p * p), hy = lerpf(7, -14, p * p), jets = true;
  }
  bool crouch = t > 3.5f && t < 3.95f;
  int sx = 0, sy = 0;
  if (crouch) {
    sy = (int)(t * 30) % 2;
    if (t < 3.56f)
      for (int i = 0; i < 6; i++) boomAdd(hx + 2 + (i % 2 ? 2.5f : -2.5f), 14, (i % 2 ? 3 : -3) * rnd(0.5f, 1), -rnd(0.3f, 1), 0.8f, 1);
  }
  if (jets && hx > -6 && hy > -10 && random(100) < 60) boomAdd(hx + 2 + rnd(-1, 1), hy + 9, rnd(-0.3f, 0.3f), rnd(0.5f, 1.5f), 0.9f, 1);

  // Two drones fly in and bob about.
  static const Ink DI[] = {{'K', 0x50505C}, {'E', 0xFF2010}, {0, 0}};
  float ax = lerpf(18, 11, ease(seg(3.6f, 5))), ay = 10 + 0.6f * sinf(t * 3);
  float bx = lerpf(18, 11, ease(seg(4.2f, 5.6f))), by = 2.5f + 0.7f * sinf(t * 2.4f + 1);
  if (!blewA && t > 6.2f) blewA = true, boomBurst(ax + 2, ay + 1.5f);
  if (!blewB && t > 8.6f) blewB = true, boomBurst(bx + 2, by + 1.5f);
  if (!blewA) drawArt(DRONE, 4, ax, ay, DI);
  if (!blewB) drawArt(DRONE, 4, bx, by, DI);
  // Drone B fires red shots that spark off the armor.
  if (t > 6.9f && t < 8.2f) {
    float p = fmodf((t - 6.9f) * 2.5f, 1);
    float px = lerpf(bx + 2, hx + 3, p), py = lerpf(by + 3, hy + 3, p);
    if (p < 0.9f) addAA(px, py, 0xFF2010);
    else fbAdd(hx + 4, hy + 3, 0xFFC040);
  }

  // Explosions and smoke.
  for (auto &b : booms) {
    if (b.life <= 0) continue;
    b.life -= dt;
    b.x += b.vx * dt, b.y += b.vy * dt;
    b.vx *= expf(-dt * 2), b.vy = b.vy * expf(-dt * 2) + (b.smoke ? -0.5f : 3) * dt;
    float k = min(1.0f, b.life);
    uint32_t c = b.smoke ? scale(0x707078, 140 * k, 140 * k, 140 * k)
                         : lerpColor(0x801000, b.life > 0.6f ? 0xFFF0A0 : 0xFF8000, k);
    if (b.smoke) fbAdd(roundf(b.x), roundf(b.y), c);
    else addAA(b.x, b.y, c);
  }

  // The hero, with boot jets.
  static const Ink AI[] = {{'R', 0xC01010}, {'G', 0xE0A020}, {'W', 0xC0FFFF}, {'C', 0x80FFFF}, {0, 0}};
  int ix = roundf(hx), iy = roundf(hy) + (crouch ? 1 : 0);
  drawArt(ARMOR, 8, ix, iy, AI);
  if (jets) {
    bool f = (int)(t * 20) % 2;
    for (int k = 0; k < 2; k++) {
      int fx = ix + 1 + k * 2;
      setPx(fx, iy + 8, 0xFFF0C0), setPx(fx, iy + 9, f ? 0xFF8000 : 0xFFB000);
      if (f) setPx(fx, iy + 10, 0x902000);
    }
  }
  // Hand repulsor at drone A.
  if (t > 5.2f && t < 6.6f) {
    setPx(ix + 5, iy + 4, 0xE0A020);
    float charge = seg(5.2f, 5.8f);
    uint8_t g = 255 * charge;
    fbAdd(ix + 6, iy + 4, rgb(g / 2, g, g));
    if (t > 5.8f && t < 6.2f) {
      float p = seg(5.8f, 6.2f);
      for (int i = 0; i < 4; i++) {
        float q = max(0.0f, p - i * 0.08f);
        uint8_t k = 255 - i * 60;
        addAA(lerpf(ix + 6, ax + 2, q), lerpf(iy + 4, ay + 1.5f, q), scale(0xA0FFFF, k, k, k));
      }
    }
  }
  // Chest beam at drone B.
  if (t > 7.9f && t < 9.1f) {
    float ch = 0.5f + 0.5f * sinf(t * 30);
    uint8_t g = 120 * ch * seg(7.9f, 8.3f);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++)
        if (dx || dy) fbAdd(ix + 2 + dx, iy + 3 + dy, rgb(g / 3, g, g));
    if (t > 8.3f) {
      float k = 1 - seg(8.7f, 9.1f);
      float x0 = ix + 2, y0 = iy + 3, x1 = bx + 2, y1 = by + 1.5f;
      float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
      for (float s = 0; s <= len; s += 0.4f) {
        float px = lerpf(x0, x1, s / len), py = lerpf(y0, y1, s / len);
        addAA(px, py, scale(0xE0FFFF, 255 * k, 255 * k, 255 * k));
        addAA(px, py - 0.7f, scale(0x40C0FF, 160 * k, 160 * k, 160 * k));
        addAA(px, py + 0.7f, scale(0x40C0FF, 160 * k, 160 * k, 160 * k));
      }
    }
  }
  if ((t > 6.2f && t < 6.4f) || (t > 8.6f && t < 8.8f)) sx = (int)(t * 40) % 3 - 1;
  if (sx || sy) {
    static uint32_t tmp[H][W];
    memcpy(tmp, fb, sizeof(tmp));
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        int ox = x - sx, oy = y - sy;
        fb[y][x] = ox >= 0 && oy >= 0 && ox < W && oy < H ? tmp[oy][ox] : 0;
      }
  }
  storyFade(LEN);
}

// ---- Sky Hero: a bird, a plane... then a caped hero in blue and red, who
// stops a falling meteor with heat vision -----------------------------------
const char *const SUPER[] = {".KKK.", ".ESE.", "..S..", "BBYBB", "BBRBB", ".RYR.", ".B.B.", ".B.B.", ".R.R."};
const char *const SUPER_FLY[] = {"CCC...KK...", "RBBBBYBSSBS", "RBBRB......"};

void fxSky() {
  const float LEN = 14.5f;
  static bool shattered;
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    shattered = false;
  }
  float t = storyT, dt = sdt();
  // A sunny day over the city.
  skyGradient(0x1850C0, 0x80B8F0, 15);
  disc(13, 2, 1.4f, 0xFFF0A0);
  for (int x = 0; x < W; x++) {
    int top = 9 + (int)(fwHash(x / 2 + 21) * 4);
    for (int y = top; y < 15; y++) fb[y][x] = x % 2 && y > top && y % 2 ? 0x8098B8 : 0x405070;
  }
  rect(0, 15, W - 1, 15, 0x304030);
  // "Look, up in the sky!" A bird...
  if (t > 0.5f && t < 2.6f) {
    float bx = lerpf(-2, 17, seg(0.5f, 2.6f)), by = 4 + 0.5f * sinf(t * 4);
    bool up = (int)(t * 8) % 2;
    setPx(bx, by, 0x202020), setPx(bx - 1, by - up, 0x202020), setPx(bx + 1, by - up, 0x202020);
  }
  // ...a plane...
  if (t > 2.2f && t < 4.4f) {
    int px = roundf(lerpf(17, -6, seg(2.2f, 4.4f)));
    rect(px, 6, px + 4, 6, 0xE0E0E8);
    setPx(px + 2, 5, 0xB0B0C0), setPx(px + 2, 7, 0xB0B0C0), setPx(px + 4, 5, 0xB0B0C0);
  }
  // ...no, it's the hero, streaking past!
  static const Ink SI[] = {{'K', 0x101018}, {'E', 0x2040A0}, {'S', 0xF0B080}, {'B', 0x1840D0},
                           {'Y', 0xFFD020}, {'R', 0xE01010}, {'C', 0xE01010}, {0, 0}};
  if (t > 4.5f && t < 5.6f) {
    float fx = lerpf(-12, 17, seg(4.5f, 5.6f));
    drawArt(SUPER_FLY, 3, fx, 3, SI);
    for (int i = 1; i < 6; i++) fbAdd(roundf(fx) - i, 4, scale(0xFFFFFF, 90 - i * 15, 90 - i * 15, 90 - i * 15));
  }
  // A meteor falls toward the city.
  float mp = seg(6, 10), mx = lerpf(17, 9, mp), my = lerpf(-2, 6, mp);
  if (t > 6 && !shattered) {
    for (int i = 1; i < 6; i++) addAA(mx + i * 0.8f, my - i * 0.8f, scale(0xFF6000, 200 - i * 35, 200 - i * 35, 200 - i * 35));
    disc(mx, my, 1.2f, (int)(t * 12) % 2 ? 0xFFB020 : 0xFF7010);
    setPx(roundf(mx), roundf(my), 0xFFF0C0);
  }
  if (!shattered && t > 9.6f) {
    shattered = true;
    boomBurst(mx, my);
  }
  // The hero flies up to meet it, then hovers, hands on hips.
  float hx = 2, hy = 4;
  bool armUp = false;
  if (t < 7) hy = 30;
  else if (t < 8.4f) hy = lerpf(17, 4, ease(seg(7, 8.4f))), armUp = true;
  else if (t > 12.4f) hy = lerpf(4, -14, seg(12.4f, 14)), armUp = true;
  hy += 0.4f * sinf(t * 2);
  if (hy < 20) {
    int ix = 2, iy = roundf(hy);
    // Cape billows behind.
    float wave = sinf(t * 6);
    for (int y = 3; y <= 8; y++) setPx(ix - 1 - (y > 6 && wave > 0), iy + y, 0xE01010);
    if (armUp) for (int y = 9; y <= 10; y++) setPx(ix + 1 + (wave > 0) + (y - 9), iy + y, 0xC00808);
    drawArt(SUPER, 9, ix, iy, SI);
    if (armUp) setPx(ix + 4, iy + 2, 0x1840D0), setPx(ix + 4, iy + 1, 0x1840D0), setPx(ix + 4, iy, 0xF0B080);
    // Heat vision!
    if (t > 8.6f && t < 9.7f) {
      float x0 = ix + 1.5f, y0 = iy + 1, len = sqrtf((mx - x0) * (mx - x0) + (my - y0) * (my - y0));
      for (float s = 0; s < len; s += 0.4f) {
        float px = lerpf(x0, mx, s / len), py = lerpf(y0, my, s / len);
        addAA(px, py - 0.4f, 0xC00000), addAA(px, py + 0.4f, 0xC00000);
      }
      setPx(ix + 1, iy + 1, 0xFF4040), setPx(ix + 3, iy + 1, 0xFF4040);
    }
  }
  // Pieces of meteor burn up.
  for (auto &b : booms) {
    if (b.life <= 0) continue;
    b.life -= dt;
    b.x += b.vx * dt, b.y += b.vy * dt;
    b.vx *= expf(-dt * 2), b.vy = b.vy * expf(-dt * 2) + (b.smoke ? -0.5f : 3) * dt;
    float k = min(1.0f, b.life);
    uint32_t c = b.smoke ? scale(0x808088, 140 * k, 140 * k, 140 * k) : lerpColor(0x801000, b.life > 0.6f ? 0xFFF0A0 : 0xFF8000, k);
    addAA(b.x, b.y, c);
  }
  storyFade(LEN);
}

// ---- Bat Signal: the signal lights the clouds; a caped night hero glides
// down from a rooftop, stops a thug (POW!) and grapples away ----------------
const char *const BATS[] = {"K...K", "KKKKK", "KWKWK", ".KSK.", "CGKGC", "CGGGC", "CYYYC", "CG.GC", "CK.KC"};

void fxBat() {
  const float LEN = 13.5f;
  storyStart(LEN);
  float t = storyT;
  // Night, clouds, a moon, and the signal shining on the clouds.
  skyGradient(0x020208, 0x0C0C1E, 15);
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < W; x++) {
      float d = sinf(x * 0.5f + tsec * 0.2f + y) + sinf(x * 0.21f - tsec * 0.13f + y * 2) + 0.6f - y * 0.4f;
      if (d > 0) fb[y][x] = lerpColor(fb[y][x], 0x16161E, min(1.0f, d));
    }
  float on = seg(0.3f, 1.2f);
  for (float s = 0; s < 1; s += 0.04f) {  // searchlight beam
    float bx = lerpf(15, 10, s), by = lerpf(15, 4.5f, s);
    uint8_t k = 50 * on;
    addAA(bx, by, rgb(k, k * 0.85f, k * 0.4f));
  }
  static const char *const SIGNAL[] = {"...........", ".K..K.K..K.", ".KKKKKKKKK.", "..K.KKK.K..", "..........."};
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 11; x++) {
      float dx = (x - 5) / 5.4f, dy = (y - 2) / 2.6f;
      if (dx * dx + dy * dy > 1) continue;
      fb[y][x + 5] = lerpColor(fb[y][x + 5], SIGNAL[y][x] == 'K' ? 0x060604 : 0xC8A030, on);
    }
  // Rooftop on the left, skyline, street.
  for (int x = 0; x < W; x++) {
    int top = x < 5 ? 10 : 10 + (int)(fwHash(x / 2 + 40) * 3);
    for (int y = top; y < 15; y++) fb[y][x] = x >= 5 && x % 2 && y > top && fwHash(x * 5 + y * 9) > 0.75f ? 0x403010 : 0x0A0A14;
  }
  rect(0, 15, W - 1, 15, 0x141418);
  // The thug walks in, then gets knocked flat.
  static const Ink TI2[] = {{'K', 0x303030}, {'S', 0xC09070}, {'G', 0x6A6A78}, {0, 0}};
  bool down = t > 7;
  if (!down) {
    float tx = lerpf(17, 9.5f, seg(1.5f, 4.5f));
    drawArt(t < 4.5f && (int)(t * 6) % 2 ? THIEF_RUN : THIEF, 7, tx, 8, TI2, true);
  } else {
    rect(10, 14, 14, 14, 0x6A6A78), rect(9, 13, 9, 14, 0x303030), setPx(10, 13, 0xC09070);
    if (t < 10.5f)  // seeing stars
      for (int i = 0; i < 2; i++) {
        float a = t * 5 + i * PI;
        setPx(roundf(10 + 1.6f * cosf(a)), roundf(11.5f + 0.6f * sinf(a)), 0xFFE040);
      }
  }
  // The hero: on the rooftop, leaps, glides, punches, grapples away.
  static const Ink BI2[] = {{'K', 0x18181E}, {'W', 0xE0E0E0}, {'S', 0xC09070}, {'G', 0x686870},
                            {'Y', 0xE0C020}, {'C', 0x2A2A44}, {0, 0}};
  float hx = 0, hy = 1;
  bool glide = false, punch = false;
  if (t > 4.2f && t < 6.4f) {
    float p = ease(seg(4.2f, 6.4f));
    hx = lerpf(0, 5, p), hy = lerpf(1, 6, p) - 2.5f * sinf(PI * p), glide = true;
  } else if (t >= 6.4f) {
    hx = 5, hy = 6;
    punch = t > 6.8f && t < 7.6f;
  }
  if (t > 10.2f) hy = lerpf(6, -12, seg(10.6f, 11.6f));
  int ix = roundf(hx), iy = roundf(hy);
  if (!glide && t < 4.2f && (int)(t * 4) % 2) setPx(ix + 5, iy + 8, 0x2A2A44);  // cape stirs in the wind
  if (glide)
    for (int k = 1; k <= 3; k++)
      for (int j = 0; j < k; j++) setPx(ix - 1 - j, iy + 3 + k, 0x2A2A44), setPx(ix + 5 + j, iy + 3 + k, 0x2A2A44);
  if (iy > -10) drawArt(BATS, 9, ix, iy, BI2);
  if (punch) setPx(ix + 5, iy + 4, 0x686870), setPx(ix + 6, iy + 4, 0x18181E);
  if (t > 10.2f && t < 11.6f) webLine(ix + 4, iy, ix + 5, -1);  // grapple line
  // POW!
  if (t > 6.9f && t < 8) {
    float pop = min(1.0f, seg(6.9f, 7.05f) * 1.2f);
    ellipse(10, 4.5f, 6 * pop, 3.4f * pop, 0xC01818);
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4 + 0.4f;
      setPx(roundf(10 + cosf(a) * 7 * pop), roundf(4.5f + sinf(a) * 4 * pop), 0xFF8020);
    }
    if (pop >= 1) tinyText("POW", 5, 2, 0xFFE020);
  }
  storyFade(LEN);
}

// ---- Helpers shared by the later hero stories -------------------------------
// Shifts the whole frame by (sx, sy) for a screen shake.
void fbShift(int sx, int sy) {
  if (!sx && !sy) return;
  static uint32_t tmp[H][W];
  memcpy(tmp, fb, sizeof(tmp));
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int ox = x - sx, oy = y - sy;
      fb[y][x] = ox >= 0 && oy >= 0 && ox < W && oy < H ? tmp[oy][ox] : 0;
    }
}

// Moves and draws the sparks and smoke made by boomAdd / boomBurst.
void boomStep(float dt, uint32_t smoke, bool ice = false) {
  for (auto &b : booms) {
    if (b.life <= 0) continue;
    b.life -= dt;
    b.x += b.vx * dt, b.y += b.vy * dt;
    b.vx *= expf(-dt * 2), b.vy = b.vy * expf(-dt * 2) + (b.smoke ? -0.5f : 3) * dt;
    float k = constrain(b.life, 0.0f, 1.0f);
    if (b.smoke) fbAdd(roundf(b.x), roundf(b.y), scale(smoke, 160 * k, 160 * k, 160 * k));
    else if (ice) addAA(b.x, b.y, lerpColor(0x102850, b.life > 0.6f ? 0xE0F4FF : 0x80C0FF, k));
    else addAA(b.x, b.y, lerpColor(0x801000, b.life > 0.6f ? 0xFFF0A0 : 0xFF8000, k));
  }
}

// A sprite blended over the scene (a = 0..1), for fading in and out.
void drawArtA(const char *const *rows, int n, float fx, float fy, const Ink *pal, float a, bool flip = false) {
  if (a <= 0) return;
  a = min(1.0f, a);
  int ox = roundf(fx), oy = roundf(fy);
  for (int y = 0; y < n; y++) {
    int len = strlen(rows[y]);
    for (int x = 0; x < len; x++) {
      char ch = rows[y][flip ? len - 1 - x : x];
      for (const Ink *p = pal; p->k; p++)
        if (p->k == ch) {
          int px = ox + x, py = oy + y;
          if (px >= 0 && py >= 0 && px < W && py < H) fb[py][px] = lerpColor(fb[py][px], p->c, a);
          break;
        }
    }
  }
}

// A jagged electric bolt, re-randomised every frame.
void boltLine(float x0, float y0, float x1, float y1, uint32_t c) {
  float px = x0, py = y0;
  for (int i = 1; i <= 4; i++) {
    float f = i / 4.0f, nx = lerpf(x0, x1, f) + (i < 4 ? rnd(-0.6f, 0.6f) : 0),
          ny = lerpf(y0, y1, f) + (i < 4 ? rnd(-1.4f, 1.4f) : 0);
    for (float s = 0; s <= 1; s += 0.25f) addAA(lerpf(px, nx, s), lerpf(py, ny, s), c);
    px = nx, py = ny;
  }
}

// ---- Star Shield: a soldier in stars and stripes blocks laser fire with his
// round shield, then throws it to ricochet through three robots and back ----
const char *const CAPN[] = {".BBB.", ".BSB.", "..S..", "BBWBB", "BRWRB", ".WRW.", ".B.B.", ".B.B.", ".R.R."};
const char *const CAPN_RUN[] = {".BBB.", ".BSB.", "..S..", "BBWBB", "BRWRB", ".WRW.", ".B.B.", "B...B", "R...R"};
const char *const ROBO[] = {".KK.", "KEEK", "KKKK", "K..K", "K..K"};
const char *const ROBO_STEP[] = {".KK.", "KEEK", "KKKK", "K..K", ".K.K"};

// The round shield: red rim, white ring, blue centre, with a glint as it spins.
void drawShield(float cx, float cy, float spin) {
  ellipse(cx, cy, 2.2f, 2.2f, 0xC01010);
  ellipse(cx, cy, 1.1f, 1.1f, 0xE8E8E8);
  setPx(roundf(cx), roundf(cy), 0x1840D0);
  addAA(cx + 2 * cosf(spin), cy + 2 * sinf(spin), 0x707070);
}

void fxShield() {
  const float LEN = 12;
  static bool hit[3];
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    hit[0] = hit[1] = hit[2] = false;
  }
  float t = storyT, dt = sdt();
  // Dusk over a ruined street.
  skyGradient(0x0A0E2A, 0x8A4428, 15);
  for (int x = 0; x < W; x++) {
    int top = 8 + (int)(fwHash(x / 3 + 61) * 5);
    for (int y = top; y < 15; y++) fb[y][x] = y > top && fwHash(x * 5 + y * 11) > 0.85f ? 0x3A2018 : 0x180E14;
  }
  rect(0, 15, W - 1, 15, 0x2C2218);

  // Three robots arrive: a walker and two drones.
  static const Ink BI[] = {{'K', 0x585864}, {'E', 0xFF2010}, {0, 0}};
  float ax = lerpf(18, 12, seg(0.5f, 2.5f)), ay = 10;
  float bx = lerpf(18, 11, ease(seg(0.8f, 2.4f))), by = 1.5f + 0.6f * sinf(t * 2.6f);
  float cx = lerpf(-7, 5, ease(seg(1.4f, 3))), cy = 0.5f + 0.5f * sinf(t * 3 + 2);
  // The hero runs in, and later runs off.
  float hx = lerpf(-6, 1, seg(0.3f, 2.3f));
  if (t > 8.6f) hx = lerpf(1, 18, seg(8.6f, 10.6f));
  int ix = roundf(hx), iy = 6;
  // The shield's ricochet path: hand, walker, drone, drone, hand.
  const float HT[5] = {3.7f, 4.15f, 4.6f, 5.05f, 5.6f};
  float px[5] = {ix + 4.0f, ax + 1.5f, bx + 2, cx + 2, ix + 6.0f};
  float py[5] = {iy + 4.0f, ay + 2, by + 1.5f, cy + 1.5f, iy + 4.0f};
  for (int i = 0; i < 3; i++)
    if (!hit[i] && t > HT[i + 1]) hit[i] = true, boomBurst(px[i + 1], py[i + 1]);
  if (!hit[0]) drawArt(t < 2.5f && (int)(t * 6) % 2 ? ROBO_STEP : ROBO, 5, ax, ay, BI);
  if (!hit[1]) drawArt(DRONE, 4, bx, by, BI);
  if (!hit[2]) drawArt(DRONE, 4, cx, cy, BI);

  static const Ink CI[] = {{'B', 0x1838C0}, {'S', 0xF0B080}, {'W', 0xE8E8E8}, {'R', 0xD01010}, {0, 0}};
  bool run = (t > 0.3f && t < 2.3f) || t > 8.6f;
  drawArt(run && (int)(t * 8) % 2 ? CAPN_RUN : CAPN, 9, ix, iy, CI);

  // Where the shield is.
  float sx = ix + 6, sy = iy + 4, spin = t * 3;
  if (t > 3.3f && t < HT[0]) sx -= 2 * ease(seg(3.3f, HT[0]));  // wind up
  if (t >= HT[0] && t < HT[4]) {
    int k = 0;
    while (k < 3 && t > HT[k + 1]) k++;
    float p = (t - HT[k]) / (HT[k + 1] - HT[k]);
    sx = lerpf(px[k], px[k + 1], p), sy = lerpf(py[k], py[k + 1], p), spin = t * 25;
    setPx(ix + 5, iy + 3, 0x1838C0);  // arm out after the throw
  }
  float up = ease(seg(6, 6.5f)) * (1 - ease(seg(7.9f, 8.3f)));  // held high in victory
  if (up > 0) sx = lerpf(sx, ix + 2.5f, up), sy = lerpf(sy, iy - 3, up);
  drawShield(sx, sy, spin);
  if (t > 6.6f && t < 7.6f) {  // a twinkle on the raised shield
    uint8_t g = 255 * sinf(PI * seg(6.6f, 7.6f));
    uint32_t c = rgb(g, g, g);
    int gx = roundf(sx) + 1, gy = roundf(sy) - 2;
    fbAdd(gx, gy, c), fbAdd(gx - 1, gy, scale(c, 120, 120, 120)), fbAdd(gx + 1, gy, scale(c, 120, 120, 120));
    fbAdd(gx, gy - 1, scale(c, 120, 120, 120)), fbAdd(gx, gy + 1, scale(c, 120, 120, 120));
  }

  // The drone's laser shots spark off the shield.
  if (t > 2.6f && t < 3.3f) {
    float p = fmodf((t - 2.6f) * 3, 1);
    float x0 = bx + 2, y0 = by + 4, x1 = sx + 1.5f, y1 = sy - 0.5f;
    if (p < 0.85f) {
      addAA(lerpf(x0, x1, p), lerpf(y0, y1, p), 0xFF3020);
      addAA(lerpf(x0, x1, max(0.0f, p - 0.08f)), lerpf(y0, y1, max(0.0f, p - 0.08f)), 0x801000);
    } else fbAdd(roundf(x1) + 1, roundf(y1) - 1, 0xFFE080), fbAdd(roundf(x1) + 2, roundf(y1), 0xFFA040);
  }
  boomStep(dt, 0x707078);
  int sh = 0;
  for (int i = 1; i <= 3; i++)
    if (t > HT[i] && t < HT[i] + 0.18f) sh = (int)(t * 40) % 3 - 1;
  fbShift(sh, 0);
  storyFade(LEN);
}

// ---- Green Smash: a mild scientist is shelled by a tank, turns into a huge
// green brute, smashes the ground to flip the tank away, and leaps off ------
const char *const NERD[] = {".HHH.", ".SSS.", "..S..", "WWWWW", "SWWWS", ".PPP.", ".P.P.", ".K.K."};
const char *const BRUTE[] = {"..KKKKK..", "..GGGGG..", "..GEGEG..", "..GGMGG..", ".GGGGGGG.", "GGGGGGGGG",
                             "GG.GGG.GG", "GG.GGG.GG", "...PPP...", "..PP.PP..", "..GG.GG..", ".GGG.GGG."};
const char *const BRUTE_UP[] = {"GG.....GG", "GGKKKKKGG", ".GGGGGGG.", "..GEGEG..", "..GGMGG..", ".GGGGGGG.",
                                "..GGGGG..", "..GGGGG..", "...PPP...", "..PP.PP..", "..GG.GG..", ".GGG.GGG."};

void fxSmash() {
  const float LEN = 13.5f;
  static bool shot1, shot2, slam, hop;
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    shot1 = shot2 = slam = hop = false;
  }
  float t = storyT, dt = sdt();
  // A desert canyon at sunset.
  skyGradient(0x281040, 0xE87830, 15);
  disc(12.5f, 5, 1.8f, 0xFFD870);
  for (int x = 0; x < W; x++) {
    int top = x < 6 ? 8 + (x == 5) : x < 10 ? 12 : 7 + (x == 10);
    for (int y = top; y < 15; y++) fb[y][x] = lerpColor(0x6A2818, 0x2A0E10, min(1.0f, (y - top) / 5.0f));
  }
  for (int x = 0; x < W; x++) fb[15][x] = 0x6A4422;
  if (slam) {  // the ground cracks out from the impact
    float reach = 14 * (t - 8);
    for (int x = 0; x < W; x++)
      if (fabsf(x - 4) < reach && (fabsf(x - 4) < 2.5f || fwHash(x * 3 + 5) > 0.45f)) fb[15][x] = 0x24140A;
  }

  // The tank rolls in; later it gets flipped into the sky.
  static const Ink TI[] = {{'L', 0x5A6A30}, {'D', 0x3A4420}, {'K', 0x141410}, {0, 0}};
  bool rolling = t > 0.5f && t < 2.6f;
  const char *track = rolling && (int)(t * 10) % 2 ? "DKDKDKD" : "KDKDKDK";
  const char *const tank[] = {"...LLL.", "LLLLLLL", ".DDDDDD", track};
  const char *const tankFlip[] = {track, ".DDDDDD", "LLLLLLL", "...LLL."};
  float tx = lerpf(18, 9, ease(seg(0.5f, 2.6f))), ty = 11;
  if (t > 8.35f) {
    float p = seg(8.35f, 9.5f);
    tx = lerpf(9, 22, p), ty = 11 - 20 * p + 6 * p * p;
  }
  drawArt(t > 8.35f && (int)(t * 8) % 2 ? tankFlip : tank, 4, tx, ty, TI);
  // Shell one lands by the scientist; shell two bounces off the brute.
  if (t > 2.9f && t < 2.98f) fbAdd(roundf(tx) - 1, 12, 0xFFF0A0);
  if (t > 2.9f && t < 3.15f) addAA(lerpf(tx - 1, 8.5f, seg(2.9f, 3.15f)), lerpf(12, 13.5f, seg(2.9f, 3.15f)), 0xFFD060);
  if (!shot1 && t > 3.15f) shot1 = true, boomBurst(8.5f, 13.5f);
  if (t > 6.8f && t < 6.88f) fbAdd(roundf(tx) - 1, 12, 0xFFF0A0);
  if (t > 6.8f && t < 7) addAA(lerpf(tx - 1, 7.5f, seg(6.8f, 7)), lerpf(12, 8, seg(6.8f, 7)), 0xFFD060);
  if (!shot2 && t > 7) {
    shot2 = true;
    for (int i = 0; i < 8; i++) boomAdd(7.5f, 8, rnd(0, 5), rnd(-4, 0.5f), rnd(0.3f, 0.6f), 0);
  }

  // The scientist trembles, glows green, and grows.
  static const Ink NI[] = {{'H', 0x3A2410}, {'S', 0xE0A880}, {'W', 0xC8C8D0}, {'P', 0x5A2A7A}, {'K', 0x181818}, {0, 0}};
  static const Ink NG[] = {{'H', 0x3A2410}, {'S', 0x60C040}, {'W', 0xC8C8D0}, {'P', 0x5A2A7A}, {'K', 0x181818}, {0, 0}};
  static const Ink GI[] = {{'K', 0x101810}, {'G', 0x38A028}, {'E', 0xD0FFC0}, {'M', 0x38A028}, {'P', 0x6A2A8A}, {0, 0}};
  static const Ink GR[] = {{'K', 0x101810}, {'G', 0x38A028}, {'E', 0xD0FFC0}, {'M', 0x200808}, {'P', 0x6A2A8A}, {0, 0}};
  float grow = seg(4, 5.6f);
  bool big = t > 5.6f || (t > 4 && fmodf((t - 4) * (3 + 10 * grow), 1) < grow);
  int shx = 0, shy = 0;
  if (t > 3.6f && t < 6.2f) {  // green aura
    float k = (0.6f + 0.4f * sinf(t * 14)) * seg(3.6f, 4.2f) * (1 - seg(5.6f, 6.2f));
    for (int y = 2; y < 15; y++)
      for (int x = 0; x < 10; x++) {
        float dx = (x - 4.5f) / 5.5f, dy = (y - 9) / 6.5f, d = dx * dx + dy * dy;
        if (d < 1) fbAdd(x, y, rgb(0, 70 * k * (1 - d), 0));
      }
  }
  if (!big) {
    int jit = t > 3.2f && t < 5.6f ? (int)(t * 24) % 2 : 0;
    bool greenish = t > 3.6f && (int)(t * 10) % 2;
    drawArt(NERD, 8, 3 + jit, 7, greenish ? NG : NI);
  } else {
    if (t < 5.6f) shx = (int)(t * 30) % 3 - 1;
    bool roar = (t > 5.7f && t < 6.7f) || (t > 9.6f && t < 10.6f);
    bool armsUp = (t > 7.4f && t < 8) || (t > 9.6f && t < 10.6f && (int)(t * 4) % 2);
    float bx = 0, by = 3;
    if ((t > 8 && t < 8.4f) || (t > 11 && t < 11.3f)) by = 4;  // crouch
    if (t > 11.3f) {
      float p = seg(11.3f, 12.3f);
      by = lerpf(3, -16, sqrtf(p)), bx = lerpf(0, 6, p);
    }
    drawArt(armsUp ? BRUTE_UP : BRUTE, 12, bx, by, roar ? GR : GI);
    if (roar) {  // the roar ripples out
      shx = (int)(t * 30) % 2;
      for (int j = 0; j < 2; j++) {
        float r = fmodf(t * 6 + j * 1.5f, 3) + 2;
        uint8_t g = 150 - r * 25;
        for (int k = -1; k <= 1; k++) addAA(4 + r, 6 + k * r * 0.5f, rgb(g, g, g));
      }
    }
  }
  // SMASH: the ground shakes, dust flies, the tank goes up.
  if (!slam && t > 8) {
    slam = true;
    for (int i = 0; i < 16; i++) boomAdd(4 + rnd(-1, 1), 14.5f, (i % 2 ? 1 : -1) * rnd(2, 9), -rnd(0.5f, 3), rnd(0.8f, 1.6f), 1);
  }
  if (t > 8 && t < 9) {
    float d = 14 * (t - 8);
    fbAdd(roundf(4 - d), 14, 0x806040), fbAdd(roundf(4 + d), 14, 0x806040);
    fbAdd(roundf(4 - d), 13, 0x403020), fbAdd(roundf(4 + d), 13, 0x403020);
  }
  if (t > 8 && t < 8.6f) shx = (int)(t * 40) % 3 - 1, shy = (int)(t * 33) % 2;
  if (!hop && t > 11.3f) {
    hop = true;
    for (int i = 0; i < 10; i++) boomAdd(4 + rnd(-2, 2), 14.5f, rnd(-4, 4), -rnd(0.5f, 2), rnd(0.6f, 1.2f), 1);
  }
  boomStep(dt, 0xA08060);
  fbShift(shx, shy);
  storyFade(LEN);
}

// ---- Spark Spider: a spider hero in black and red hides in camouflage, drops
// on two crooks, webs one, zaps the other with an electric blast, sprays his
// tag on the wall, and vanishes --------------------------------------------
const char *const SPARKY[] = {".KKK.", ".WKW.", "..K..", "KKRKK", "RKKKR", ".K.K.", ".R.R."};
const char *const SPARKY_HANG[] = {".R.R.", ".K.K.", "RKKKR", "KKRKK", "..K..", ".WKW.", ".KKK."};
const char *const TAG[] = {"T...T", ".MMM.", "TMMMT", ".MMM.", "T...T"};

void fxSpark() {
  const float LEN = 13.5f;
  storyStart(LEN);
  float t = storyT;
  // Night: a purple city glow above a brick alley wall.
  skyGradient(0x060214, 0x3A0C44, 6);
  for (int x = 0; x < W; x++) {
    int top = 2 + (int)(fwHash(x / 2 + 77) * 3);
    for (int y = top; y < 6; y++) fb[y][x] = y > top && fwHash(x * 7 + y * 3) > 0.75f ? 0x106070 : 0x12081C;
  }
  for (int y = 6; y < 15; y++)
    for (int x = 0; x < W; x++) fb[y][x] = y % 3 == 2 || (x + (y / 3) * 2) % 4 == 0 ? 0x1C0A22 : 0x401838;
  rect(0, 15, W - 1, 15, 0x0E0A12);

  // The tag goes up on the wall, column by column.
  static const Ink TGI[] = {{'M', 0xE02090}, {'T', 0x20C0C0}, {0, 0}};
  float spray = seg(8, 9.6f);
  if (spray > 0)
    for (int y = 0; y < 5; y++)
      for (int x = 0; x < 5 && x < spray * 5.5f; x++)
        for (const Ink *p = TGI; p->k; p++)
          if (p->k == TAG[y][x]) setPx(11 + x, 6 + y, p->c);

  // Two crooks walk in from either side.
  static const Ink TI[] = {{'K', 0x181818}, {'S', 0xC09070}, {'G', 0x587058}, {0, 0}};
  static const Ink TZ[] = {{'K', 0xFFFFA0}, {'S', 0xFFFFA0}, {'G', 0xFFE040}, {0, 0}};
  bool walk = t > 0.3f && t < 2.4f && (int)(t * 8) % 2;
  float g1 = lerpf(-6, 0, seg(0.3f, 2.4f)), g2 = lerpf(17, 11, seg(0.3f, 2.4f));
  drawArt(walk ? THIEF_RUN : THIEF, 7, g1, 8, TI);
  float wrap = seg(4.8f, 5.6f);  // crook one gets webbed up
  if (wrap > 0) {
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 5; x++)
        if (y >= 7 - wrap * 7 && (THIEF[y][x] != '.' || (y > 1 && (x == 1 || x == 3))))
          setPx(x, 8 + y, (x + y) % 2 ? 0xC8C8D0 : 0x8C8C98);
    if (wrap >= 1) webLine(1, 9, -1, 6), webLine(3, 13, 5, 15);
  }
  if (t < 7) {  // crook two gets zapped, then drops
    bool zap = t > 6.2f && (int)(t * 16) % 2;
    drawArt(walk ? THIEF_RUN : THIEF, 7, g2, 8, zap ? TZ : TI, true);
  } else {
    rect(12, 14, 15, 14, 0x587058), setPx(11, 14, 0xC09070), setPx(11, 13, 0x181818);
    if (t < 9 && random(100) < 35) setPx(11 + random(5), 13 + random(2), 0xFFF080);
  }

  // The hero: hidden, revealed, drops, fights, sprays, zips away unseen.
  static const Ink MI[] = {{'K', 0x060608}, {'W', 0xF0F0FF}, {'R', 0xE01020}, {0, 0}};
  float hx = 5, hy = 1, alpha = 1;
  if (t < 2.6f) alpha = 0.16f + 0.08f * sinf(t * 9);  // camouflage shimmer
  else if (t < 3.4f) alpha = lerpf(0.16f, 1, seg(2.6f, 3.4f));
  if (t > 3.6f) hy = lerpf(1, 8, ease(seg(3.6f, 4.1f)));
  if (t > 10.4f) hy = lerpf(8, -12, ease(seg(10.4f, 11.4f))), alpha = 1 - seg(10.7f, 11.3f);
  bool hang = t < 3.85f;
  // His web line: hanging, then zipping up.
  if (hang || t > 10.1f) {
    float top = hang ? hy : hy + 1, k = hang ? min(1.0f, alpha * 2) : max(0.0f, alpha);
    if (t > 10.1f && t < 10.4f) top = lerpf(9, -1, seg(10.1f, 10.4f));
    uint8_t g = 255 * k;
    for (float y = -1; y < top; y += 0.5f) addAA(hx + 2, y, scale(0x808088, g, g, g));
  }
  if (t < 2.6f && random(100) < 30) addAA(hx + rnd(0, 5), hy + rnd(0, 7), 0x182838);
  drawArtA(hang ? SPARKY_HANG : SPARKY, 7, hx, hy, MI, alpha);
  if (t > 4.1f && t < 4.35f) fbAdd(4, 14, 0x403040), fbAdd(10, 14, 0x403040);  // landing dust
  // Web shot at crook one.
  if (t > 4.4f && t < 5.6f) {
    float p = seg(4.4f, 4.8f);
    webLine(hx, hy + 4, lerpf(hx, 2.5f, p), lerpf(hy + 4, 11, p));
  }
  // Charge up, then an electric blast at crook two.
  if (t > 5.7f && t < 6.9f) {
    for (int i = 0; i < 3; i++) addAA(hx + 4.5f + rnd(-1, 1), hy + 4 + rnd(-1.2f, 1.2f), 0xFFE040);
    if (t > 6.2f) {
      boltLine(hx + 5, hy + 4, 13, 11, 0xFFF080);
      boltLine(hx + 5, hy + 4, 13, 12, 0x806010);
      for (int y = 6; y < 15; y++)  // the flash lights up the wall
        for (int x = 7; x < W; x++) {
          float d = sqrtf((x - 12.0f) * (x - 12.0f) + (y - 11.0f) * (y - 11.0f));
          if (d < 5) fbAdd(x, y, scale(0x605010, 255 * (1 - d / 5), 255 * (1 - d / 5), 255 * (1 - d / 5)));
        }
    }
  }
  // Spray can: arm up, paint mist drifting onto the wall.
  if (t > 7.8f && t < 9.8f) {
    int ax = roundf(hx) + 5, ay = roundf(hy) + 2;
    setPx(ax, ay + 1, 0x060608), setPx(ax, ay, 0xE01020), setPx(ax + 1, ay - 1, 0x909098);
    if (spray > 0 && spray < 1)
      for (int i = 0; i < 3; i++) {
        float f = rnd(0, 1), tx = 11 + spray * 5, ty = rnd(6, 11);
        fbAdd(roundf(lerpf(ax + 1, tx, f)), roundf(lerpf(ay - 1, ty, f)), (i % 2) ? 0x501040 : 0x105050);
      }
  }
  storyFade(LEN);
}

// A sprite drawn at s times its size.
void drawArtS(const char *const *rows, int n, float fx, float fy, const Ink *pal, int s) {
  int ox = roundf(fx), oy = roundf(fy);
  for (int y = 0; y < n; y++) {
    int len = strlen(rows[y]);
    for (int x = 0; x < len; x++)
      for (const Ink *p = pal; p->k; p++)
        if (p->k == rows[y][x]) {
          rect(ox + x * s, oy + y * s, ox + x * s + s - 1, oy + y * s + s - 1, p->c);
          break;
        }
  }
}

// ---- Tiny Titan: a hero in a red-and-black suit shrinks to ant size, rides a
// flying ant round a robot, grows to giant size to kick it away, then waves -
const char *const ANTMAN[] = {"K...K", ".LLL.", "LELEL", ".LLL.", "RKRKR", "RRKRR", ".KRK.", ".R.R.", ".K.K."};
const char *const MECH[] = {".KKKK.", "KEKKEK", "KKKKKK", ".KKKK.", "KKKKKK", "K.KK.K", "K.KK.K", ".K..K.", "KK..KK"};

// A flying ant, optionally with a tiny rider.
void flyingAnt(float x, float y, bool rider) {
  int ix = roundf(x), iy = roundf(y);
  bool wing = (int)(tsec * 20) % 2;
  if (wing) setPx(ix, iy - 1, 0xB0D0F0), setPx(ix - 1, iy - 1, 0x80A0C0);
  else setPx(ix - 1, iy, 0x80A0C0);
  setPx(ix, iy, 0x080808), setPx(ix + 1, iy, 0x141414), setPx(ix + 2, iy, 0x080808);
  if (rider) setPx(ix + 1, iy - 1, 0xC01010), setPx(ix + 1, iy - 2, 0xA0A8B8);
}

void fxAnt() {
  const float LEN = 12.5f;
  static bool kicked, zap1, zap2;
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    kicked = zap1 = zap2 = false;
  }
  float t = storyT, dt = sdt();
  // A sunny back garden, seen from down among the grass.
  skyGradient(0x3070D0, 0xA0D0F0, 15);
  disc(13.5f, 2, 1.4f, 0xFFF0A0);
  for (int x = 0; x < W; x++)
    for (int y = (x % 3 == 1 ? 6 : 7); y < 15; y++) fb[y][x] = x % 3 == 2 ? 0x3A2410 : 0x7A5028;
  rect(0, 15, W - 1, 15, 0x2A6018);
  for (int x = 0; x < W; x++) {
    if (fwHash(x * 9 + 1) > 0.4f) setPx(x, 14, 0x308020);
    if (fwHash(x * 9 + 1) > 0.75f) setPx(x, 13, 0x40A028);
  }
  int shx = 0;

  // The robot stomps in, gets zapped, then gets kicked into the sky.
  static const Ink BI[] = {{'K', 0x606070}, {'E', 0xFF2010}, {0, 0}};
  float mx = lerpf(18, 9, seg(0.5f, 2.5f)), my = 6;
  // The flying ant's loop around the robot.
  float ant = seg(3.8f, 6.6f), a = ant * 2 * PI * 1.5f;
  float lx = 11 + 5 * sinf(a), ly = 6 + 3.5f * sinf(2 * a);
  float in = ease(seg(3.8f, 4.3f)), out = ease(seg(6.1f, 6.6f));
  float antx = lerpf(lerpf(3, lx, in), 4, out), anty = lerpf(lerpf(14, ly, in), 14, out);
  if (t < 3.8f) antx = lerpf(-3, 3, seg(3.2f, 3.8f)), anty = 14;
  if (t > 9.8f) antx = lerpf(4, 19, seg(9.8f, 11)), anty = lerpf(14, 2, seg(9.8f, 11));
  if (!zap1 && t > 4.8f) zap1 = true, boomBurst(antx + 1, anty);
  if (!zap2 && t > 5.7f) zap2 = true, boomBurst(antx + 1, anty);
  bool jolt = (t > 4.8f && t < 5.1f) || (t > 5.7f && t < 6);
  if (t > 7.9f) {
    float p = seg(7.9f, 9);
    mx = lerpf(9, 22, p), my = lerpf(6, -6, p) + 6 * p * p;
  }
  if (!kicked && t > 7.9f) kicked = true, boomBurst(10, 10);
  drawArt(MECH, 9, mx + (jolt ? (int)(t * 30) % 2 : 0), my, BI, t > 7.9f ? (int)(t * 10) % 2 : antx < mx + 3);

  // The hero.
  static const Ink AI[] = {{'K', 0x181818}, {'L', 0xA0A8B8}, {'E', 0xFF2020}, {'R', 0xC01010}, {0, 0}};
  float hx = lerpf(-6, 1, seg(0.3f, 2));
  int ix = roundf(hx), iy = 6;
  float shrink = seg(2.6f, 3.1f), grow = seg(7, 7.5f), back = seg(9.2f, 9.7f);
  auto flick = [&](float s, float g) { return fmodf((t - s) * (4 + 12 * g), 1) < g; };
  int size = 1;  // 0 tiny, 1 normal, 2 giant
  if (t > 2.6f && t < 3.1f) size = flick(2.6f, shrink) ? 0 : 1;
  else if (t >= 3.1f && t < 7) size = 0;
  else if (t >= 7 && t < 7.5f) size = flick(7, grow) ? 2 : 0;
  else if (t >= 7.5f && t < 9.2f) size = 2;
  else if (t >= 9.2f && t < 9.7f) size = flick(9.2f, back) ? 1 : 2;
  bool riding = t > 3.8f && t < 6.6f;
  if (size == 1) {
    drawArt(ANTMAN, 9, ix, iy, AI);
    if (t > 10 && t < 11.8f) setPx(ix + 5, iy + ((int)(t * 4) % 2 ? 2 : 3), 0xC01010);  // waves
  } else if (size == 2) {
    int kx = t > 7.75f && t < 8.15f ? 1 : 0;
    drawArtS(ANTMAN, 9, -1 + kx, -2, AI, 2);
    if (t > 7.5f && t < 7.75f) shx = (int)(t * 40) % 3 - 1;
  } else if (!riding) setPx(3, 14, 0xC01010), setPx(3, 13, 0xA0A8B8);
  // Red sparkles while changing size.
  bool changing = (t > 2.6f && t < 3.2f) || (t > 7 && t < 7.6f) || (t > 9.2f && t < 9.8f);
  if (changing)
    for (int i = 0; i < 4; i++) addAA(rnd(ix - 1, ix + 6), rnd(iy, iy + 9), 0xFF3020);
  if (t > 3.2f && t < 11) flyingAnt(antx, anty, riding);
  if (t > 7.9f && t < 8.4f) shx = (int)(t * 40) % 3 - 1;
  boomStep(dt, 0x707078);
  fbShift(shx, 0);
  storyFade(LEN);
}

// ---- Storm Hammer: a caped thunder warrior lands in a storm, hurls his hammer
// at an ice giant, calls down lightning to shatter it, and flies off -------
const char *const THUNDER[] = {"..LLL.", "LYSSS.", ".YSSS.", "CCDDDS", "CDLDLS", "CDDDD.", "C.D.D.", "..D.D.", "..K.K."};
const char *const ICEGIANT[] = {"..BBB..", ".BEBEB.", "..BBB..", "BBBBBBB", "BLBBBLB", "BBBBBBB",
                                "B.BBB.B", "B.BBB.B", "..B.B..", "..B.B..", ".BB.BB."};

void drawHammer(float x, float y, bool glow) {
  int ix = roundf(x), iy = roundf(y);
  rect(ix - 1, iy, ix + 1, iy + 1, glow ? 0xC0E0FF : 0x8890A0);
  setPx(ix, iy + 2, 0x6A4020), setPx(ix, iy + 3, 0x6A4020);
}

void fxThunder() {
  const float LEN = 11.5f;
  static bool landed, hitG, shattered;
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    landed = hitG = shattered = false;
  }
  float t = storyT, dt = sdt();
  // A stormy night on a rocky clifftop, with rain.
  skyGradient(0x080A14, 0x242C44, 13);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < W; x++) {
      float d = sinf(x * 0.6f + tsec * 0.4f + y) + sinf(x * 0.23f - tsec * 0.3f) - y * 0.5f + 0.4f;
      if (d > 0) fb[y][x] = lerpColor(fb[y][x], 0x1A1C26, min(1.0f, d));
    }
  for (int y = 13; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = (x * 7 + y * 3) % 5 ? 0x2A2830 : 0x3A3842;
  for (int i = 0; i < 12; i++) {
    float rx = pmod(i * 3.7f + tsec * 3, 18) - 1, ry = pmod(i * 7.1f + tsec * 13, 14);
    addAA(rx, ry, 0x202838), addAA(rx - 0.25f, ry - 1, 0x141824);
  }
  // Lightning flashes light up the sky.
  float flash = 0;
  if (t > 0.3f && t < 0.5f) flash = 1;
  if (t > 4.3f && t < 4.8f) flash = (int)(t * 20) % 2 ? 1 : 0.4f;
  if (t > 6.6f && t < 7.5f) flash = (int)(t * 20) % 2 ? 1 : 0.5f;
  if (t > 9 && t < 9.2f) flash = 1;
  if (flash > 0)
    for (int y = 0; y < 13; y++)
      for (int x = 0; x < W; x++) fbAdd(x, y, scale(0x303448, 255 * flash, 255 * flash, 255 * flash));
  int shx = 0, shy = 0;

  // The hero drops out of the sky, then later flies off.
  float hx = 1, hy = 4;
  if (t < 1.6f) hy = lerpf(-12, 4, seg(0.6f, 1.6f) * seg(0.6f, 1.6f));
  if (t > 9) hy = lerpf(4, -14, seg(9, 10)), hx = lerpf(1, 4, seg(9, 10));
  if (!landed && t > 1.6f) {
    landed = true;
    for (int i = 0; i < 12; i++) boomAdd(3 + rnd(-1, 1), 12.5f, (i % 2 ? 1 : -1) * rnd(2, 6), -rnd(0.5f, 2), rnd(0.6f, 1.2f), 1);
  }
  if (t > 1.6f && t < 1.85f) shy = (int)(t * 40) % 2;
  int ix = roundf(hx), iy = roundf(hy);

  // The ice giant stomps in, is hit, and shatters.
  static const Ink GI[] = {{'B', 0x3070B0}, {'L', 0xA0D8FF}, {'E', 0xFF2010}, {0, 0}};
  static const Ink GW[] = {{'B', 0xE0F0FF}, {'L', 0xFFFFFF}, {'E', 0xFFFFFF}, {0, 0}};
  float gx = lerpf(18, 9, seg(1.8f, 3.2f));
  bool gstep = t > 1.8f && t < 3.2f && (int)(t * 4) % 2;
  if (!hitG && t > 5.4f) {
    hitG = true;
    for (int i = 0; i < 10; i++) boomAdd(12, 6, rnd(-3, 4), rnd(-4, 1), rnd(0.3f, 0.7f), 0);
  }
  if (!shattered && t > 7.4f) {
    shattered = true;
    for (int i = 0; i < 26; i++) boomAdd(gx + rnd(0, 7), rnd(2, 12), rnd(-4, 6), rnd(-6, 0), rnd(0.8f, 1.6f), 0);
    for (int i = 0; i < 8; i++) boomAdd(gx + rnd(1, 6), rnd(4, 11), rnd(-1, 1), rnd(-1.5f, -0.5f), rnd(1.5f, 2.5f), 1);
  }
  if (!shattered) {
    bool white = (t > 5.4f && t < 5.7f && (int)(t * 20) % 2) || (t > 6.9f && (int)(t * 16) % 2);
    drawArt(ICEGIANT, 11, gx + ((t > 5.4f && t < 5.8f) || t > 6.9f ? (int)(t * 30) % 2 : 0), 2 + gstep, white ? GW : GI);
  }
  if (t > 5.4f && t < 5.7f) shx = (int)(t * 40) % 3 - 1;
  if (t > 7.4f && t < 7.8f) shx = (int)(t * 40) % 3 - 1, shy = (int)(t * 30) % 2;

  // The hero, his hammer and the lightning.
  static const Ink TI[] = {{'L', 0xA8B0C0}, {'Y', 0xF0C030}, {'S', 0xF0B080}, {'C', 0xC01010}, {'D', 0x384058}, {'K', 0x483020}, {0, 0}};
  drawArt(THUNDER, 9, ix, iy, TI);
  bool raised = (t > 4 && t < 4.9f) || (t > 6.3f && t < 7.6f) || t > 9;
  float mxh = ix + 6, myh = iy + 2;  // hammer in hand
  bool glow = (t > 4.3f && t < 5.4f) || (t > 6.6f && t < 7.6f);
  if (raised) mxh = ix + 4, myh = iy - 4, setPx(ix + 4, iy + 2, 0xF0B080), setPx(ix + 4, iy, 0xF0B080);
  if (t > 5 && t < 5.9f) {  // thrown at the giant, then flies back
    float p = t < 5.4f ? seg(5, 5.4f) : 1 - seg(5.4f, 5.9f);
    mxh = lerpf(ix + 6, 12, p), myh = lerpf(iy + 2, 5, p);
  }
  if (t > 8.2f && t < 9) {  // spins it like a propeller before flying off
    float a = t * 18;
    mxh = ix + 5 + 2 * cosf(a), myh = iy + 3 + 2 * sinf(a);
    for (int k = 1; k < 4; k++) addAA(ix + 5 + 2 * cosf(a - k * 0.5f), iy + 4 + 2 * sinf(a - k * 0.5f), scale(0x8890A0, 200 - k * 50, 200 - k * 50, 200 - k * 50));
  }
  drawHammer(mxh, myh, glow && (int)(t * 16) % 2);
  if (t > 4.3f && t < 4.8f) boltLine(mxh + rnd(-1, 1), -1, mxh, myh, 0xE0E8FF);
  if (t > 6.6f && t < 7.4f) {
    boltLine(mxh, -1, mxh, myh, 0xE0E8FF);
    boltLine(mxh + 1, myh + 1, gx + 3.5f, 6, 0xE0E8FF);
    if ((int)(t * 12) % 2) boltLine(gx + 5, -1, gx + 3, 4, 0xA0B0FF);
  }
  if (t > 9 && t < 10) for (int k = 1; k < 5; k++) addAA(mxh, myh + 2 + k * 1.5f, scale(0xA0B0FF, 200 - k * 40, 200 - k * 40, 200 - k * 40));
  boomStep(dt, t > 7 ? 0xA0C8E0 : 0x605850, t > 5);
  fbShift(shx, shy);
  storyFade(LEN);
}

// ---- Arrow Ace: a purple-clad archer on a rooftop picks off a swarm of
// drones, the last one in mid-leap, then fires a signal flare --------------
const char *const ARCHER[] = {".KKK.", ".SSS.", "..S..", "PKPKP", "PPKPP", ".KKK.", ".K.K.", ".K.K.", ".P.P."};

void drawArrow(float x, float y, float dx, float dy) {
  addAA(x, y, 0xFFFFFF);
  addAA(x - dx, y - dy, 0x9090A0), addAA(x - 2 * dx, y - 2 * dy, 0x707080);
  addAA(x - 3 * dx, y - 3 * dy, 0xA040E0);
}

void fxArrow() {
  const float LEN = 11.5f;
  static bool fired[3], hit[3];
  static float ax0[3], ay0[3];
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    for (int i = 0; i < 3; i++) fired[i] = hit[i] = false;
  }
  float t = storyT, dt = sdt();
  // Sunset over the city, the hero on a rooftop to the left.
  skyGradient(0x182868, 0xF09858, 15);
  for (int x = 0; x < W; x++) {
    int top = 7 + (int)(fwHash(x / 2 + 91) * 4);
    for (int y = top; y < 15; y++) fb[y][x] = y > top && x % 2 && fwHash(x * 3 + y * 7) > 0.8f ? 0x806030 : 0x2A1C30;
  }
  for (int y = 11; y < 15; y++)
    for (int x = 0; x < 7; x++) fb[y][x] = y == 11 ? 0x504858 : x % 3 == 1 && y > 12 ? 0x705828 : 0x302838;
  rect(0, 15, W - 1, 15, 0x1C1820);

  // Three drones.
  static const Ink DI[] = {{'K', 0x40404C}, {'E', 0xFF2010}, {0, 0}};
  float dx[3], dy[3];
  dx[0] = lerpf(18, 11, ease(seg(1, 2.4f))), dy[0] = 1 + 0.6f * sinf(t * 2.3f);
  dx[1] = lerpf(18, 10, ease(seg(1.4f, 2.8f))), dy[1] = 8 + 0.6f * sinf(t * 2.8f + 1);
  dx[2] = lerpf(18, 11, ease(seg(1.8f, 3.2f))) + 1.5f * sinf(t * 1.7f), dy[2] = 4.5f + 2.5f * sinf(t * 1.3f);
  for (int i = 0; i < 3; i++)
    if (!hit[i]) drawArt(DRONE, 4, dx[i], dy[i], DI);

  // The hero: on the roof, then a flip down to the street.
  float hx = 1, hy = 2;
  bool flipped = false;
  if (t > 6.3f) {
    float p = seg(6.3f, 7.6f);
    hx = lerpf(1, 6, p), hy = lerpf(2, 6, p) - 4 * sinf(PI * p);
    flipped = p > 0.3f && p < 0.65f;
  }
  int ix = roundf(hx), iy = roundf(hy);
  static const Ink HI[] = {{'K', 0x202028}, {'S', 0xF0B080}, {'P', 0x7020B0}, {0, 0}};
  if (flipped) {
    const char *const UPSIDE[] = {ARCHER[8], ARCHER[7], ARCHER[6], ARCHER[5], ARCHER[4], ARCHER[3], ARCHER[2], ARCHER[1], ARCHER[0]};
    drawArt(UPSIDE, 9, ix, iy, HI);
  } else drawArt(ARCHER, 9, ix, iy, HI);

  // Shots: fire time, hit time.
  const float FT[3] = {3.4f, 4.6f, 6.85f}, HT[3] = {3.75f, 4.95f, 7.2f};
  float pull = 0;
  for (int i = 0; i < 3; i++) pull = max(pull, seg(FT[i] - 0.5f, FT[i] - 0.1f) * (t < FT[i] ? 1.0f : 0.0f));
  if (!flipped) {  // the bow, with the string drawn back before each shot
    int bx = ix + 5, by = iy + 2;
    uint32_t bow = 0x9040C0;
    setPx(bx, by, bow), setPx(bx + 1, by + 1, bow), setPx(bx + 1, by + 2, bow), setPx(bx + 1, by + 3, bow), setPx(bx, by + 4, bow);
    float sx = bx - 2 * pull, sy = by + 2;
    if (pull > 0) {
      for (float s = 0; s <= 1; s += 0.34f) addAA(lerpf(bx, sx, s), lerpf(by, sy, s), 0x383040), addAA(lerpf(bx, sx, s), lerpf(by + 4, sy, s), 0x383040);
      drawArrow(sx + 3, sy, 1, 0);
    } else for (int k = 1; k < 4; k++) setPx(bx, by + k, 0x302838);
  }
  for (int i = 0; i < 3; i++) {
    if (!fired[i] && t > FT[i]) fired[i] = true, ax0[i] = ix + 7, ay0[i] = iy + 4;
    float tx = dx[i] + 2, ty = dy[i] + 1.5f;
    if (fired[i] && !hit[i]) {
      float p = seg(FT[i], HT[i]), x = lerpf(ax0[i], tx, p), y = lerpf(ay0[i], ty, p);
      float l = sqrtf((tx - ax0[i]) * (tx - ax0[i]) + (ty - ay0[i]) * (ty - ay0[i])) + 0.01f;
      drawArrow(x, y, (tx - ax0[i]) / l, (ty - ay0[i]) / l);
    }
    if (!hit[i] && t > HT[i]) {
      hit[i] = true;
      boomBurst(tx, ty);
      if (i == 1) boomBurst(tx, ty);  // the explosive one
    }
  }
  // The last drone fires back before it goes down.
  if (t > 5.4f && t < 6.2f && !hit[2]) {
    float p = fmodf((t - 5.4f) * 2.5f, 1);
    float x0 = dx[2] + 2, y0 = dy[2] + 4, x1 = 3, y1 = 10.5f;
    if (p < 0.85f) addAA(lerpf(x0, x1, p), lerpf(y0, y1, p), 0xFF2010);
    else fbAdd(3, 10, 0xFFC040), fbAdd(4, 10, 0xFF8020);
  }
  // A flare arrow straight up that bursts in purple stars.
  if (t > 8.6f && t < 9) drawArrow(ix + 2, lerpf(iy - 1, -1, seg(8.6f, 9)), 0, -1);
  if (t > 9 && t < 10.6f) {
    float p = seg(9, 10.6f), r = 1 + 4 * sqrtf(p), k = 1 - p;
    for (int i = 0; i < 10; i++) {
      float a = i * PI / 5 + 0.3f;
      addAA(ix + 2 + r * cosf(a), 4 + r * sinf(a) + 1.5f * p, scale(i % 2 ? 0xC060FF : 0xFFFFFF, 255 * k, 255 * k, 255 * k));
    }
  }
  if (t > 8.4f && t < 9) setPx(ix + 3, iy + 2, 0xF0B080), setPx(ix + 3, iy + 1, 0xF0B080);  // arm up
  boomStep(dt, 0x707078);
  int sh = 0;
  for (int i = 0; i < 3; i++)
    if (t > HT[i] && t < HT[i] + (i == 1 ? 0.3f : 0.12f)) sh = (int)(t * 40) % 3 - 1;
  fbShift(sh, 0);
  storyFade(LEN);
}

// ---- Bark & Blaster: on an alien jungle world, a walking tree and a raccoon
// with a huge blaster fight off alien bugs, the tree fills the night with
// glowing spores, and the two of them dance ---------------------------------
const char *const TREEGUY[] = {".L.L..", ".BBBB.", ".BBBB.", ".EBBE.", ".BBBB.", "..BB..", "BBBBBB",
                               "B.BB.B", "B.BB.B", "D.BB.D", "..BB..", ".B..B.", "BB..BB"};
const char *const TREEGUY_UP[] = {"DL.L..", "BBBBB.", "B.BBBB", ".EBBE.", ".BDDB.", "..BB..", ".BBBBB",
                                  "..BB.B", "..BB.B", "..BB.D", "..BB..", ".B..B.", "BB..BB"};
const char *const RACC[] = {".G..G.", ".GGGG.", "KWKKWK", ".GWWN.", "..OOO.", "TTOOO.", "T..O.O"};
const char *const ABUG[] = {".YY.", "RRRR", "R..R"};
const char *const ABUG_HOP[] = {".YY.", "RRRR", ".RR."};

void fxBark() {
  const float LEN = 14;
  static bool dead[3];
  if (storyStart(LEN)) {
    for (auto &b : booms) b.life = 0;
    dead[0] = dead[1] = dead[2] = false;
  }
  float t = storyT, dt = sdt();
  // Night on an alien jungle world: two moons, a dark canopy, glowing plants.
  skyGradient(0x0A0420, 0x2A1048, 15);
  disc(11.5f, 3, 1.7f, 0x9888C0);
  disc(14.6f, 1.2f, 0.8f, 0xC08068);
  for (int x = 0; x < W; x++) {
    int top = 7 + (int)(2.5f * (1 + sinf(x * 0.9f + 1)) * fwHash(x + 5));
    for (int y = top; y < 15; y++) fb[y][x] = y == top ? 0x0C2018 : 0x050C08;
  }
  rect(0, 15, W - 1, 15, 0x180C1C);
  for (int i = 0; i < 5; i++) {  // glowing plants
    int px = 1 + (int)(fwHash(i * 13 + 2) * 14);
    float g = 0.6f + 0.4f * sinf(tsec * 1.5f + i * 2);
    setPx(px, 14, scale(0x20E0C0, 200 * g, 200 * g, 200 * g));
  }
  bool dance = t > 10.8f;
  // The spores: dozens of glowing seeds drifting up into the night.
  if (t > 7.5f && t < 11.5f)
    for (int i = 0; i < 22; i++) {
      float st = 7.5f + fwHash(i * 7 + 1) * 1.6f, age = t - st;
      if (age < 0) continue;
      float k = min(1.0f, age * 2) * (1 - seg(10.5f, 11.5f)) * (0.6f + 0.4f * sinf(tsec * 4 + i));
      float sx = 1 + fwHash(i * 3 + 9) * 14 + 0.8f * sinf(age * 1.3f + i), sy = 13 - age * (1.2f + fwHash(i) * 1.5f);
      addAA(sx, sy, scale(0xC0FF60, 230 * k, 230 * k, 230 * k));
    }

  // Alien bugs: two scuttle in along the ground, one swoops from the sky.
  static const Ink BI[] = {{'R', 0xD04018}, {'Y', 0xFFE020}, {0, 0}};
  float bx[3], by[3];
  bx[0] = lerpf(18, 13, seg(2.2f, 3.6f)), by[0] = 12;
  bx[1] = lerpf(22, 14, seg(2.6f, 5)), by[1] = 12;
  float sw = seg(5.6f, 6.6f);
  bx[2] = lerpf(17, 10, sw), by[2] = lerpf(-2, 4.5f, sw);
  const float DIE[3] = {4.4f, 5.3f, 6.6f};
  for (int i = 0; i < 3; i++) {
    if (!dead[i] && t > DIE[i]) dead[i] = true, boomBurst(bx[i] + 2, by[i] + 1);
    bool hop = (int)(t * 6 + i) % 2;
    if (!dead[i]) drawArt(hop ? ABUG_HOP : ABUG, 3, bx[i], by[i] - (i < 2 && hop ? 1 : 0), BI);
  }

  // The walking tree.
  static const Ink TI[] = {{'B', 0x6A4424}, {'D', 0x2A180A}, {'L', 0x40B030}, {'E', 0x100800}, {0, 0}};
  int tx = 0, ty = 2;
  if (dance) {
    int beat = (int)((t - 10.8f) * 3);
    tx = beat % 2;
    drawArt(TREEGUY_UP, 13, tx, ty, TI, beat % 2);
  } else {
    drawArt(TREEGUY, 13, tx, ty, TI);
  }
  if (t > 7.5f && t < 11) setPx(tx + 3, ty, 0xC0FF60);  // a glowing bud on his head
  // His arm stretches out into a long branch to swat the swooping bug.
  float reach = ease(seg(6.15f, 6.6f)) * (1 - ease(seg(6.9f, 7.5f)));
  if (reach > 0) {
    float x0 = tx + 5, y0 = ty + 6, x1 = lerpf(x0, bx[2] + 1, reach), y1 = lerpf(y0, by[2] + 1, reach);
    for (float s = 0; s <= 1; s += 0.1f) setPx(roundf(lerpf(x0, x1, s)), roundf(lerpf(y0, y1, s)), 0x6A4424);
    setPx(roundf(x1), roundf(y1) - 1, 0x40B030), setPx(roundf(lerpf(x0, x1, 0.5f)), roundf(lerpf(y0, y1, 0.5f)) - 1, 0x40B030);
  }

  // The raccoon and his blaster.
  static const Ink RI[] = {{'G', 0xA8A8B0}, {'K', 0x101010}, {'W', 0xF0F0F0}, {'N', 0x000000}, {'T', 0x706050}, {'O', 0xE07018}, {0, 0}};
  float rx = lerpf(-7, 6, seg(0.5f, 2));
  int bob = dance ? (int)((t - 10.8f) * 6) % 2 : 0;
  int ix = roundf(rx), iy = 8 - bob;
  drawArt(RACC, 7, ix, iy, RI);
  if ((int)(t * 3) % 2) setPx(ix, iy + 5, 0x302820);  // tail stripes flicker as it swishes
  if (!dance) {  // the blaster
    rect(ix + 3, iy + 5, ix + 6, iy + 5, 0x505868);
    rect(ix + 4, iy + 4, ix + 6, iy + 4, 0x8088A0);
    setPx(ix + 7, iy + 4, 0x404050);
    bool firing = (t > 3.9f && t < 4.4f) || (t > 4.8f && t < 5.3f);
    if (firing) {
      float p = fmodf(t * 6, 1);
      int tgt = t < 4.4f ? 0 : 1;
      float x0 = ix + 8, y0 = iy + 4, x1 = bx[tgt] + 2, y1 = by[tgt] + 1;
      if ((int)(t * 12) % 2) setPx(ix + 8, iy + 4, 0xFFE080);  // muzzle flash
      addAA(lerpf(x0, x1, p), lerpf(y0, y1, p), 0xFF8020), addAA(lerpf(x0, x1, p) - 1, lerpf(y0, y1, p), 0x802000);
    }
  } else if ((int)(t * 2) % 2) {
    // Music notes float up while they dance.
    float ny = 8 - fmodf(t * 2, 1) * 4;
    setPx(9, ny, 0xFFD040), setPx(10, ny - 1, 0xFFD040), setPx(10, ny, 0xFFD040);
  }
  boomStep(dt, 0x605070);
  int sh = 0;
  for (int i = 0; i < 3; i++)
    if (t > DIE[i] && t < DIE[i] + 0.15f) sh = (int)(t * 40) % 3 - 1;
  fbShift(sh, 0);
  storyFade(LEN);
}

bool isStory(uint8_t fx) { return (fx >= FX_GARDEN && fx <= FX_SNOWMAN) || (fx >= FX_HATCH && fx <= FX_BARK); }

// ---- Dream: a never-ending series of random generative patterns ------------
struct DreamRecipe {
  uint8_t kind, sym, scheme;
  float f1, f2, f3, spd, rot;
  uint16_t hue;
} dream;
float dreamAge, dreamLen;
struct DreamDot { float x, y; } dots[16];
uint32_t dreamColor(float v, uint8_t val = 255) {
  v -= floorf(v);
  uint16_t h = dream.hue;
  uint8_t sat = 255;
  switch (dream.scheme) {
    case 0: h += (int32_t)(6000 * sinf(v * TWO_PI)); break;                   // neighbouring colours
    case 1: h += (int32_t)(32768 * (0.5f - 0.5f * cosf(v * TWO_PI))); break;  // two opposite colours
    case 2: h += (int32_t)(v * 65535); break;                                 // the whole rainbow
    default: sat = 120 + 135 * (0.5f + 0.5f * sinf(v * TWO_PI)); break;  // one colour, pale to deep
  }
  return hsv(h + (uint16_t)(uint32_t)(tsec * 300), sat, val);
}

uint8_t crystal[H][W];
uint32_t crystalCol[H][W];
int crystalCount;

void crystalSeed() {
  memset(crystal, 0, sizeof(crystal));
  for (int y = 7; y <= 8; y++)
    for (int x = 7; x <= 8; x++) crystal[y][x] = 1, crystalCol[y][x] = dreamColor(0);
  crystalCount = 4;
}

void newDream() {
  dream.kind = random(7);
  dream.sym = random(4);
  dream.scheme = random(4);
  dream.f1 = rnd(0.3f, 1.2f), dream.f2 = rnd(0.3f, 1.2f), dream.f3 = rnd(0.2f, 1);
  dream.spd = rnd(0.5f, 1.6f);
  dream.rot = random(2) ? rnd(-0.4f, 0.4f) : 0;
  dream.hue = random(65536);
  dreamAge = 0;
  dreamLen = rnd(15, 25);
  for (auto &d : dots) d = {rnd(0, W), rnd(0, H)};
  crystalSeed();
  fill(0);
}

// Plots a point plus its mirror images for the current symmetry.
void dreamPlot(int x, int y, uint32_t c) {
  addPx(x, y, c);
  if (dream.sym >= 1) addPx(W - 1 - x, y, c);
  if (dream.sym >= 2) addPx(x, H - 1 - y, c), addPx(W - 1 - x, H - 1 - y, c);
  if (dream.sym == 3) addPx(y, x, c), addPx(W - 1 - y, H - 1 - x, c);
}

void fxDream() {
  if (fxFresh) newDream();
  dreamAge += frameDt;
  if (dreamAge > dreamLen) newDream();
  float T = tsec * dream.spd;
  if (dream.kind < 5) {
    float ca = cosf(T * dream.rot), sa = sinf(T * dream.rot);
    for (int py = 0; py < H; py++)
      for (int px = 0; px < W; px++) {
        float x0 = px - 7.5f, y0 = py - 7.5f;
        float x = x0 * ca - y0 * sa, y = x0 * sa + y0 * ca;
        if (dream.sym >= 1) x = fabsf(x);
        if (dream.sym >= 2) y = fabsf(y);
        if (dream.sym == 3 && y > x) { float s = x; x = y; y = s; }
        float d = sqrtf(x * x + y * y), a = atan2f(y, x), v;
        switch (dream.kind) {
          case 0:  // plasma
            v = (sinf(x * dream.f1 + T) + sinf(y * dream.f2 - T * 1.3f) + sinf((x + y) * dream.f3 + T * 0.7f) +
                 sinf(d * dream.f1 - T)) / 8 + 0.5f;
            break;
          case 1:  // spiral arms
            v = a * (1 + (int)(dream.f3 * 4)) / TWO_PI + d * dream.f1 * 0.15f - T * 0.2f;
            break;
          case 2: {  // two wandering ripple centres
            float d1 = hypotf(x - 4 * sinf(T * 0.5f), y - 4 * cosf(T * 0.4f));
            float d2 = hypotf(x + 4 * sinf(T * 0.3f), y + 3 * cosf(T * 0.6f));
            v = 0.5f + 0.25f * (sinf(d1 * dream.f1 * 1.5f - T * 2) + sinf(d2 * dream.f2 * 1.5f - T * 2));
            break;
          }
          case 3:  // warped stripes
            v = 0.5f + 0.5f * sinf(x * dream.f1 + 2 * sinf(y * dream.f2 + T) + T * 0.5f);
            break;
          default:  // tunnel
            v = dream.f1 * 3 / (d + 1.5f) + T * 0.3f + a / TWO_PI * (1 + (int)(dream.f3 * 3));
            break;
        }
        float band = 0.5f + 0.5f * sinf(v * TWO_PI * (1 + dream.scheme % 2));
        fb[py][px] = dreamColor(v * 0.5f, 50 + 205 * band);
      }
  } else if (dream.kind == 5) {  // glowing trails following an invisible current
    fadeAll(225, 225, 225);
    for (int i = 0; i < 16; i++) {
      DreamDot &p = dots[i];
      float ang = 2 * sinf(p.x * dream.f1 * 0.4f + T) + 2 * cosf(p.y * dream.f2 * 0.4f - T * 0.8f);
      p.x += cosf(ang) * 6 * sdt();
      p.y += sinf(ang) * 6 * sdt();
      p.x = fmodf(p.x + W, W), p.y = fmodf(p.y + H, H);
      dreamPlot(p.x, p.y, dreamColor(i / 16.0f + T * 0.05f));
    }
  } else {  // a crystal grows from the centre, then dissolves
    if (crystalCount < 150) {
      for (int w = 0; w < 4; w++) {
        int x = random(W), y = random(2) ? 0 : H - 1;
        if (random(2)) { int s = x; x = y; y = s; }
        for (int step = 0; step < 300; step++) {
          x = constrain(x + (int)random(3) - 1, 0, W - 1);
          y = constrain(y + (int)random(3) - 1, 0, H - 1);
          bool touch = false;
          for (int dy = -1; dy <= 1 && !touch; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              int nx = x + dx, ny = y + dy;
              if (nx >= 0 && ny >= 0 && nx < W && ny < H && crystal[ny][nx]) { touch = true; break; }
            }
          if (touch && !crystal[y][x]) {
            uint32_t c = dreamColor(crystalCount / 150.0f);
            int pts[4][2] = {{x, y}, {W - 1 - x, y}, {x, H - 1 - y}, {W - 1 - x, H - 1 - y}};
            for (int k = 0; k < (dream.sym ? 4 : 1); k++)
              if (!crystal[pts[k][1]][pts[k][0]]) {
                crystal[pts[k][1]][pts[k][0]] = 1;
                crystalCol[pts[k][1]][pts[k][0]] = c;
                crystalCount++;
              }
            break;
          }
        }
      }
    } else if (++crystalCount > 150 + 90) {
      crystalSeed();
    }
    uint8_t dim = crystalCount > 150 ? 255 * (1 - (crystalCount - 150) / 90.0f) : 255;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        uint8_t v = dim * (0.7f + 0.3f * sinf(tsec * 3 + x * 0.9f + y * 1.3f));
        fb[y][x] = crystal[y][x] ? scale(crystalCol[y][x], v, v, v) : 0;
      }
  }
  // Fade between dreams.
  float f = min(1.0f, min(dreamAge / 1.2f, (dreamLen - dreamAge) / 1.2f));
  if (f < 1) {
    uint8_t v = max(0.0f, f) * 255;
    fadeAll(v, v, v);
  }
}
