// More calm effects: gentle nature scenes, slow patterns, and a breathing guide.
// Included from MatrixPanel.ino after stories.h.

// Gamma-corrects the whole frame, for deeper, richer colours on the LEDs.
void gammaAll() {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = Adafruit_NeoPixel::gamma32(fb[y][x]);
}
inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline float smooth01(float v) {
  v = clamp01(v);
  return v * v * (3 - 2 * v);
}

// ============================================================================
// Nature scenes
// ============================================================================

inline float sqf(float v) { return v * v; }
// Blends colour c over the pixel by amount a (0..1).
inline void blendPx(int x, int y, uint32_t c, float a) {
  if (x >= 0 && y >= 0 && x < W && y < H && a > 0) fb[y][x] = lerpColor(fb[y][x], c, a);
}
// Adds a soft round glow of colour c that fades out at radius r.
void softGlow(float cx, float cy, float r, uint32_t c, float k = 1) {
  for (int y = floorf(cy - r); y <= ceilf(cy + r); y++)
    for (int x = floorf(cx - r); x <= ceilf(cx + r); x++) {
      float e = clamp01(1 - sqrtf(sqf(x - cx) + sqf(y - cy)) / r);
      e = clamp01(e * e * k);
      if (e > 0.004f) fbAdd(x, y, scale(c, 255 * e, 255 * e, 255 * e));
    }
}
// Smooth value noise, roughly 0..1.
float vnoise(float x, float y) {
  int ix = floorf(x), iy = floorf(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx), fy = fy * fy * (3 - 2 * fy);
  auto h = [](int a, int b) { return fwHash((uint32_t)(a * 7919 + b * 104729 + 1013)); };
  return lerpf(lerpf(h(ix, iy), h(ix + 1, iy), fx), lerpf(h(ix, iy + 1), h(ix + 1, iy + 1), fx), fy);
}

// ---- Fireflies: soft lights drifting and blinking over a meadow at dusk ---
void fxFireflies() {
  float t = tsec;
  // Deep blue dusk sky, a few stars and a low moon.
  skyGradient(0x0A1448, 0x2A4C8C, 11);
  for (int i = 0; i < 7; i++) {
    int sx = fwHash(i * 5 + 600) * 16, sy = fwHash(i * 5 + 601) * 5;
    float tw = 0.5f + 0.5f * sinf(t * (0.8f + fwHash(i + 610)) + i * 2);
    fbAdd(sx, sy, scale(0x9098C0, 150 * tw, 150 * tw, 150 * tw));
  }
  softGlow(12.5f, 2.5f, 3.5f, 0x403C30);
  disc(12.5f, 2.5f, 1.2f, 0xF0E8C0);
  // Tree line: pines and rounded trees, black against the sky.
  static const float PINE[][3] = {{1, 3, 2.2f}, {4.5f, 5, 1.8f}, {9, 4, 2.4f}, {14.5f, 2.5f, 2.2f}};
  for (int x = 0; x < W; x++) {
    int top = 8 + (int)(fwHash(x + 640) * 1.6f);
    for (auto &p : PINE) {
      float w = fabsf(x - p[0]);
      if (w < p[2]) top = min(top, (int)ceilf(p[1] + w / p[2] * (8 - p[1])));
    }
    for (int y = top; y < 12; y++) fb[y][x] = 0x020604;
  }
  // The meadow, with grass tips swaying against the trees.
  for (int y = 11; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = lerpColor(0x184620, 0x22582A, (y - 11) / 4.0f);
  for (int x = 0; x < W; x++) {
    float sw = sinf(t * 0.9f + x * 0.7f);
    if (fwHash(x * 3 + 650) > 0.35f) setPx(x + (sw > 0.6f), 10, 0x164020);
    if (fwHash(x * 3 + 651) > 0.65f) setPx(x, 9, 0x12381A);
  }
  // Every half minute a wave of light ripples through them all together.
  float wave = pmod(t, 32);
  for (int i = 0; i < 18; i++) {
    float a = fwHash(i * 11 + 1), b = fwHash(i * 11 + 2), c = fwHash(i * 11 + 3);
    bool far = i >= 13;  // a few far off, among the trees
    float x = 8 + 7 * sinf(t * (0.05f + 0.05f * a) + a * 20) + 1.2f * sinf(t * 0.37f + b * 9);
    float y = far ? 7.5f + 1.5f * sinf(t * 0.08f + b * 20)
                  : 10 + 3.5f * sinf(t * (0.04f + 0.05f * b) + b * 20) + 0.9f * sinf(t * 0.29f + c * 9);
    // A quick brightening, a slow fade, then a rest, each on its own rhythm.
    float period = 3.5f + 4 * c, ph = pmod(t + a * 17, period);
    float glow = ph < 0.25f ? smooth01(ph / 0.25f) : 1 - smooth01((ph - 0.25f) / 1.35f);
    if (wave < 4) glow = max(glow, expf(-sqf((wave * 5.5f - 1 - x) / 1.8f)));
    if (glow <= 0.02f) continue;
    if (far) glow *= 0.5f;
    softGlow(x, y, far ? 1.6f : 2.6f, 0x60C000, glow * 1.5f);
    addAA(x, y, scale(0xE0FF60, 255 * glow, 255 * glow, 255 * glow));
    if (!far && y > 10) softGlow(x, y + 1.5f, 2.2f, 0x305818, glow);  // lighting the grass below
  }
  gammaAll();
}

// ---- Koi Pond: koi gliding under lily pads, with the odd ripple ----------
// Distance from (px, py) to a fish's spine, and how far along it (0 head, 1 tail).
void nearSeg(const float *X, const float *Y, int n, float px, float py, float &d, float &s) {
  d = 99, s = 0;
  for (int i = 0; i < n - 1; i++) {
    float ex = X[i + 1] - X[i], ey = Y[i + 1] - Y[i], l2 = ex * ex + ey * ey + 1e-4f;
    float u = clamp01(((px - X[i]) * ex + (py - Y[i]) * ey) / l2);
    float dd = sqrtf(sqf(px - X[i] - ex * u) + sqf(py - Y[i] - ey * u));
    if (dd < d) d = dd, s = (i + u) / (n - 1);
  }
}
void fxKoi() {
  float t = tsec;
  // Green-blue water, darker at the edges, with slow light patterns.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float s = sinf(x * 0.5f + t * 0.4f + sinf(y * 0.4f + t * 0.3f)) * sinf(y * 0.45f - t * 0.35f);
      float edge = clamp01((fabsf(x - 7.5f) + fabsf(y - 7.5f) - 6) / 7);
      fb[y][x] = lerpColor(lerpColor(0x146A74, 0x208A90, 0.5f + 0.45f * s), 0x0C4654, edge);
    }
  // Three koi on slow looping paths. Each spine is traced back along the
  // path, one point every 0.8 pixels, with a swish towards the tail.
  const int N = 6;
  float SX[3][N], SY[3][N];
  for (int f = 0; f < 3; f++) {
    float w1 = 0.11f + f * 0.025f, w2 = 0.16f + f * 0.02f, p = f * 2.1f;
    auto pos = [&](float tt, float &x, float &y) {
      x = 8 + 6.5f * sinf(tt * w1 + p) + 0.6f * sinf(tt * 0.5f + f), y = 8 + 6.5f * sinf(tt * w2 + p * 1.7f);
    };
    float tt = t, px = 0, py = 0;
    pos(t, SX[f][0], SY[f][0]);
    for (int k = 1; k < N; k++) {
      for (int i = 0; i < 60; i++) {
        tt -= 0.05f;
        pos(tt, px, py);
        if (sqf(px - SX[f][k - 1]) + sqf(py - SY[f][k - 1]) >= 0.56f) break;
      }
      SX[f][k] = px, SY[f][k] = py;
    }
    for (int k = 1; k < N; k++) {
      float ex = SX[f][k] - SX[f][k - 1], ey = SY[f][k] - SY[f][k - 1], l = sqrtf(ex * ex + ey * ey) + 1e-3f;
      float sw = sinf(t * 4.5f + f * 2 - k * 0.9f) * 0.09f * k;
      SX[f][k] += -ey / l * sw, SY[f][k] += ex / l * sw;
    }
  }
  auto halfW = [](float s) { return s < 0.2f ? 0.95f + s * 1.75f : max(0.2f, 1.3f - (s - 0.2f) * 1.4f); };
  // Their shadows on the pond floor first.
  for (int f = 0; f < 3; f++)
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float d, s;
        nearSeg(SX[f], SY[f], N, x - 1, y - 1.4f, d, s);
        blendPx(x, y, 0x06303C, clamp01(halfW(s) - d + 0.5f) * 0.4f);
      }
  static const uint32_t BODY[3] = {0xFFF4EC, 0xFF6A10, 0xFFD848};
  static const uint32_t MARK[3] = {0xF02010, 0xFFB050, 0xFFF4C0};
  for (int f = 0; f < 3; f++) {
    float ex = SX[f][N - 1] - SX[f][N - 2], ey = SY[f][N - 1] - SY[f][N - 2], l = sqrtf(ex * ex + ey * ey) + 1e-3f;
    ex /= l, ey /= l;
    // The tail fin fans out behind, flicking from side to side.
    float flick = sinf(t * 4.5f + f * 2 - 6.5f) * 0.5f;
    for (int side = -1; side <= 1; side += 2) {
      float tx = SX[f][N - 1] + ex * 1.3f - ey * (side * 0.9f + flick), ty = SY[f][N - 1] + ey * 1.3f + ex * (side * 0.9f + flick);
      for (float q = 0.3f; q <= 1; q += 0.35f)
        blendPx(roundf(lerpf(SX[f][N - 1], tx, q)), roundf(lerpf(SY[f][N - 1], ty, q)), scale(BODY[f], 200, 200, 200), 0.55f);
    }
    // Little side fins near the head.
    float hx = SX[f][1] - SX[f][0], hy = SY[f][1] - SY[f][0], hl = sqrtf(hx * hx + hy * hy) + 1e-3f;
    float fin = 1.5f + 0.25f * sinf(t * 3 + f);
    for (int side = -1; side <= 1; side += 2)
      blendPx(roundf(SX[f][1] - hy / hl * fin * side), roundf(SY[f][1] + hx / hl * fin * side), BODY[f], 0.45f);
    // The body: rounded, widest behind the head, tapering to the tail.
    int x0 = 99, x1 = -99, y0 = 99, y1 = -99;
    for (int k = 0; k < N; k++)
      x0 = min(x0, (int)SX[f][k] - 2), x1 = max(x1, (int)SX[f][k] + 2), y0 = min(y0, (int)SY[f][k] - 2), y1 = max(y1, (int)SY[f][k] + 2);
    for (int y = max(0, y0); y <= min(H - 1, y1); y++)
      for (int x = max(0, x0); x <= min(W - 1, x1); x++) {
        float d, s;
        nearSeg(SX[f], SY[f], N, x, y, d, s);
        float w = halfW(s), a = clamp01(w - d + 0.5f);
        if (a <= 0) continue;
        uint32_t c = BODY[f];
        if (f == 0 && (s < 0.12f || sinf(s * 12 + 1.3f) > 0.35f)) c = MARK[0];  // red patches
        if (f == 1 && d < 0.45f) c = MARK[1];                                        // a pale stripe
        if (f == 2) c = lerpColor(c, MARK[2], 0.5f + 0.5f * sinf(t * 2 + s * 7));    // a golden sheen
        c = lerpColor(c, scale(c, 175, 175, 185), clamp01(d / max(0.3f, w)) * 0.6f);
        blendPx(x, y, c, a * 0.95f);
      }
  }
  // Lily pads, each with its notch, and a pink flower.
  static const float PAD[3][4] = {{3, 3, 2.3f, 0.3f}, {12.5f, 11.5f, 2.7f, 2.4f}, {13, 3.5f, 1.7f, 4}};
  for (int i = 0; i < 3; i++) {
    float px = PAD[i][0] + 0.25f * sinf(t * 0.2f + i), py = PAD[i][1] + 0.25f * cosf(t * 0.17f + i * 2), R = PAD[i][2];
    for (int y = floorf(py - R - 1); y <= ceilf(py + R + 1); y++)
      for (int x = floorf(px - R - 1); x <= ceilf(px + R + 1); x++) {
        float dx = x - px, dy = y - py, d = sqrtf(dx * dx + dy * dy);
        float da = fabsf(pmod(atan2f(dy, dx) - PAD[i][3] + PI, 2 * PI) - PI);
        float a = clamp01(R - d + 0.5f) * clamp01((da - 0.3f) * 4 + (d < 0.6f));
        if (a <= 0) continue;
        uint32_t c = lerpColor(0x3A9A2C, 0x206A1C, d / R);
        if (fabsf(fmodf(atan2f(dy, dx) + 7, 1.2f) - 0.6f) < 0.12f && d > 0.8f) c = lerpColor(c, 0x58B040, 0.5f);  // veins
        blendPx(x, y, c, a);
      }
  }
  setPx(12, 11, 0xFF88B8), setPx(13, 11, 0xFFB0D0), setPx(12, 12, 0xFF70A8), setPx(13, 12, 0xFF88B8);
  setPx(12, 10, 0xFFC0DC), setPx(11, 11, 0xFFA0C8), setPx(14, 12, 0xFF90C0), setPx(13, 13, 0xFF80B0);
  blendPx(12, 11, 0xFFE060, 0.6f), blendPx(13, 12, 0xFFE060, 0.3f);
  // Now and then a koi noses the surface and sends out a ring.
  float rp = pmod(t, 7) / 7;
  int n = (int)(t / 7), rf = n % 3;
  float rr = rp * 4.5f, rk = (1 - rp) * (1 - rp);
  static float rx, ry;
  static int lastN = -1;
  if (n != lastN) lastN = n, rx = SX[rf][0], ry = SY[rf][0];
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float d = sqrtf(sqf(x - rx) + sqf(y - ry));
      float e = clamp01(1 - fabsf(d - rr) * 1.4f) + 0.5f * clamp01(1 - fabsf(d - rr * 0.6f) * 1.6f);
      if (e > 0) fbAdd(x, y, scale(0x507070, 255 * e * rk, 255 * e * rk, 255 * e * rk));
    }
  // A few glints of sky on the water.
  for (int i = 0; i < 3; i++) {
    float g = sinf(t * (0.9f + i * 0.3f) + i * 2);
    if (g > 0.85f) fbAdd(fwHash(i + (int)(t / 7) * 3 + 900) * 16, fwHash(i + 950 + (int)(t / 7)) * 16, scale(0xFFFFFF, (g - 0.85f) * 900, (g - 0.85f) * 900, (g - 0.85f) * 900));
  }
  gammaAll();
}

// ---- Falling Leaves: leaves (or spring blossom) drifting down -------------
struct Leaf { float x, y, v, ph, sw; uint8_t c; };
Leaf leaves[12];
void fxLeaves() {
  float dt = sdt(), t = tsec;
  // Autumn for a while, then spring blossom, and round again.
  float season = pmod(t / 40, 2), spring = smooth01((season > 1 ? 2 - season : season) * 4 - 1.5f);
  static const uint32_t AUT[4] = {0xD02010, 0xF06010, 0xF0B020, 0xA03008};
  static const uint32_t SPR[4] = {0xFFA0C0, 0xFFD0E0, 0xFF80A8, 0xFFFFFF};
  skyGradient(lerpColor(0x182040, 0x2060B0, spring), lerpColor(0x804838, 0x90C8F0, spring), 15);
  if (fxFresh)
    for (auto &l : leaves) l = {rnd(0, W), rnd(-16, 15), rnd(1.2f, 2.2f), rnd(0, 6.28f), rnd(1, 2.2f), (uint8_t)random(4)};
  // A branch across the top.
  for (int x = 0; x < 11; x++) setPx(x, 1 + x / 4, 0x2A1808);
  for (int x = 0; x < 10; x += 2) setPx(x + 1, 2 + x / 4, lerpColor(AUT[x % 4], SPR[x % 4], spring));
  for (int x = 0; x < W; x++) setPx(x, 15, lerpColor(0x3A2010, 0x2A6020, spring));
  for (auto &l : leaves) {
    l.y += l.v * dt, l.ph += dt * 1.6f;
    if (l.y > 16) l = {rnd(-2, 12), rnd(-4, 0), rnd(1.2f, 2.2f), rnd(0, 6.28f), rnd(1, 2.2f), (uint8_t)random(4)};
    float x = l.x + l.sw * sinf(l.ph), tilt = cosf(l.ph);
    uint32_t c = lerpColor(AUT[l.c], SPR[l.c], spring);
    // A leaf is two pixels, tipping as it swings; blossom is a single petal.
    addAA(x, l.y, c);
    if (spring < 0.5f) addAA(x + (tilt > 0 ? 0.8f : -0.8f), l.y - 0.5f * fabsf(tilt), scale(c, 170, 170, 170));
  }
  gammaAll();
}

// ---- Candle: a single flame flickering, its glow warming the room -------
void fxCandle() {
  float t = tsec;
  // Now and then a draught makes the flame dance for a moment; afterwards a
  // thin curl of smoke drifts up.
  int gn = (int)(t / 14);
  float gp = pmod(t, 14);
  bool gusty = fwHash(gn + 700) > 0.35f;
  float gust = gusty ? sinf(PI * clamp01((gp - 6) / 2.5f)) : 0;
  float fl = 0.9f + 0.05f * sinf(t * 7.3f) + 0.03f * sinf(t * 13.1f) + 0.03f * sinf(t * 3.1f) - gust * 0.18f * (0.5f + 0.5f * sinf(t * 23));
  float sway = 0.22f * sinf(t * 1.3f) + 0.1f * sinf(t * 3.7f) + gust * 1.0f * sinf(t * 5.3f);
  float fx = 7.5f, base = 9.6f, hgt = 6.4f * fl;
  // A dark room, warmed by the glow.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float d = sqrtf(sqf(x - fx - sway * 0.4f) + sqf((y - 7) * 1.1f)), k = clamp01(1 - d / 10);
      fb[y][x] = lerpColor(0x100500, 0x8A4010, k * k * fl);
    }
  // The table, lit beneath the candle.
  for (int x = 0; x < W; x++) fb[15][x] = lerpColor(0x1A0C04, 0x6A3A18, clamp01(1 - fabsf(x - 7.5f) / 8) * fl);
  // A brass dish.
  for (int x = 3; x <= 12; x++) fb[15][x] = lerpColor(0x7A5018, 0xC89038, clamp01(1 - fabsf(x - 6.5f) / 6) * fl);
  setPx(4, 14, 0xA07028), setPx(11, 14, 0x805820);
  // A wide pillar candle: translucent wax glowing near the top, shaded at the sides.
  for (int y = 10; y <= 14; y++)
    for (int x = 5; x <= 10; x++) {
      uint32_t c = lerpColor(0xF0D8B0, 0x907458, (y - 10) / 4.5f);
      if (x == 5) c = lerpColor(c, 0xFFF0D0, 0.25f);
      if (x >= 9) c = scale(c, x == 10 ? 150 : 205, x == 10 ? 140 : 200, x == 10 ? 130 : 195);
      fb[y][x] = c;
    }
  // The melted pool on top glows from the flame, and a drip runs down one side.
  for (int x = 6; x <= 9; x++) fb[10][x] = lerpColor(0xFFC060, 0xFFE8A0, 0.5f + 0.5f * fl - 0.4f);
  setPx(5, 10, 0xFFF4D8), setPx(10, 10, 0xE8D0A8);
  setPx(10, 11, 0xF0DCB4), setPx(10, 12, 0xE0C8A0), setPx(10, 13, 0xC8B090);
  // The flame: a teardrop with a hot core and a blue base, leaning and stretching.
  softGlow(fx + sway * 0.5f, base - hgt * 0.45f, 3.4f, 0x3A1800, fl);
  for (int y = 0; y <= 10; y++)
    for (int x = 3; x <= 12; x++) {
      float v = (base - y) / hgt;
      if (v < -0.15f || v > 1.05f) continue;
      float cx = fx + sway * v * v, w = 1.7f * powf(clamp01(1 - v), 0.75f) * clamp01(v * 2.2f + 0.45f);
      float dx = fabsf(x - cx), a = clamp01(w - dx + 0.55f) * clamp01((1.05f - v) * hgt * 0.6f);
      if (a <= 0) continue;
      float in = clamp01(1.25f - dx / max(0.35f, w)), heat = in * (1 - v * 0.6f);
      uint32_t c = heat < 0.35f ? lerpColor(0xA81800, 0xFF6A10, heat / 0.35f)
                   : heat < 0.65f ? lerpColor(0xFF6A10, 0xFFC040, (heat - 0.35f) / 0.3f)
                                  : lerpColor(0xFFC040, 0xFFF8E0, (heat - 0.65f) / 0.35f);
      blendPx(x, y, c, a);
    }
  // Smoke after the draught.
  float sm = gusty ? clamp01((gp - 8.4f) * 2) * clamp01(1 - (gp - 8.4f) / 4.5f) : 0;
  for (int s = 0; s < 7 && sm > 0; s++) {
    float yy = base - hgt - 0.5f - s * 0.9f, xx = fx + sinf(s * 0.9f - t * 1.8f) * s * 0.22f + s * 0.1f;
    float g = 110 * sm * (1 - s / 7.0f);
    addAA(xx, yy, rgb(g, g * 0.95f, g * 0.9f));
  }
  gammaAll();
}

// ---- Campfire: lively flames, glowing logs and drifting sparks ----------
struct Ember { float x, y, vx, vy, life; };
Ember campSparks[14];
float fireHeat[H][W];
void fxCampfire() {
  float t = tsec, dt = min(0.1f, sdt());
  static float acc = 0;
  if (fxFresh) {
    memset(fireHeat, 0, sizeof(fireHeat)), acc = 0;
    for (auto &s : campSparks) s.life = 0;
  }
  // Step the flames 30 times a second: heat rises from the logs, drifting
  // with the breeze and cooling as it goes.
  float wind = 0.6f * sinf(t * 0.35f) + 0.3f * sinf(t * 1.1f);
  acc += dt;
  while (acc > 1 / 30.0f) {
    acc -= 1 / 30.0f;
    for (int x = 3; x <= 12; x++) {
      float c = clamp01(1 - fabsf(x - 7.5f) / 4.8f);
      fireHeat[14][x] = c * rnd(0.6f, 1.2f);
      fireHeat[13][x] = max(fireHeat[13][x] * 0.6f, c * rnd(0.3f, 1.15f));
    }
    for (int y = 1; y <= 12; y++)
      for (int x = 1; x < W - 1; x++) {
        int sx = x;
        if (wind > 0.3f && random(3) == 0) sx--;
        if (wind < -0.3f && random(3) == 0) sx++;
        sx = constrain(sx, 1, W - 2);
        float h = fireHeat[y + 1][sx] * 0.52f + (fireHeat[y + 1][sx - 1] + fireHeat[y + 1][sx + 1]) * 0.19f + fireHeat[min(y + 2, 14)][sx] * 0.1f;
        fireHeat[y][x] = max(0.0f, h - rnd(0.02f, 0.13f));
      }
  }
  float glow = 0;
  for (int x = 4; x <= 11; x++) glow += fireHeat[11][x];
  glow = 0.6f + 0.4f * clamp01(glow / 3.5f);
  // Night sky, stars, and dark trees lit faintly by the fire.
  skyGradient(0x060A2A, 0x141838, 11);
  for (int i = 0; i < 9; i++) {
    int sx = fwHash(i * 7 + 400) * 16, sy = fwHash(i * 7 + 401) * 6;
    float tw = 0.6f + 0.4f * sinf(t * (0.7f + fwHash(i + 410)) + i);
    fbAdd(sx, sy, scale(0x9098C0, 170 * tw, 170 * tw, 170 * tw));
  }
  for (int x = 0; x < W; x++) {
    int top = 7 + (int)(fwHash(x / 2 + 420) * 2.5f) + (x > 4 && x < 11 ? 2 : 0);
    for (int y = top; y < 12; y++) fb[y][x] = lerpColor(0x040604, 0x2A1004, clamp01((y - top) / 4.0f) * glow * clamp01(1 - fabsf(x - 7.5f) / 9));
  }
  // The ground, warm in the firelight.
  for (int y = 12; y < H; y++)
    for (int x = 0; x < W; x++) {
      float d = sqrtf(sqf(x - 7.5f) + sqf((y - 13) * 1.6f)), k = clamp01(1 - d / 8.5f);
      fb[y][x] = lerpColor(0x140A04, 0x7A3810, k * k * glow);
    }
  // The flames.
  for (int y = 1; y <= 13; y++)
    for (int x = 0; x < W; x++) {
      float h = fireHeat[y][x];
      if (h < 0.06f) continue;
      uint32_t c = h < 0.3f ? lerpColor(0x500800, 0xC82800, h / 0.3f)
                   : h < 0.55f ? lerpColor(0xC82800, 0xFF7010, (h - 0.3f) / 0.25f)
                   : h < 0.8f  ? lerpColor(0xFF7010, 0xFFC040, (h - 0.55f) / 0.25f)
                               : lerpColor(0xFFC040, 0xFFF0C0, (h - 0.8f) / 0.4f);
      blendPx(x, y, c, clamp01(h * 3));
    }
  // Logs: two angled behind, one across the front, with glowing cracks.
  static const int8_t LOGS[][2] = {{3, 13}, {4, 13}, {5, 12}, {12, 13}, {11, 13}, {10, 12}};
  for (int i = 0; i < 6; i++) setPx(LOGS[i][0], LOGS[i][1], i % 3 == 0 ? 0x5A3418 : 0x3A200C);
  for (int x = 4; x <= 11; x++) {
    bool crack = fwHash(x + 430) > 0.6f;
    float e = 0.5f + 0.5f * sinf(t * 2.3f + x * 1.9f);
    setPx(x, 14, crack ? lerpColor(0x802000, 0xFF7010, e) : (x == 4 || x == 11 ? 0x6A4020 : 0x4A2C10));
  }
  setPx(4, 14, 0xD06020), setPx(11, 14, 0xB04818);  // glowing log ends
  for (int x = 5; x <= 10; x++) setPx(x, 15, lerpColor(0x601000, 0xFF5008, 0.5f + 0.5f * sinf(t * 3.1f + x * 2.7f)));
  // A ring of stones, lit on the side facing the fire.
  static const int8_t STONES[][2] = {{1, 14}, {2, 15}, {3, 15}, {12, 15}, {13, 15}, {14, 14}};
  for (auto &s : STONES) setPx(s[0], s[1], lerpColor(0x302C2C, 0xA06040, glow * (s[0] == 3 || s[0] == 12 ? 0.8f : 0.5f)));
  // Sparks float up, wobbling and fading; now and then a log pops.
  bool pop = random(1000) < 6;
  for (auto &s : campSparks) {
    if (s.life <= 0) {
      if (random(100) < 5 || pop) s = {rnd(5.5f, 9.5f), rnd(8, 10), rnd(-0.6f, 0.6f) + wind * 0.4f, rnd(-4.5f, -2.5f), rnd(1.2f, 2.6f)}, pop = false;
      continue;
    }
    s.life -= dt, s.vy *= 1 - 0.5f * dt;
    s.y += s.vy * dt, s.x += (s.vx + 0.6f * sinf(t * 4 + s.y * 1.3f)) * dt;
    float k = clamp01(s.life / 1.2f);
    addAA(s.x, s.y, lerpColor(0x601000, s.life > 1.4f ? 0xFFE080 : 0xFF8020, k));
  }
  gammaAll();
}

// ---- Rainy Window: drops sliding down misted glass, city lights blurred behind
struct PaneDrop { float x, y, v, hold; };
PaneDrop raindrops[8];
uint8_t dew[H][W];  // little beads stuck to the glass
float fog[H][W];    // mist on the glass: 1 = misted, 0 = wiped clear
void fxRainyWindow() {
  float t = tsec, dt = min(0.1f, sdt());
  // The city at night behind the glass, all out of focus: soft discs of light,
  // and every so often a car's lights gliding past.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = lerpColor(0x101640, 0x201C40, y / 15.0f);
  static const uint32_t BOKEH[] = {0xFF9020, 0xFFD060, 0x40A0FF, 0xFF4060, 0xFFF0D0, 0x60FFA0};
  auto bokeh = [&](float bx, float by, float r, uint32_t c, float k) {
    for (int y = floorf(by - r - 1); y <= ceilf(by + r + 1); y++)
      for (int x = floorf(bx - r - 1); x <= ceilf(bx + r + 1); x++) {
        float d = sqrtf(sqf(x - bx) + sqf(y - by)), e = clamp01((r + 0.5f - d) / 1.3f) * k;
        if (e > 0) fbAdd(x, y, scale(c, 255 * e, 255 * e, 255 * e));
      }
  };
  for (int i = 0; i < 10; i++)
    bokeh(fwHash(i * 7 + 1) * 16, 1 + fwHash(i * 7 + 2) * 11, 1.4f + fwHash(i * 7 + 3) * 1.4f, BOKEH[i % 6],
          0.45f + 0.12f * sinf(t * (0.4f + fwHash(i + 30)) + i));
  float cp = pmod(t, 11) / 4;
  if (cp < 1) {
    bool right = (int)(t / 11) % 2;
    float cx = right ? lerpf(-4, 20, cp) : lerpf(20, -4, cp);
    uint32_t c = right ? 0xFFF0C0 : 0xFF2010;
    bokeh(cx, 13, 1.8f, c, 0.7f), bokeh(cx + 3.5f, 13, 1.8f, c, 0.7f);
  }
  if (fxFresh) {
    memset(dew, 0, sizeof(dew));
    for (auto &row : fog)
      for (auto &v : row) v = 1;
    for (auto &d : raindrops) d = {rnd(0, 15), rnd(-12, 12), 0, rnd(0, 3)};
  }
  // The mist creeps back over the cleared trails, and new beads land on it.
  for (auto &row : fog)
    for (auto &v : row) v = min(1.0f, v + dt * 0.05f);
  if (random(100) < 10) {
    int x = random(W), y = random(H);
    if (fog[y][x] > 0.4f) dew[y][x] = random(140, 255);
  }
  // Drops cling, then slip and slide, wiping the mist and swallowing beads.
  for (auto &d : raindrops) {
    if (d.hold > 0) d.hold -= dt, d.v = 0;
    else {
      d.v = min(7.0f, d.v + 8 * dt);
      float oy = d.y;
      d.y += d.v * dt;
      if (random(100) < 2) d.hold = rnd(0.4f, 2);
      if (random(100) < 4) d.x = constrain(d.x + (random(2) ? 0.5f : -0.5f), 0.0f, 15.0f);
      int ix = roundf(d.x);
      for (int y = max(0, (int)oy - 1); y <= min(H - 1, (int)d.y); y++) {
        fog[y][ix] = 0;
        if (ix > 0) fog[y][ix - 1] = min(fog[y][ix - 1], 0.5f);
        if (dew[y][ix]) dew[y][ix] = 0, d.v += 1.5f;
      }
      if (random(100) < 6 && d.y > 2 && d.y < H + 1) dew[min(H - 1, (int)d.y - 2)][ix] = 120;  // a bead left behind
    }
    if (d.y > 18) d = {rnd(0, 15), rnd(-8, -1), 0, rnd(0, 1.5f)};
  }
  // Seen through mist the lights soften and grey; through cleared trails they're sharp.
  static uint32_t sharp[H][W];
  memcpy(sharp, fb, sizeof(sharp));
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int r = 0, g = 0, b = 0, n = 0;
      for (int yy = max(0, y - 1); yy <= min(H - 1, y + 1); yy++)
        for (int xx = max(0, x - 1); xx <= min(W - 1, x + 1); xx++) {
          uint32_t c = sharp[yy][xx];
          r += c >> 16 & 255, g += c >> 8 & 255, b += c & 255, n++;
        }
      uint32_t misty = lerpColor(rgb(r / n, g / n, b / n), 0x4A5874, 0.25f);
      fb[y][x] = lerpColor(sharp[y][x], misty, fog[y][x] * 0.75f);
      if (dew[y][x]) fb[y][x] = lerpColor(fb[y][x], 0xB8CCE4, dew[y][x] / 255.0f * 0.5f);
    }
  for (auto &d : raindrops) {
    int ix = roundf(d.x), iy = d.y;
    blendPx(ix, iy, 0xE0F0FF, 0.85f);
    blendPx(ix, iy + 1, 0x000000, 0.3f);  // the darker underside of the drop
    blendPx(ix, iy - 1, 0xA0B8D8, 0.3f);
  }
  gammaAll();
}

// ---- Moonrise: a big moon rising over still water, its path shimmering ---
void fxMoonrise() {
  float t = tsec;
  // Slowly up, a long hold, then back down, over about a minute and a half.
  float cyc = pmod(t / 90, 1), up = smooth01(cyc * 3) * (1 - smooth01((cyc - 0.75f) * 4));
  float my = lerpf(11.5f, 3.5f, up), mx = 9.5f;
  float glow = up;
  skyGradient(lerpColor(0x000208, 0x020820, glow), lerpColor(0x100818, 0x182848, glow), 10);
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < W; x++)
      if (fwHash(x * 53 + y * 17) > 0.9f) fbAdd(x, y, scale(0x606070, 150 + 100 * sinf(t * 1.3f + x * 3 + y), 150, 150));
  uint32_t moonC = lerpColor(0xFF9040, 0xF0F0D8, up);
  // The moon, with soft darker patches, and a halo.
  for (int y = 0; y < 11; y++)
    for (int x = 4; x < W; x++) {
      float d = sqrtf((x - mx) * (x - mx) + (y - my) * (y - my));
      if (d < 2.6f) {
        uint32_t c = fwHash(x * 5 + y * 3 + 900) > 0.75f ? scale(moonC, 200, 200, 200) : moonC;
        fb[y][x] = lerpColor(fb[y][x], c, clamp01(2.6f - d));
      } else if (d < 5.5f) fbAdd(x, y, scale(moonC, 30 * (5.5f - d), 30 * (5.5f - d), 30 * (5.5f - d)));
    }
  // Hills on the far shore hide the moon until it rises.
  for (int x = 0; x < W; x++) {
    int top = 9 + (x < 6 ? (x < 3 ? -1 : 0) : 1) - (x > 12);
    for (int y = top; y <= 10; y++) fb[y][x] = 0x020406;
  }
  // Water, and the moon's shimmering path across it.
  for (int y = 11; y < H; y++)
    for (int x = 0; x < W; x++) {
      fb[y][x] = lerpColor(0x02060E, 0x061020, (y - 11) / 4.0f);
      float wob = 0.6f * sinf(t * 2 + y * 2.3f) + 0.4f * sinf(t * 3.3f + y * 1.1f + x);
      float w = 0.8f + (y - 11) * 0.45f;
      float k = clamp01(w - fabsf(x - mx - wob)) * (0.4f + 0.6f * (sinf(t * 4 + x * 2 + y * 3) > 0)) * up;
      fb[y][x] = lerpColor(fb[y][x], moonC, k * 0.8f);
    }
  gammaAll();
}

// ---- Jellyfish: glowing jellies pulsing slowly up through deep water -----
void fxJellyfish() {
  float t = tsec;
  // Deep water, faint light shafts from far above, and drifting specks.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float ray = powf(clamp01(0.5f + 0.6f * sinf(x * 0.6f + y * 0.3f + t * 0.25f) * sinf(x * 0.27f - t * 0.17f)), 3) * clamp01(1 - y / 14.0f);
      fb[y][x] = lerpColor(lerpColor(0x0C2A5A, 0x04102A, y / 15.0f), 0x285A90, ray * 0.6f);
    }
  for (int i = 0; i < 12; i++) {
    float sx = pmod(fwHash(i) * 16 + t * 0.15f + sinf(t * 0.3f + i), 16), sy = pmod(fwHash(i + 50) * 16 + t * 0.25f, 16);
    addAA(sx, sy, 0x405868);
  }
  static const uint32_t JC[3] = {0xFF60C8, 0x50D8FF, 0xB878FF};
  static const float SIZE[3] = {1.0f, 0.72f, 0.88f}, XS[3] = {4.5f, 12, 9.5f};
  for (int j = 2; j >= 0; j--) {  // the furthest (smallest) first
    float sc = SIZE[j], P = 2.6f + j * 0.5f;
    float u = t / P + j * 0.37f, ph = u - floorf(u);
    // Each beat: a quick squeeze that pushes it up, then a slow relax.
    float squeeze = ph < 0.3f ? smooth01(ph / 0.3f) : 1 - smooth01((ph - 0.3f) / 0.7f);
    float prog = floorf(u) + smooth01(ph / 0.55f);
    float y = 23 - pmod(prog * 1.4f * sc + j * 11, 34);
    float x = XS[j] + 1.5f * sinf(t * 0.13f + j * 2.4f);
    float a = (2.7f - 0.75f * squeeze) * sc, b = (2.2f + 0.7f * squeeze) * sc;
    uint32_t c = JC[j];
    // Tentacles stream out behind, bunching on the squeeze; the middle arms are frillier.
    for (int k = 0; k < 5; k++) {
      bool oral = k == 2;
      float tx0 = x + (k - 2) * a * 0.38f;
      int len = oral ? 5 : 8;
      for (int s = 1; s <= len; s++) {
        float yy = y + 0.2f + s * (0.7f + 0.3f * (1 - squeeze)) * sc;
        float xx = tx0 + sinf(t * 1.6f - s * 0.7f + k * 1.3f) * 0.22f * s * sc + (k - 2) * s * 0.04f;
        float g = (oral ? 255 : 200) * (1 - s / (len + 1.0f)) * sc;
        blendPx(roundf(xx), roundf(yy), c, g / 255);
        if (oral) blendPx(roundf(xx + 0.7f * sinf(t * 3 + s)), roundf(yy), c, g / 510);
      }
    }
    // The bell: see-through, with a glowing rim, a bright frilled edge and a lit crown.
    softGlow(x, y - b * 0.5f, a + 2.2f, c, 0.25f * sc);
    for (int yy = floorf(y - b - 1); yy <= ceilf(y + 1); yy++)
      for (int xx = floorf(x - a - 1); xx <= ceilf(x + a + 1); xx++) {
        float dx = (xx - x) / a, dy = (yy - y) / b;
        if (dy > 0.3f) continue;
        float r = sqrtf(dx * dx + dy * dy), edge = (1 - r) * min(a, b);
        float cov = clamp01(edge + 0.5f) * clamp01((0.3f - dy) * b * 2);
        if (cov <= 0) continue;
        float rim = clamp01(1 - fabsf(edge - 0.25f) * 1.3f);
        bool margin = dy > -0.05f;
        float g = 0.18f + 0.75f * rim + 0.3f * clamp01(-dy - 0.6f);
        if (margin) g = ((xx + (int)(t * 4)) % 2) ? 1 : 0.7f;  // the frilled edge shimmers
        uint32_t col = lerpColor(c, 0xFFFFFF, rim * 0.3f + (margin ? 0.3f : 0));
        blendPx(xx, yy, col, cov * min(1.0f, g) * (0.55f + 0.45f * sc));
      }
    blendPx(roundf(x - 0.5f), roundf(y - b * 0.45f), lerpColor(c, 0xFFFFFF, 0.5f), 0.6f);  // the glowing heart
  }
  gammaAll();
}

// ============================================================================
// Slow patterns
// ============================================================================

// ---- Ink in Water: drops of colour plume down, curl and drift apart -------
struct InkBit { float x, y, vx, vy, age; uint8_t c; };
InkBit inkBits[72];
float inkD[H][W][3];  // how much of each colour is in the water at each pixel
void fxInk() {
  float t = tsec, dt = min(0.08f, sdt());
  static const uint32_t INK[] = {0x3050FF, 0xFF2878, 0x10D0A8, 0xFF7010, 0xA040FF, 0xFFD020};
  static int nextBit, drops;
  static float nextDrop;
  if (fxFresh) {
    memset(inkD, 0, sizeof(inkD));
    for (auto &b : inkBits) b.age = 99;
    nextBit = 0, drops = (int)(esp_random() % 6), nextDrop = 0;
  }
  // Every few seconds a new drop falls in somewhere along the top.
  nextDrop -= dt;
  if (nextDrop <= 0) {
    nextDrop = rnd(6, 9);
    float cx = rnd(4, 12);
    uint8_t col = drops++ % 6;
    for (int i = 0; i < 24; i++) {
      float a = rnd(0, 6.28f), s = rnd(0, 1);
      inkBits[nextBit] = {cx + cosf(a) * 0.5f * s, rnd(-2.5f, -0.5f), cosf(a) * s * 1.2f, rnd(3.5f, 6.5f), 0, col};
      nextBit = (nextBit + 1) % 72;
    }
  }
  // The ink is carried by slow swirling currents and sinks gently.
  auto psi = [&](float x, float y) { return sinf(x * 0.55f + t * 0.21f) * cosf(y * 0.45f - t * 0.17f) + 0.6f * sinf((x - y) * 0.38f + t * 0.3f); };
  for (auto &b : inkBits) {
    if (b.age > 18) continue;
    const float e = 0.3f;
    float fx = (psi(b.x, b.y + e) - psi(b.x, b.y - e)) / (2 * e), fy = -(psi(b.x + e, b.y) - psi(b.x - e, b.y)) / (2 * e);
    float k = min(1.0f, 1.3f * dt);
    b.vx += (fx * 1.6f - b.vx) * k, b.vy += (fy * 1.6f + 0.35f - b.vy) * k;
    b.x += b.vx * dt, b.y += b.vy * dt, b.age += dt;
    if (b.y > 17 || b.x < -2 || b.x > 17) b.age = 99;
    // Each bit leaves colour where it passes.
    int x0 = floorf(b.x), y0 = floorf(b.y);
    float fx2 = b.x - x0, fy2 = b.y - y0, amt = 0.3f * dt * clamp01((18 - b.age) / 4);
    uint32_t c = INK[b.c];
    for (int i = 0; i < 4; i++) {
      int xx = x0 + (i & 1), yy = y0 + (i >> 1);
      if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
      float w = ((i & 1) ? fx2 : 1 - fx2) * ((i >> 1) ? fy2 : 1 - fy2) * amt;
      inkD[yy][xx][0] += w * (c >> 16 & 255) / 255.0f, inkD[yy][xx][1] += w * (c >> 8 & 255) / 255.0f, inkD[yy][xx][2] += w * (c & 255) / 255.0f;
    }
  }
  // It spreads softly into the water around it, and slowly thins away.
  static float tmp[H][W][3];
  float keep = 1 - 0.14f * dt, mix = min(0.5f, 0.7f * dt);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      for (int ch = 0; ch < 3; ch++) {
        float avg = (inkD[y][max(0, x - 1)][ch] + inkD[y][min(W - 1, x + 1)][ch] + inkD[max(0, y - 1)][x][ch] + inkD[min(H - 1, y + 1)][x][ch]) / 4;
        tmp[y][x][ch] = (inkD[y][x][ch] * (1 - mix) + avg * mix) * keep;
      }
  memcpy(inkD, tmp, sizeof(inkD));
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint32_t water = lerpColor(0x0C2034, 0x061020, y / 15.0f);
      float r = 1 - expf(-inkD[y][x][0] * 1.4f), g = 1 - expf(-inkD[y][x][1] * 1.4f), b = 1 - expf(-inkD[y][x][2] * 1.4f);
      fb[y][x] = rgb(max((float)(water >> 16 & 255), r * 255), max((float)(water >> 8 & 255), g * 255), max((float)(water & 255), b * 255));
    }
  gammaAll();
}

// ---- Tide Pool: sunlight rippling over a rock pool full of little life ---
void fxTidePool() {
  float t = tsec * 0.8f;
  // Sandy shallows deepening to blue-green, seen through the water.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float depth = clamp01((x * 0.5f + y * 0.8f) / 17 + (vnoise(x * 0.35f, y * 0.35f) - 0.5f) * 0.6f);
      uint32_t sand = lerpColor(0xE8C890, 0xC8A870, fwHash(x * 13 + y * 7 + 500));
      fb[y][x] = lerpColor(sand, 0x1A7888, 0.12f + depth * 0.7f);
    }
  // Rocks, shaded and patched with green weed, casting a little shadow.
  static const float ROCK[2][3] = {{2.2f, 12.8f, 2.8f}, {13.8f, 2, 2}};
  for (auto &r : ROCK)
    for (int y = floorf(r[1] - r[2] - 1); y <= ceilf(r[1] + r[2] + 2); y++)
      for (int x = floorf(r[0] - r[2] - 1); x <= ceilf(r[0] + r[2] + 2); x++) {
        float dx = x - r[0], dy = y - r[1], rr = r[2] * (0.85f + 0.3f * vnoise(x * 0.9f + r[0], y * 0.9f));
        float d = sqrtf(dx * dx + dy * dy);
        blendPx(x, y, 0x0A3038, clamp01(rr - sqrtf(sqf(dx - 1) + sqf(dy - 1)) + 0.5f) * 0.5f);
        float a = clamp01(rr - d + 0.5f);
        if (a <= 0) continue;
        uint32_t c = lerpColor(0x8A8070, 0x3A3630, clamp01(0.5f + (dx + dy) / (2 * r[2])));
        if (vnoise(x * 1.3f, y * 1.3f + 40) > 0.62f) c = lerpColor(c, 0x3A8A30, 0.7f);
        blendPx(x, y, c, a);
      }
  // Seaweed swaying from the big rock.
  for (int k = 0; k < 2; k++)
    for (int s = 0; s < 6; s++) {
      float xx = 3.5f + k * 1.5f + s * 0.25f + sinf(t * 1.2f - s * 0.6f + k) * 0.3f * s, yy = 10.5f - k * 0.5f - s;
      blendPx(roundf(xx), roundf(yy), lerpColor(0x2A8030, 0x80D050, s / 6.0f), 0.85f);
    }
  // A pink anemone, its tentacles waving.
  for (int i = 0; i < 7; i++) {
    float a = i * 0.9f + 0.3f * sinf(t * 1.5f + i), l = 1.5f + 0.4f * sinf(t * 2 + i * 1.7f);
    addAA(12.5f + cosf(a) * l, 12.5f + sinf(a) * l, 0x803050);
  }
  disc(12.5f, 12.5f, 0.9f, 0xFF6A98);
  setPx(12, 12, 0xFFA0C0);
  // A five-armed starfish, turning very slowly, with a speckled back.
  float rot = t * 0.03f;
  for (int y = 2; y <= 12; y++)
    for (int x = 4; x <= 13; x++) {
      float dx = x - 8.5f, dy = y - 7, d = sqrtf(dx * dx + dy * dy), th = atan2f(dy, dx) - rot;
      float arm = 0.9f + 2.5f * powf(fabsf(cosf(2.5f * th)), 2.5f);
      float a = clamp01(arm - d + 0.5f);
      if (a <= 0) continue;
      uint32_t c = d < 1 ? 0xFF8040 : fwHash(x * 31 + y * 17 + 520) > 0.7f ? 0xFFB070 : 0xF05A20;
      blendPx(x, y, c, a);
    }
  // Sunlight rippling through the surface onto everything below.
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float n1 = vnoise(x * 0.22f + t * 0.25f, y * 0.22f + t * 0.08f), n2 = vnoise(x * 0.22f - t * 0.18f + 7, y * 0.22f + t * 0.2f + 3);
      float c = powf(clamp01(1 - fabsf(n1 - n2) * 5), 2);
      fb[y][x] = scale(fb[y][x], 215 + 40 * c, 215 + 40 * c, 215 + 40 * c);
      fbAdd(x, y, scale(0xA0FFF0, 150 * c, 150 * c, 150 * c));
    }
  // Now and then a little fish darts across in short bursts, its shadow below.
  float cyc = pmod(t, 11);
  if (cyc < 4.5f) {
    bool right = (int)(t / 11) % 2;
    float q = cyc * 2, prog = (floorf(q) + smooth01(clamp01((q - floorf(q)) * 2.5f))) / 9;
    float fx = right ? lerpf(-2, 18, prog) : lerpf(18, -2, prog), fy = 4 + fwHash((int)(t / 11) + 530) * 6 + sinf(prog * 9);
    int dir = right ? -1 : 1;
    blendPx(roundf(fx + 1), roundf(fy + 2), 0x0A3038, 0.5f), blendPx(roundf(fx + 1 + dir), roundf(fy + 2), 0x0A3038, 0.4f);
    blendPx(roundf(fx), roundf(fy), 0xD8E8E8, 0.95f), blendPx(roundf(fx + dir), roundf(fy), 0x90A8B0, 0.9f);
    blendPx(roundf(fx + 2 * dir), roundf(fy), 0x607880, 0.6f);
  }
  gammaAll();
}

// ---- Kaleidoscope: soft eight-way symmetric shapes turning slowly --------
void fxKaleido() {
  float t = tsec * 0.35f;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float dx = x - 7.5f, dy = y - 7.5f, r = sqrtf(dx * dx + dy * dy);
      float a = atan2f(dy, dx) + t * 0.3f;
      a = pmod(a, PI / 4);
      if (a > PI / 8) a = PI / 4 - a;  // mirror inside each wedge
      float u = r * cosf(a), v = r * sinf(a);
      float s = sinf(u * 0.7f - t * 1.3f) + sinf(v * 1.3f + t) + sinf((u + v) * 0.5f + t * 0.7f);
      float hue = t * 2500 + s * 5500 + r * 900;
      uint8_t val = 90 + 140 * clamp01(0.5f + s * 0.3f);
      fb[y][x] = hsv(hueOf(hue), 165, val);
    }
  gammaAll();
}

// ---- Galaxy: a spiral of stars and dust slowly turning -------------------
void fxGalaxy() {
  float t = tsec * 0.12f;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float dx = (x - 7.5f) * 1.0f, dy = (y - 7.5f) * 1.35f;  // tilted disc
      float r = sqrtf(dx * dx + dy * dy) + 0.01f, a = atan2f(dy, dx);
      float arm = powf(0.5f + 0.5f * cosf(2 * (a - logf(r + 1) * 2.4f + t)), 3);
      float fall = expf(-r / 4.5f);
      float core = expf(-r * r / 3);
      uint32_t c = lerpColor(0x000006, lerpColor(0x2030A0, 0xB060E0, clamp01(r / 8)), clamp01(arm * fall * 1.5f));
      if (arm > 0.6f && fwHash(x * 7 + y * 31) > 0.8f) c = lerpColor(c, 0xFF80B0, 0.5f * fall * 2);
      c = lerpColor(c, 0xFFF0D0, core);
      fb[y][x] = c;
      if (fwHash(x * 91 + y * 13 + 7) > 0.96f) {  // background stars twinkle
        uint8_t g = 60 + 50 * sinf(tsec * 1.5f + x * 3 + y);
        fbAdd(x, y, rgb(g, g, g));
      }
    }
  gammaAll();
}

// ============================================================================
// Slow, watchable things
// ============================================================================

// ---- Sand Garden: a rake draws patterns round the stones, a board smooths
// them away, and the next pattern begins ----------------------------------
void fxSandGarden() {
  float t = tsec;
  const float CYC = 32, PER = 3;  // seconds per pattern, pixels between grooves
  int n = (int)(t / CYC);
  float p = pmod(t, CYC) / CYC;
  int kind = n % 3;                                       // lines, waves, or a spiral
  float draw = clamp01(p / 0.62f);                        // how far the rake has got
  float wipe = p > 0.86f ? clamp01((p - 0.86f) / 0.12f) * 17 - 1 : -2;  // the smoothing board's row
  static const float ST[2][3] = {{5, 6, 1.9f}, {12.5f, 12, 1.2f}};  // stones: x, y, radius
  static const float ZONE[2] = {4.5f, 3};                            // ring width round each stone
  // Which stone's rings a point belongs to (-1 for none).
  auto zoneOf = [&](float x, float y, float &d) {
    int best = -1;
    float bm = 0;
    for (int i = 0; i < 2; i++) {
      float dd = sqrtf(sqf(x - ST[i][0]) + sqf(y - ST[i][1])), m = dd - ST[i][2] - ZONE[i];
      if (m < bm) bm = m, best = i, d = dd;
    }
    return best;
  };
  // Height of the raked sand (1 ridge, -1 groove).
  auto height = [&](float x, float y) {
    float dx = x - ST[0][0], dy = y - ST[0][1];
    if (kind == 2) return sinf(sqrtf(dx * dx + dy * dy) * 2 * PI / PER - atan2f(dy, dx));
    float d;
    int z = zoneOf(x, y, d);
    if (z >= 0) return sinf((d - ST[z][2]) * 2 * PI / PER - PI / 2);
    return sinf((kind == 1 ? y + 1.1f * sinf(x * 0.5f) : y) * 2 * PI / PER);
  };
  // Rings are raked first, sweeping round each stone; then the lines, pass by
  // pass, back and forth. The spiral is raked from the stone outwards.
  float ringDraw = clamp01(draw / 0.3f), lineDraw = clamp01((draw - 0.3f) / 0.7f) * 6;
  auto raked = [&](int x, int y) {
    if (y <= wipe) return false;
    float dx = x - ST[0][0], dy = y - ST[0][1];
    if (kind == 2) return (sqrtf(dx * dx + dy * dy) + (atan2f(dy, dx) + PI) / (2 * PI) * PER) / 14 < draw;
    float d;
    int z = zoneOf(x, y, d);
    if (z >= 0) return (atan2f(y - ST[z][1], x - ST[z][0]) + PI) / (2 * PI) < ringDraw * 2 - z;
    int band = y / 3, cur = lineDraw;
    float f = lineDraw - cur;
    return band < cur || (band == cur && (cur % 2 ? x > 16 - f * 17 : x < f * 17));
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      bool r = raked(x, y);
      float h = r ? height(x, y) : 0, h2 = r ? height(x - 0.6f, y - 0.6f) : 0;
      float lit = (h - h2) * 0.55f;  // slopes facing the light (top left) are brighter
      uint32_t c = lerpColor(0xC4A874, 0xB49866, fwHash(x * 7 + y * 13 + 800));
      c = lit > 0 ? lerpColor(c, 0xF0DCB0, lit) : lerpColor(c, 0x6A5236, -lit);
      if (r && h < 0) c = lerpColor(c, 0x8A7048, -h * 0.3f);
      fb[y][x] = c;
    }
  // A leaf drifts down and settles on the sand until the board sweeps it away.
  float lp = clamp01((p - 0.3f) / 0.12f);
  float lx = 2 + fwHash(n + 810) * 12, ly = 2 + fwHash(n + 811) * 11;
  if (p > 0.3f && ly > wipe) {
    float xx = lx + (1 - lp) * 3 * sinf(lp * 9), yy = lerpf(-1, ly, lp);
    blendPx(roundf(xx), roundf(yy), 0xC03810, 0.9f), blendPx(roundf(xx) + 1, roundf(yy), 0xE07020, 0.7f);
  }
  // The stones, lit from the top left, casting shadows; moss on the big one.
  for (int i = 0; i < 2; i++)
    for (int y = floorf(ST[i][1] - 3); y <= ceilf(ST[i][1] + 3); y++)
      for (int x = floorf(ST[i][0] - 3); x <= ceilf(ST[i][0] + 3); x++) {
        float R = ST[i][2], dx = x - ST[i][0], dy = y - ST[i][1], d = sqrtf(dx * dx + dy * dy);
        blendPx(x, y, 0x4A3A22, clamp01(R - sqrtf(sqf(dx - 0.9f) + sqf(dy - 1)) + 0.5f) * 0.55f);
        float a = clamp01(R - d + 0.5f);
        if (a <= 0) continue;
        uint32_t c = lerpColor(0xA0A0A8, 0x34343C, clamp01(0.5f + (dx + dy) / (2.4f * R)));
        c = lerpColor(c, 0x2A2A30, vnoise(x * 1.5f + i * 9, y * 1.5f) * 0.35f);
        if (i == 0 && dy < -0.6f && fwHash(x * 5 + y + 830) > 0.45f) c = lerpColor(c, 0x4A7A30, 0.7f);
        blendPx(x, y, c, a);
      }
  // The rake at work: a wooden head with tines, and its handle.
  if (draw < 1 && p < 0.62f) {
    float rx, ry, ax, ay, bx = -0.45f, by = -0.9f;  // head position, head direction, handle direction
    if (kind == 2) {
      float f = pmod(draw * 14 / PER, 1), a = f * 2 * PI - PI, rr = draw * 14 - f * PER;
      rx = ST[0][0] + rr * cosf(a), ry = ST[0][1] + rr * sinf(a), ax = cosf(a), ay = sinf(a);
    } else if (ringDraw < 1) {
      int z = ringDraw < 0.5f ? 0 : 1;
      float a = (ringDraw * 2 - z) * 2 * PI - PI, rr = ST[z][2] + ZONE[z] * 0.5f;
      rx = ST[z][0] + rr * cosf(a), ry = ST[z][1] + rr * sinf(a), ax = cosf(a), ay = sinf(a);
    } else {
      int cur = min(5, (int)lineDraw);
      float f = lineDraw - (int)lineDraw, mx = cur % 2 ? -1 : 1;
      rx = cur % 2 ? 16 - f * 17 : f * 17, ry = cur * 3 + 1, ax = 0, ay = 1, bx = -mx * 0.6f, by = -0.8f;
    }
    for (int k = -1; k <= 1; k++) blendPx(roundf(rx + ax * k), roundf(ry + ay * k), 0x5A3818, 0.9f);
    for (int s = 1; s < 7; s++) blendPx(roundf(rx + bx * s), roundf(ry + by * s), 0x8A6030, 0.8f);
  }
  // The smoothing board, drawn across the sand.
  if (wipe > -1 && wipe < 16) {
    for (int x = 0; x < W; x++) blendPx(x, roundf(wipe), 0x6A4420, 0.85f);
    for (int s = 1; s < 5; s++) blendPx(7 + s / 2, roundf(wipe) - s, 0x8A6030, 0.8f);
  }
  gammaAll();
}

// ---- Hourglass: sand pours through a glass in a wooden frame; when it has
// all run through, the hourglass turns over smoothly and starts again --------
void fxHourglass() {
  float t = tsec;
  const float RUN = 20, HOLD = 1.2f, TURN = 1.8f;
  float p = pmod(t, RUN + HOLD + TURN), fill = clamp01(p / RUN);  // how much sand has fallen
  // Inner half-width of the glass at a row: two round bulbs and a narrow neck.
  auto halfW = [](float y) {
    float d = fabsf(y - 7.5f);
    return d < 0.6f ? 0.5f : 0.5f + 4.0f * sinf(clamp01((d - 0.6f) / 6.2f) * PI * 0.62f);
  };
  auto bg = [](int x, int y) {  // a warm, dim glow behind
    float k = clamp01(1 - sqrtf(sqf(x - 7.5f) + sqf(y - 7.5f)) / 11);
    return lerpColor(0x0A0604, 0x3A2412, k * k);
  };
  // How many pixels of sand each row holds, worked out once.
  static float area[H];
  static bool ready = false;
  if (!ready) {
    for (int y = 0; y < H; y++) {
      area[y] = 0;
      for (int x = 0; x < W; x++) area[y] += clamp01(halfW(y) - fabsf(x - 7.5f) + 0.5f);
    }
    ready = true;
  }
  float total = area[3] * 0.4f + area[4] + area[5] + area[6] + area[7];
  // Fill the top bulb from the neck upwards, and the bottom from the base up.
  float topAmt[H] = {0}, botAmt[H] = {0}, v = total * (1 - fill);
  for (int y = 7; y >= 1 && v > 0; y--) topAmt[y] = min(1.0f, v / area[y]), v -= area[y];
  v = total * fill;
  for (int y = 14; y >= 8 && v > 0; y--) botAmt[y] = min(1.0f, v / area[y]), v -= area[y];
  int surface = 15;  // the top row of the pile below
  for (int y = 8; y <= 14; y++)
    if (botAmt[y] > 0) { surface = y; break; }

  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) fb[y][x] = bg(x, y);
  for (int y = 1; y < 15; y++) {
    float w = halfW(y);
    for (int x = 0; x < W; x++) {
      float ax = fabsf(x - 7.5f), inside = clamp01(w - ax + 0.5f);
      // The glass: a faint tint inside, and the walls catching the light.
      blendPx(x, y, 0x34505E, inside * 0.5f);
      float wall = clamp01(1.2f - fabsf(ax - w - 0.6f) * 1.3f);
      blendPx(x, y, 0xA8D4EC, wall * 0.9f);
      if (inside <= 0) continue;
      // Sand: the top drains from the middle into a funnel; the bottom heaps up in the middle.
      float a = 0;
      if (y <= 7 && topAmt[y] > 0) a = topAmt[y] >= 1 ? 1 : clamp01(ax - (1 - topAmt[y]) * w + 0.5f);
      if (y >= 8 && botAmt[y] > 0) a = botAmt[y] >= 1 ? 1 : clamp01(botAmt[y] * (w + 0.5f) - ax + 0.5f);
      if (a <= 0) continue;
      uint32_t c = lerpColor(0xE8B868, 0xC08848, fwHash(x * 17 + y * 31 + 870));
      if (x < 7.5f - w * 0.4f) c = lerpColor(c, 0xF8D890, 0.3f);  // the lit side
      if ((y <= 7 && (y == 1 || topAmt[y - 1] <= 0)) || (y >= 8 && y == surface)) c = lerpColor(c, 0xFFE0A0, 0.35f);  // the top layer
      blendPx(x, y, c, a * inside);
    }
  }
  // The thin falling stream, grains glinting as they drop.
  if (fill > 0 && fill < 1) {
    blendPx(7, 7, 0xE8B868, 0.8f), blendPx(8, 7, 0xE8B868, 0.5f);
    for (int y = 8; y < surface; y++) {
      float g = fwHash(y * 7 + (int)(t * 24) * 13);
      blendPx(7 + (g > 0.7f), y, lerpColor(0xD8A050, 0xFFF0C0, g), 0.55f + 0.45f * g);
    }
    if (fwHash((int)(t * 10) + 880) > 0.6f) blendPx(7 + (fwHash((int)(t * 10) + 881) > 0.5f ? 2 : -1), surface - 1, 0xE8B868, 0.5f);
  }
  // Light glinting on the glass.
  for (int y : {2, 3, 4, 5, 10, 11, 12, 13}) blendPx(roundf(7.5f - halfW(y) + 0.6f), y, 0xE0F4FF, 0.55f);
  // The wooden frame: turned posts and two end caps.
  for (int y = 1; y < 15; y++) {
    bool bead = y == 3 || y == 7 || y == 8 || y == 12;
    setPx(1, y, bead ? 0x9A6030 : 0x5A3418), setPx(14, y, bead ? 0x7A4824 : 0x4A2A12);
  }
  for (int x = 0; x < W; x++) {
    uint32_t c = x == 0 || x == 15 ? 0x4A2810 : lerpColor(0x9A5C28, 0x6A3C18, x / 15.0f);
    setPx(x, 0, c), setPx(x, 15, c);
  }
  // The turn: the whole hourglass rotates half a turn, smoothly sampled.
  if (p > RUN + HOLD) {
    float ang = PI * smooth01((p - RUN - HOLD) / TURN), ca = cosf(ang), sa = sinf(ang);
    static uint32_t tmp[H][W];
    memcpy(tmp, fb, sizeof(tmp));
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float dx = x - 7.5f, dy = y - 7.5f, sx = 7.5f + dx * ca + dy * sa, sy = 7.5f - dx * sa + dy * ca;
        int x0 = floorf(sx), y0 = floorf(sy);
        float fx = sx - x0, fy = sy - y0;
        uint32_t b = bg(x, y);
        auto at = [&](int xx, int yy) { return xx < 0 || yy < 0 || xx >= W || yy >= H ? b : tmp[yy][xx]; };
        fb[y][x] = lerpColor(lerpColor(at(x0, y0), at(x0 + 1, y0), fx), lerpColor(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx), fy);
      }
  }
  gammaAll();
}

// ---- Breathing Guide: grow while you breathe in (4 s), shrink while you
// breathe out (6 s). Runs on real time, whatever the speed setting. --------
void fxBreatheGuide() {
  float s = pmod(millis() / 1000.0f, 10);
  float k = s < 4 ? smooth01(s / 4) : 1 - smooth01((s - 4) / 6);
  float r = 1.8f + 5.2f * k;
  uint32_t c = lerpColor(0x30A0C0, 0xA080F0, k);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float d = sqrtf((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
      float inside = clamp01(r - d + 0.5f);
      float halo = clamp01(1 - (d - r) / 2.5f) * 0.25f;
      uint32_t bg = lerpColor(0x000204, scale(c, 40, 40, 40), halo * 4);
      fb[y][x] = lerpColor(bg, lerpColor(c, 0xFFFFFF, clamp01(1 - d / r) * 0.35f), inside);
    }
  gammaAll();
}
