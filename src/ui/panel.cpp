// Bass Suite — panel.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/panel.hpp"

#include "presets.hpp"
#include "version.hpp"
#include "dsp/nam_files.hpp"
#include "dsp/user_ir.hpp"
#include "value_text.hpp"

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>

namespace bass {

namespace {

struct Rgb {
  double r, g, b;
};

struct Skin {
  const char* caption;
  Rgb bodyTop, bodyBottom;
  Rgb legend, legendDim;
  Rgb headerBg;
  Rgb tabOff, tabOn, tabTextOff, tabTextOn;
  Rgb knobCap, knobCapEdge, knobSkirt, knobIndicator;
  Rgb btnOff, btnOn, btnText, btnTextOn;
  Rgb accent;
  bool knurled;
};

Skin makeSkin(const char* caption, Rgb bodyTop, Rgb bodyBottom, Rgb legend,
              Rgb legendDim, Rgb headerBg, Rgb tabOff, Rgb tabOn,
              Rgb tabTextOff, Rgb tabTextOn, Rgb knobCap, Rgb knobCapEdge,
              Rgb knobSkirt, Rgb knobIndicator, Rgb btnOff, Rgb btnOn,
              Rgb btnText, Rgb btnTextOn, Rgb accent, bool knurled) {
  Skin s;
  s.caption = caption;
  s.bodyTop = bodyTop;
  s.bodyBottom = bodyBottom;
  s.legend = legend;
  s.legendDim = legendDim;
  s.headerBg = headerBg;
  s.tabOff = tabOff;
  s.tabOn = tabOn;
  s.tabTextOff = tabTextOff;
  s.tabTextOn = tabTextOn;
  s.knobCap = knobCap;
  s.knobCapEdge = knobCapEdge;
  s.knobSkirt = knobSkirt;
  s.knobIndicator = knobIndicator;
  s.btnOff = btnOff;
  s.btnOn = btnOn;
  s.btnText = btnText;
  s.btnTextOn = btnTextOn;
  s.accent = accent;
  s.knurled = knurled;
  return s;
}

Skin skinFor(int model) {
  switch (model) {
    case 0:  // Chorus Clean — silver / blue
      return makeSkin("Studio DI — clean solid-state",
                      {0.80, 0.82, 0.84}, {0.58, 0.61, 0.64},
                      {0.10, 0.10, 0.12}, {0.28, 0.30, 0.34},
                      {0.16, 0.18, 0.22},
                      {0.42, 0.46, 0.52}, {0.90, 0.92, 0.94},
                      {0.12, 0.13, 0.15}, {0.02, 0.02, 0.02},
                      {0.86, 0.88, 0.90}, {0.48, 0.52, 0.56},
                      {0.16, 0.18, 0.22}, {0.20, 0.48, 0.78},
                      {0.10, 0.12, 0.16}, {0.24, 0.52, 0.82},
                      {0.90, 0.92, 0.94}, {0.98, 0.99, 1.0},
                      {0.20, 0.48, 0.78}, false);
    case 1:  // Chime — black / cream / copper
      return makeSkin("Flip Top — warm vintage tube",
                      {0.13, 0.12, 0.11}, {0.05, 0.05, 0.05},
                      {0.93, 0.89, 0.80}, {0.55, 0.52, 0.46},
                      {0.09, 0.08, 0.07},
                      {0.24, 0.21, 0.17}, {0.72, 0.30, 0.18},
                      {0.55, 0.52, 0.46}, {1.0, 0.96, 0.88},
                      {0.88, 0.84, 0.74}, {0.48, 0.45, 0.39},
                      {0.10, 0.09, 0.08}, {0.86, 0.48, 0.22},
                      {0.10, 0.09, 0.08}, {0.70, 0.31, 0.18},
                      {0.92, 0.88, 0.80}, {1.0, 0.97, 0.90},
                      {0.86, 0.48, 0.22}, true);
    case 2:  // SVT — dark blue / steel
      return makeSkin("SVT — big classic tube",
                      {0.16, 0.20, 0.27}, {0.08, 0.11, 0.16},
                      {0.94, 0.96, 0.98}, {0.60, 0.67, 0.75},
                      {0.06, 0.08, 0.11},
                      {0.22, 0.27, 0.34}, {0.26, 0.48, 0.78},
                      {0.72, 0.78, 0.84}, {0.98, 0.99, 1.0},
                      {0.86, 0.90, 0.94}, {0.42, 0.48, 0.56},
                      {0.14, 0.18, 0.24}, {0.24, 0.52, 0.86},
                      {0.22, 0.27, 0.33}, {0.26, 0.48, 0.80},
                      {0.88, 0.92, 0.96}, {1.0, 1.0, 1.0},
                      {0.26, 0.48, 0.80}, true);
    case 3:  // Hot Rod — black / gold
      return makeSkin("Modern — aggressive bass",
                      {0.10, 0.10, 0.10}, {0.03, 0.03, 0.03},
                      {0.93, 0.89, 0.72}, {0.56, 0.52, 0.40},
                      {0.05, 0.05, 0.05},
                      {0.24, 0.22, 0.17}, {0.80, 0.66, 0.30},
                      {0.58, 0.54, 0.42}, {1.0, 0.97, 0.84},
                      {0.83, 0.82, 0.78}, {0.42, 0.42, 0.40},
                      {0.09, 0.09, 0.08}, {0.88, 0.70, 0.28},
                      {0.06, 0.06, 0.05}, {0.78, 0.62, 0.26},
                      {0.93, 0.89, 0.76}, {1.0, 0.98, 0.90},
                      {0.88, 0.70, 0.28}, true);
    default:  // Modern
      return skinFor(3);
  }
}

constexpr double kPi = 3.14159265358979323846;

constexpr int kTunerPage = 5;
constexpr double kPresetX = 72.0;
constexpr double kPresetY = 14.0;
constexpr double kPresetW = 330.0;
constexpr double kPresetH = 36.0;
constexpr double kListY = 54.0;
constexpr double kListRow = 22.0;
constexpr int kListVisible = 10;
constexpr double kListFooterH = 42.0;

constexpr double kUserBoxX = 60.0;
constexpr double kUserBoxY = 300.0;
constexpr double kUserBoxW = 420.0;
constexpr double kUserBoxH = 260.0;
constexpr double kUserRowY = 352.0;
constexpr double kUserRowH = 20.0;
constexpr int kUserRows = 10;

constexpr double kFbX = 170.0;
constexpr double kFbY = 90.0;
constexpr double kFbW = 700.0;
constexpr double kFbH = 460.0;
constexpr double kFbRowY = kFbY + 44.0;
constexpr double kFbRowH = 20.0;
constexpr int kFbRows = 18;

double knobAngle(double f) {
  return (135.0 + 270.0 * f) * kPi / 180.0;
}

double clamp01(double v) { return std::min(std::max(v, 0.0), 1.0); }

double dbToNorm(double db, double minDb, double maxDb) {
  if (!std::isfinite(db)) {
    return 0.0;
  }
  return clamp01((db - minDb) / (maxDb - minDb));
}

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 3 * kPi / 2);
  cairo_close_path(cr);
}

void setRgb(cairo_t* cr, Rgb c, double a = 1.0) {
  cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

Rgb mix(Rgb a, Rgb b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t};
}

bool hasPresence(int model) { return model != 0; }
bool hasResonance(int model) { return model == 2 || model == 3; }
const char* factoryCabName(int set) {
  switch (set) {
    case 0: return "1×15";
    case 1: return "4×10";
    case 2: return "2×12";
    case 3: return "8×10";
    case 4: return "2×10";
    default: return "1×12";
  }
}

const char* factoryCabSubtitle(int set) {
  switch (set) {
    case 0: return "warm vintage · round low end";
    case 1: return "tight punch · fast midrange";
    case 2: return "warm and wide · classic rock";
    case 3: return "big stack · deep and bright";
    case 4: return "tight punch · fast midrange";
    default: return "warm and focused · studio";
  }
}

const char* cabName(int, int cab, int cabSet) {
  if (cab == 1) {
    return "User IR";
  }
  return factoryCabName(std::min(std::max(cabSet, 0), 5));
}

const char* cabSubtitle(int, int cab, int cabSet) {
  if (cab == 1) {
    return "user impulse response · factory fallback";
  }
  return factoryCabSubtitle(std::min(std::max(cabSet, 0), 5));
}

int cabSetFrom(const Shared& shared) {
  return std::min(std::max(static_cast<int>(std::lround(shared.get(P_CAB_SET))), 0), 5);
}

const char* modelName(int model) {
  const int m = std::min(std::max(model, 0), 3);
  return kModelLabels[m];
}

std::string shorten(std::string s, size_t maxLen = 40) {
  if (s.size() > maxLen) {
    s.resize(maxLen > 3 ? maxLen - 3 : 0);
    s += "...";
  }
  return s;
}

double nowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string selectedUserIrName(const Shared& shared, const PanelState& state) {
  const int model =
      std::min(std::max(static_cast<int>(shared.get(P_MODEL) + 0.5), 0), 3);
  const int index =
      static_cast<int>(shared.get(P_USER_IR_0 + model) + 0.5);
  if (index > 0 && index <= static_cast<int>(state.userIrs.size())) {
    return shorten(state.userIrs[static_cast<size_t>(index - 1)], 30);
  }
  return {};
}

} // namespace

Panel::Panel() {
  grain_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
  unsigned char* data = cairo_image_surface_get_data(grain_);
  const int stride = cairo_image_surface_get_stride(grain_);
  unsigned int seed = 22222u;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      seed = seed * 1103515245u + 12345u;
      const unsigned v = (seed >> 16) & 0xff;
      const unsigned a = (v < 128) ? 16u : 0u;
      const unsigned c = (v < 128) ? 0u : 255u;
      unsigned char* px = data + y * stride + x * 4;
      px[0] = static_cast<unsigned char>(c & 0xff);
      px[1] = static_cast<unsigned char>(c & 0xff);
      px[2] = static_cast<unsigned char>(c & 0xff);
      px[3] = static_cast<unsigned char>(a);
    }
  }
  cairo_surface_mark_dirty(grain_);
  fallbackFont_ = cairo_toy_font_face_create(
      "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  font_ = fallbackFont_;
  reloadUserPresets();
}

Panel::~Panel() {
  if (grain_) {
    cairo_surface_destroy(grain_);
  }
  if (font_ && font_ != fallbackFont_) {
    cairo_font_face_destroy(font_);
  }
  if (fallbackFont_) {
    cairo_font_face_destroy(fallbackFont_);
  }
  if (ftFace_) {
    FT_Done_Face(static_cast<FT_Face>(ftFace_));
  }
  if (ft_) {
    FT_Done_FreeType(static_cast<FT_Library>(ft_));
  }
}

double Panel::presetResetValue(const PanelState& state,
                               uint32_t param) const {
  double value = kParamSpecs[param].def;
  if (state.presetIndex >= kPresetCount) {
    const int user = state.presetIndex - kPresetCount;
    if (user >= 0 && user < static_cast<int>(userPresets_.size()) &&
        param < userPresets_[static_cast<size_t>(user)].values.size()) {
      value = userPresets_[static_cast<size_t>(user)].values[param];
    }
    return value;
  }
  if (state.presetIndex >= 0 && state.presetIndex < kPresetCount) {
    for (const PresetValue& v : kPresets[state.presetIndex].values) {
      if (v.param == static_cast<int>(param)) {
        value = v.value;
        break;
      }
    }
  }
  return value;
}

void Panel::loadFont(const std::string& dir) {
  if (ft_) {
    return;
  }
  std::string path = dir;
  if (!path.empty() && path.back() != '/') {
    path += '/';
  }
  path += "Jost-ExtraBold.ttf";
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) {
    return;
  }
  FT_Face face = nullptr;
  if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
    FT_Done_FreeType(lib);
    return;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(face, 0);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    cairo_font_face_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return;
  }
  ft_ = lib;
  ftFace_ = face;
  font_ = font;
}

void Panel::text(cairo_t* cr, const char* s, double x, double y, double size,
                 int align, double r, double g, double b) const {
  cairo_set_font_face(cr, font_);
  cairo_set_font_size(cr, size);
  cairo_set_source_rgb(cr, r, g, b);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  double tx = x;
  if (align == 1) {
    tx = x - e.x_advance * 0.5;
  } else if (align == 2) {
    tx = x - e.x_advance;
  }
  cairo_move_to(cr, tx, y);
  cairo_show_text(cr, s);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void Panel::layout(const Shared& shared, const PanelState& state) {
  widgets_.clear();
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);

  auto knob = [&](uint32_t param, double x, double y, double r,
                  const char* label, std::vector<const char*> scale = {},
                  bool below = false, int steps = 0) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = param;
    k.x = x;
    k.y = y;
    k.r = r;
    k.label = label;
    k.scale = std::move(scale);
    k.labelBelow = below;
    k.steps = steps;
    widgets_.push_back(k);
  };
  auto button = [&](uint32_t param, double x, double y, double w, double h,
                    double value, const char* label) {
    Widget b{};
    b.kind = Kind::Button;
    b.param = param;
    b.x = x;
    b.y = y;
    b.w = w;
    b.h = h;
    b.value = value;
    b.label = label;
    widgets_.push_back(b);
  };
  auto pill = [&](uint32_t param, double x, double y, double w, double h,
                  const char* label) {
    Widget p{};
    p.kind = Kind::Pill;
    p.param = param;
    p.x = x;
    p.y = y;
    p.w = w;
    p.h = h;
    p.value = 1.0;
    p.label = label;
    widgets_.push_back(p);
  };
  auto pagePill = [&](int page, double x, double y, double w, double h,
                      const char* label) {
    Widget p{};
    p.kind = Kind::PagePill;
    p.param = P_COUNT;
    p.value = page;
    p.x = x;
    p.y = y;
    p.w = w;
    p.h = h;
    p.label = label;
    widgets_.push_back(p);
  };
  auto toggle = [&](uint32_t param, double x, double y, double value,
                    const char* top, const char* bottom) {
    Widget t{};
    t.kind = Kind::Toggle;
    t.param = param;
    t.x = x;
    t.y = y;
    t.w = 30;
    t.h = 56;
    t.value = value;
    t.label = top;
    t.buttonText = {top, bottom};
    widgets_.push_back(t);
  };
  auto foot = [&](uint32_t param, double x, double y, double r,
                  const char* label) {
    Widget f{};
    f.kind = Kind::Footswitch;
    f.param = param;
    f.x = x;
    f.y = y;
    f.r = r;
    f.label = label;
    widgets_.push_back(f);
  };

  // Header: power, preset bar, page/toggle pills.
  {
    Widget p{};
    p.kind = Kind::Power;
    p.param = P_POWER;
    p.x = 34;
    p.y = 30;
    p.r = 14;
    widgets_.push_back(p);
  }
  {
    Widget b{};
    b.kind = Kind::PresetBar;
    b.param = P_COUNT;
    b.x = kPresetX;
    b.y = kPresetY;
    b.w = kPresetW;
    b.h = kPresetH;
    b.label = "Preset";
    widgets_.push_back(b);
  }
  pagePill(1, 638, 20, 76, 24, "PEDALS");
  pagePill(2, 722, 20, 58, 24, "CAB");
  pagePill(3, 788, 20, 58, 24, "NAM");
  pagePill(4, 854, 20, 58, 24, "OUT");
  pagePill(kTunerPage, 584, 20, 48, 24, "TUNE");

  // Model tabs.
  const int tabCount = static_cast<int>(Model::Count);
  const double tabW = (980.0 - (tabCount - 1) * 8.0) / tabCount;
  for (int i = 0; i < tabCount; ++i) {
    Widget t{};
    t.kind = Kind::ModelTab;
    t.param = P_MODEL;
    t.value = i;
    t.x = 30 + i * (tabW + 8.0);
    t.y = 70;
    t.w = tabW;
    t.h = 40;
    t.label = kModelLabels[i];
    widgets_.push_back(t);
  }

  if (state.page == 0) {
    // Amp page.
    static const std::vector<const char*> kToneScale = {"0", "5", "10"};
    static const std::vector<const char*> kTrimScale = {"-12", "0", "+12"};
    knob(P_GAIN, 190, 420, 32, "GAIN", kToneScale);
    knob(P_BASS, 300, 420, 28, "BASS", kToneScale);
    knob(P_MID, 410, 420, 28, "MID", kToneScale);
    knob(P_TREBLE, 520, 420, 28, "TREBLE", kToneScale);
    if (hasPresence(model)) {
      knob(P_PRESENCE, 630, 420, 26, "PRESENCE", kToneScale);
    }
    if (hasResonance(model)) {
      knob(P_RESONANCE, 740, 420, 26, "RESONANCE", kToneScale);
    }
    pill(P_GATE_ON, 86, 150, 64, 22, "GATE");
    knob(P_GATE_THRESH, 118, 215, 22, "THRESH", {}, true);
    knob(P_GATE_DECAY, 118, 300, 20, "DECAY", {}, true);
    knob(P_INPUT, 118, 520, 22, "INPUT", kTrimScale, true);
    knob(P_MASTER, 465, 540, 26, "MASTER", {"-18", "-9", "0"});

    // Amp bypass: keep the Comp, Drive and cab active without the preamp/power.
    pill(P_AMP_BYPASS, 852, 145, 96, 24, "AMP BYPASS");

  } else if (state.page == 1) {
    // Pedalboard page: Comp -> Octave -> Drive.
    const double cy = 185;
    // Comp card.
    knob(P_COMP_AMOUNT, 100, cy + 110, 28, "AMOUNT", {}, true);
    knob(P_COMP_ATTACK, 190, cy + 110, 28, "ATTACK", {}, true);
    knob(P_COMP_RELEASE, 280, cy + 110, 28, "RELEASE", {}, true);
    knob(P_COMP_MIX, 190, cy + 215, 26, "MIX", {}, true);
    foot(P_COMP_ON, 190, cy + 330, 25, "COMP");

    // Octave card.
    knob(P_OCT_LEVEL, 430, cy + 110, 28, "LEVEL", {}, true);
    knob(P_OCT_DRY, 520, cy + 110, 28, "DRY", {}, true);
    knob(P_OCT_TONE, 610, cy + 110, 28, "TONE", {}, true);
    foot(P_OCT_ON, 520, cy + 330, 25, "OCTAVE");

    // Drive card.
    knob(P_OD_DRIVE, 760, cy + 110, 28, "DRIVE", {}, true);
    knob(P_OD_TONE, 850, cy + 110, 28, "TONE", {}, true);
    knob(P_OD_LEVEL, 940, cy + 110, 28, "LEVEL", {}, true);
    knob(P_OD_MIX, 850, cy + 215, 26, "BLEND", {}, true);
    foot(P_OD_ON, 850, cy + 330, 25, "DRIVE");

  } else if (state.page == 3) {
    // NAM page.
    pill(P_NAM_ON, 852, 300, 96, 24, "NAM");
    knob(P_NAM_LEVEL, 900, 380, 26, "LEVEL", {}, true);
  } else if (state.page == kTunerPage) {
    // Tuner has no widgets; the display is drawn as a page.
  } else if (state.page == 4) {
    // Output page: 4-band EQ, HPF/LPF, limiter, output.
    static const std::vector<const char*> kTrimScale = {"-12", "0", "+12"};
    knob(P_OUT_EQ_LOW, 170, 285, 32, "LOW");
    knob(P_OUT_EQ_LOW_MID, 330, 285, 32, "LOW MID");
    knob(P_OUT_EQ_HIGH_MID, 490, 285, 32, "HIGH MID");
    knob(P_OUT_EQ_HIGH, 650, 285, 32, "HIGH");
    knob(P_OUT_HPF, 800, 285, 26, "HPF");
    knob(P_OUT_LPF, 920, 285, 26, "LPF");
    pill(P_LIMITER_ON, 160, 525, 96, 24, "LIMITER");
    knob(P_LIMITER_THRESH, 300, 555, 26, "THRESH",
         {"-24", "-12", "-6", "0"});
    knob(P_OUTPUT, 920, 545, 30, "OUTPUT", kTrimScale);
  } else {
    // Cabinet page.
    button(P_CAB, 60, 200, 110, 28, 0.0, "FACTORY");
    button(P_CAB, 180, 200, 110, 28, 1.0, "USER");

    for (int i = 0; i < 6; ++i) {
      button(P_CAB_SET, 513, 300 + i * 34, 180, 28, i,
             kParamSpecs[P_CAB_SET].labels.names[i]);
    }

    button(P_CAB_MIC, 725, 200, 80, 26, 0.0, "DYNAMIC");
    button(P_CAB_MIC, 815, 200, 80, 26, 1.0, "RIBBON");
    button(P_CAB_MIC, 905, 200, 70, 26, 2.0, "BLEND");

    knob(P_CAB_LEVEL, 760, 340, 28, "LEVEL", {}, true);
    knob(P_CAB_LOWCUT, 850, 340, 26, "LOW CUT", {}, true);
    knob(P_CAB_HIGHCUT, 940, 340, 26, "HIGH CUT", {}, true);
    toggle(P_CAB_BYPASS, 760, 440, 0.0, "BYPASS", "CAB");
  }
}

// ---------------------------------------------------------------------------
// Hit testing and interaction
// ---------------------------------------------------------------------------

bool Panel::hitWidget(const Widget& w, double x, double y) const {
  switch (w.kind) {
    case Kind::Knob: {
      const double dx = x - w.x;
      const double dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.3;
    }
    case Kind::Power: {
      const double dx = x - w.x;
      const double dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.8;
    }
    case Kind::Footswitch: {
      const double dx = x - w.x;
      const double dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.7;
    }
    case Kind::Toggle:
      return x >= w.x - w.w * 0.5 && x <= w.x + w.w * 0.5 &&
             y >= w.y && y <= w.y + w.h;
    default:
      return x >= w.x && x <= w.x + w.w && y >= w.y && y <= w.y + w.h;
  }
}

double Panel::knobValue(const Shared& shared, const Widget& w) const {
  return shared.get(w.param);
}

bool Panel::isLit(const Shared& shared, const PanelState& state,
                  const Widget& w) const {
  if (w.kind == Kind::PagePill) {
    return state.page == static_cast<int>(w.value);
  }
  if (w.param >= P_COUNT) {
    return false;
  }
  return shared.get(w.param) > 0.5;
}

bool Panel::pointerDown(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  (void)fine;
  if (editing_) {
    if (hitTextEntry(x, y)) {
      return true;
    }
    editing_ = false;
    editText_.clear();
  }
  if (state.fileBrowserOpen) {
    return handleFileBrowserClick(shared, state, x, y);
  }
  if (handlePresetClick(shared, state, x, y)) {
    return true;
  }
  if (state.page == 3 && x >= kUserBoxX && x <= kUserBoxX + kUserBoxW &&
      y >= kUserBoxY && y <= kUserBoxY + kUserBoxH) {
    if (y < kUserRowY) {
      openFileBrowser(state, true);
      return true;
    }
    const int row = static_cast<int>((y - kUserRowY) / kUserRowH) +
                    state.namScroll;
    const int files = static_cast<int>(state.namModels.size());
    if (row == 0) {
      shared.set(P_NAM_MODEL, 0.0);
      shared.set(P_NAM_ON, 0.0);
      markEdited(state);
    } else if (row >= 1 && row <= files) {
      shared.set(P_NAM_MODEL, static_cast<double>(row));
      shared.set(P_NAM_ON, 1.0);
      markEdited(state);
    } else {
      openFileBrowser(state, true);
    }
    return true;
  }
  if (state.page == 2 && x >= kUserBoxX && x <= kUserBoxX + kUserBoxW &&
      y >= kUserBoxY && y <= kUserBoxY + kUserBoxH) {
    if (y < kUserRowY) {
      openFileBrowser(state);
      return true;
    }
    const int row = static_cast<int>((y - kUserRowY) / kUserRowH) +
                    state.userIrScroll;
    const int files = static_cast<int>(state.userIrs.size());
    const int model =
        std::min(std::max(static_cast<int>(shared.get(P_MODEL) + 0.5), 0), 3);
    if (row == 0) {
      shared.set(P_USER_IR_0 + model, 0.0);
      shared.set(P_CAB, 0.0);
      markEdited(state);
    } else if (row >= 1 && row <= files) {
      shared.set(P_USER_IR_0 + model, static_cast<double>(row));
      shared.set(P_CAB, 1.0);
      markEdited(state);
    } else {
      openFileBrowser(state);
    }
    return true;
  }
  for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
    const Widget& w = *it;
    if (!hitWidget(w, x, y)) {
      continue;
    }
    switch (w.kind) {
      case Kind::Knob: {
        const double now = nowMs();
        const bool doubleClick =
            lastClickParam_ == static_cast<int>(w.param) &&
            now - lastClickTime_ <= kDoubleClickMs;
        lastClickParam_ = static_cast<int>(w.param);
        lastClickTime_ = now;
        if (doubleClick) {
          shared.set(w.param, presetResetValue(state, w.param));
          markEdited(state);
          dragParam_ = -1;
          return true;
        }
        dragParam_ = static_cast<int>(w.param);
        dragStartY_ = y;
        dragStartValue_ = shared.get(w.param);
        return true;
      }
      case Kind::ModelTab: {
        const int model = static_cast<int>(w.value);
        if (static_cast<int>(shared.get(P_MODEL) + 0.5) == model) {
          // Clicking the already-selected amp returns to the main amp page;
          // an unselected amp changes model but leaves the open page alone.
          state.page = 0;
          return true;
        }
        shared.set(P_MODEL, w.value);
        shared.set(P_NAM_ON, 0.0);
        // Sensible cab defaults when auditioning amps; the Cab page can
        // override them immediately.
        if (model == 0) {
          shared.set(P_CAB_SET, 0.0);
          shared.set(P_CAB_BYPASS, 1.0);
        } else if (model == 1) {
          shared.set(P_CAB_SET, 0.0);
          shared.set(P_CAB_BYPASS, 0.0);
        } else if (model == 2) {
          shared.set(P_CAB_SET, 3.0);
          shared.set(P_CAB_BYPASS, 0.0);
        } else {
          shared.set(P_CAB_SET, 1.0);
          shared.set(P_CAB_BYPASS, 0.0);
        }
        shared.set(P_CAB, 0.0);
        state.presetIndex = -1;
        state.presetDirty = false;
        return true;
      }
      case Kind::PagePill:
        state.page = (state.page == static_cast<int>(w.value))
                         ? 0
                         : static_cast<int>(w.value);
        return true;
      case Kind::PresetBar:
        state.presetOpen = !state.presetOpen;
        return true;
      case Kind::Power:
        shared.set(P_POWER, shared.get(P_POWER) > 0.5 ? 0.0 : 1.0);
        markEdited(state);
        return true;
      case Kind::Toggle:
      case Kind::Footswitch:
        shared.set(w.param,
                   shared.get(w.param) > 0.5 ? 0.0 : 1.0);
        markEdited(state);
        return true;
      case Kind::Button:
        if (w.param >= P_COUNT) {
          return true;
        }
        if (w.param == P_CAB_SET) {
          shared.set(P_CAB, 0.0);
          shared.set(w.param, w.value);
          markEdited(state);
          return true;
        }
        if (w.param == P_CAB && w.value > 0.5) {
          const int model = std::min(
              std::max(static_cast<int>(shared.get(P_MODEL) + 0.5), 0), 3);
          if (shared.get(P_USER_IR_0 + model) < 0.5) {
            openFileBrowser(state);
          }
        }
        shared.set(w.param, w.value);
        markEdited(state);
        return true;
      case Kind::Pill:
        if (w.param >= P_COUNT) {
          return true;
        }
        if (kParamSpecs[w.param].toggled) {
          shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
        } else {
          shared.set(w.param, w.value);
        }
        markEdited(state);
        return true;
    }
  }
  return false;
}

bool Panel::pointerMove(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  (void)x;
  if (dragParam_ < 0) {
    return false;
  }
  const ParamSpec& spec = kParamSpecs[dragParam_];
  const double delta = (dragStartY_ - y) * (fine ? 0.0009 : 0.0045);
  double v = dragStartValue_ + delta;
  v = std::min(std::max(v, spec.min), spec.max);
  if (spec.stepped) {
    v = std::round(v);
  }
  shared.set(static_cast<uint32_t>(dragParam_), v);
  markEdited(state);
  return true;
}

void Panel::pointerUp() { dragParam_ = -1; }

bool Panel::wheel(Shared& shared, PanelState& state, double x, double y,
                  double dy) {
  if (state.fileBrowserOpen) {
    const int maxScroll =
        std::max(0, static_cast<int>(state.fileBrowserEntries.size()) - kFbRows);
    state.fileBrowserScroll += dy > 0.0 ? -1 : 1;
    state.fileBrowserScroll =
        std::min(std::max(state.fileBrowserScroll, 0), maxScroll);
    return true;
  }
  if (state.presetOpen && x >= kPresetX && x <= kPresetX + kPresetW &&
      y >= kListY &&
      y <= kListY + kListVisible * kListRow + kListFooterH + 10) {
    const int maxScroll = std::max(0, presetCount() - kListVisible);
    state.presetScroll += dy > 0.0 ? -1 : 1;
    state.presetScroll = std::min(std::max(state.presetScroll, 0), maxScroll);
    return true;
  }
  if (state.page == 2 && x >= kUserBoxX && x <= kUserBoxX + kUserBoxW &&
      y >= kUserBoxY && y <= kUserBoxY + kUserBoxH) {
    const int rows = static_cast<int>(state.userIrs.size()) + 1;
    const int maxScroll = std::max(0, rows - kUserRows);
    state.userIrScroll += dy > 0.0 ? -1 : 1;
    state.userIrScroll = std::min(std::max(state.userIrScroll, 0), maxScroll);
    return true;
  }
  if (state.page == 3 && x >= kUserBoxX && x <= kUserBoxX + kUserBoxW &&
      y >= kUserBoxY && y <= kUserBoxY + kUserBoxH) {
    const int rows = static_cast<int>(state.namModels.size()) + 1;
    const int maxScroll = std::max(0, rows - kUserRows);
    state.namScroll += dy > 0.0 ? -1 : 1;
    state.namScroll = std::min(std::max(state.namScroll, 0), maxScroll);
    return true;
  }
  for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
    const Widget& w = *it;
    if (!hitWidget(w, x, y)) {
      continue;
    }
    if (w.kind == Kind::Knob) {
      const ParamSpec& spec = kParamSpecs[w.param];
      const double step = spec.stepped ? 1.0 : 0.02;
      double v = shared.get(w.param) + (dy > 0.0 ? step : -step);
      v = std::min(std::max(v, spec.min), spec.max);
      if (spec.stepped) {
        v = std::round(v);
      }
      shared.set(w.param, v);
      markEdited(state);
      return true;
    }
  }
  return false;
}

void Panel::tick(const Shared& shared, PanelState& state) const {
  auto norm = [](float db, double minDb, double maxDb) {
    return static_cast<float>(dbToNorm(db, minDb, maxDb));
  };
  // If the host bypasses the plug-in it may stop calling process().  The
  // heartbeat stops and the meters decay instead of freezing at the last
  // value.
  const uint32_t serial =
      shared.meterSerial.load(std::memory_order_relaxed);
  if (serial != state.lastMeterSerial) {
    state.lastMeterSerial = serial;
    state.meterIdleTicks = 0;
  } else if (state.meterIdleTicks < 1000) {
    ++state.meterIdleTicks;
  }
  const bool idle = state.meterIdleTicks > 6;
  const float inL =
      idle ? 0.0f : norm(shared.inPeakLDb.load(std::memory_order_relaxed), -60, 0);
  const float inR =
      idle ? 0.0f : norm(shared.inPeakRDb.load(std::memory_order_relaxed), -60, 0);
  const float outL =
      idle ? 0.0f : norm(shared.outPeakLDb.load(std::memory_order_relaxed), -60, 3);
  const float outR =
      idle ? 0.0f : norm(shared.outPeakRDb.load(std::memory_order_relaxed), -60, 3);
  state.inLevelL = 0.65f * state.inLevelL + 0.35f * inL;
  state.inLevelR = 0.65f * state.inLevelR + 0.35f * inR;
  state.outLevelL = 0.65f * state.outLevelL + 0.35f * outL;
  state.outLevelR = 0.65f * state.outLevelR + 0.35f * outR;
  state.inLevel = std::max(state.inLevelL, state.inLevelR);
  state.outLevel = std::max(state.outLevelL, state.outLevelR);
  state.outPeakDb = idle
                        ? -120.0f
                        : shared.outPeakDb.load(std::memory_order_relaxed);
  if (++state.userIrScanTicks >= 60) {
    state.userIrScanTicks = 0;
    state.userIrs = listUserIrFiles(userIrDir());
    const int rows = static_cast<int>(state.userIrs.size()) + 1;
    const int maxScroll = std::max(0, rows - kUserRows);
    state.userIrScroll =
        std::min(std::max(state.userIrScroll, 0), maxScroll);
  }
  if (++state.namScanTicks >= 60) {
    state.namScanTicks = 0;
    state.namModels = listNamFiles(userNamDir());
    const int rows = static_cast<int>(state.namModels.size()) + 1;
    const int maxScroll = std::max(0, rows - kUserRows);
    state.namScroll = std::min(std::max(state.namScroll, 0), maxScroll);
  }
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

void Panel::drawPresetBar(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  roundRect(cr, kPresetX, kPresetY, kPresetW, kPresetH, 4);
  setRgb(cr, mix(skin.headerBg, {0, 0, 0}, 0.15));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.12);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kPresetY + 0.5, kPresetW - 1, kPresetH - 1, 4);
  cairo_stroke(cr);

  // Previous / next arrows, as in Comp Suite.
  text(cr, "\u2039", kPresetX + 18, kPresetY + 24, 20, 1, 0.72, 0.75, 0.78);
  text(cr, "\u203a", kPresetX + kPresetW - 18, kPresetY + 24, 20, 1, 0.72,
       0.75, 0.78);

  char buf[160];
  if (state.presetIndex < 0) {
    std::snprintf(buf, sizeof(buf), "Manual — %s", modelName(model));
  } else if (state.presetIndex >= kPresetCount) {
    std::snprintf(buf, sizeof(buf), "%s%s", presetName(state.presetIndex),
                  state.presetDirty ? " *" : "");
  } else {
    const int index =
        std::min(std::max(state.presetIndex, 0), kPresetCount - 1);
    if (model != kPresets[index].model) {
      std::snprintf(buf, sizeof(buf), "Manual — %s", modelName(model));
    } else {
      std::snprintf(buf, sizeof(buf), "%s%s", kPresets[index].name,
                    state.presetDirty ? " *" : "");
    }
  }
  text(cr, buf, kPresetX + kPresetW * 0.5, kPresetY + 24, 13.5, 1, 0.90,
       0.92, 0.94);
}

void Panel::drawPresetList(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  const int count = presetCount();
  const int maxScroll = std::max(0, count - kListVisible);
  const int scroll = std::min(std::max(state.presetScroll, 0), maxScroll);
  const double h = kListVisible * kListRow + kListFooterH + 10.0;
  roundRect(cr, kPresetX, kListY, kPresetW, h, 5);
  setRgb(cr, mix(skin.headerBg, {0, 0, 0}, 0.15));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.14);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kListY + 0.5, kPresetW - 1, h - 1, 5);
  cairo_stroke(cr);

  cairo_save(cr);
  roundRect(cr, kPresetX + 1, kListY + 1, kPresetW - 2,
            kListVisible * kListRow + 8, 4);
  cairo_clip(cr);
  for (int row = 0; row < kListVisible; ++row) {
    const int i = scroll + row;
    if (i < 0 || i >= count) {
      break;
    }
    const double y = kListY + 5 + row * kListRow;
    if (i == state.presetIndex) {
      roundRect(cr, kPresetX + 3, y, kPresetW - 6, kListRow - 1, 3);
      setRgb(cr, skin.accent, 0.35);
      cairo_fill(cr);
    }
    text(cr, presetName(i), kPresetX + 12, y + 15, 11.5, 0,
         skin.btnText.r, skin.btnText.g, skin.btnText.b);
    text(cr, i < kPresetCount ? kPresets[i].category : "User",
         kPresetX + kPresetW - 10, y + 15, 9.5, 2, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }
  cairo_restore(cr);

  // Slim scroll indicator.
  if (count > kListVisible) {
    const double trackY = kListY + 6;
    const double trackH = kListVisible * kListRow - 2;
    const double thumbH = std::max(12.0, trackH * kListVisible / count);
    const double thumbY =
        trackY + (trackH - thumbH) *
                     (maxScroll > 0 ? scroll / double(maxScroll) : 0.0);
    cairo_rectangle(cr, kPresetX + kPresetW - 4, trackY, 2.0, trackH);
    setRgb(cr, {0, 0, 0}, 0.45);
    cairo_fill(cr);
    cairo_rectangle(cr, kPresetX + kPresetW - 4, thumbY, 2.0, thumbH);
    setRgb(cr, skin.accent, 0.8);
    cairo_fill(cr);
  }

  const double footerY = kListY + kListVisible * kListRow + 9.0;
  const bool userCurrent = state.presetIndex >= kPresetCount &&
                           state.presetIndex < count;
  if (state.savePreset || state.renamePreset) {
    const double bx = kPresetX + 10;
    const double bw = kPresetW - 20;
    roundRect(cr, bx, footerY, bw, 24, 4);
    setRgb(cr, {0.035, 0.04, 0.05}, 0.98);
    cairo_fill(cr);
    setRgb(cr, skin.tabOn, 0.9);
    cairo_set_line_width(cr, 1.2);
    roundRect(cr, bx + 0.5, footerY + 0.5, bw - 1, 23, 4);
    cairo_stroke(cr);
    text(cr, state.renamePreset ? "RENAME" : "SAVE AS", bx + 8, footerY + 16,
         8.0, 0, skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
    const std::string shown = shorten(state.presetText, 24) + "|";
    text(cr, shown.c_str(), bx + 68, footerY + 16, 10.5, 0, 0.98, 0.80, 0.32);
    text(cr, "Enter saves · Esc cancels", bx + bw - 8, footerY + 16, 7.5, 2,
         skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
    return;
  }

  auto button = [&](double x, double w, const char* label, bool accent) {
    roundRect(cr, x, footerY, w, 24, 4);
    setRgb(cr, accent ? skin.accent : mix(skin.headerBg, {1, 1, 1}, 0.08));
    cairo_fill(cr);
    text(cr, label, x + w * 0.5, footerY + 16, 8.0, 1,
         accent ? 0.10 : skin.btnText.r, accent ? 0.08 : skin.btnText.g,
         accent ? 0.05 : skin.btnText.b);
  };
  button(kPresetX + 10, 62, "SAVE", false);
  if (userCurrent) {
    button(kPresetX + 80, 72, "RENAME", false);
    button(kPresetX + 160, 72, state.deletePresetConfirm ? "CONFIRM" : "DELETE",
           state.deletePresetConfirm);
  }
  button(kPresetX + kPresetW - 78, 68, "CLOSE", false);
}

bool Panel::handlePresetClick(Shared& shared, PanelState& state, double x,
                              double y) {
  // While the name field is open, swallow clicks so the list cannot be
  // navigated out from under the editor.
  if (state.savePreset || state.renamePreset) {
    return true;
  }
  const int count = presetCount();
  const int maxScroll = std::max(0, count - kListVisible);
  const bool inBar = x >= kPresetX && x <= kPresetX + kPresetW &&
                     y >= kPresetY && y <= kPresetY + kPresetH;
  if (!state.presetOpen) {
    if (!inBar) {
      return false;
    }
    if (x <= kPresetX + 36) {
      const int next = state.presetIndex <= 0 ? 0 : state.presetIndex - 1;
      applyPreset(shared, state, next);
      return true;
    }
    if (x >= kPresetX + kPresetW - 36) {
      const int next =
          state.presetIndex < 0
              ? 0
              : std::min(count - 1, state.presetIndex + 1);
      applyPreset(shared, state, next);
      return true;
    }
    state.presetOpen = true;
    state.deletePresetConfirm = false;
    if (state.presetIndex >= 0) {
      state.presetScroll =
          std::min(std::max(state.presetIndex - kListVisible / 2, 0),
                   maxScroll);
    } else {
      state.presetScroll = 0;
    }
    return true;
  }
  const int scroll = std::min(std::max(state.presetScroll, 0), maxScroll);
  for (int row = 0; row < kListVisible; ++row) {
    const int i = scroll + row;
    if (i < 0 || i >= count) {
      break;
    }
    const double rowY = kListY + 5 + row * kListRow;
    if (x >= kPresetX && x <= kPresetX + kPresetW && y >= rowY &&
        y < rowY + kListRow) {
      applyPreset(shared, state, i);
      state.presetOpen = false;
      return true;
    }
  }

  const double footerY = kListY + kListVisible * kListRow + 9.0;
  const bool userCurrent = state.presetIndex >= kPresetCount &&
                           state.presetIndex < count;
  const bool inFooter = y >= footerY && y <= footerY + 24;
  if (inFooter && x >= kPresetX + 10 && x <= kPresetX + 72) {
    state.savePreset = true;
    state.renamePreset = false;
    state.deletePresetConfirm = false;
    state.presetText = userCurrent ? presetName(state.presetIndex) : "My Preset";
    state.presetTextFresh = true;
    return true;
  }
  if (userCurrent && inFooter && x >= kPresetX + 80 && x <= kPresetX + 152) {
    state.renamePreset = true;
    state.savePreset = false;
    state.deletePresetConfirm = false;
    state.presetText = presetName(state.presetIndex);
    state.presetTextFresh = true;
    return true;
  }
  if (userCurrent && inFooter && x >= kPresetX + 160 &&
      x <= kPresetX + 232) {
    if (!state.deletePresetConfirm) {
      state.deletePresetConfirm = true;
      return true;
    }
    const std::string name = presetName(state.presetIndex);
    if (deleteUserPreset(userPresetDir(), name)) {
      reloadUserPresets();
      state.presetIndex = -1;
      state.presetDirty = true;
    }
    state.deletePresetConfirm = false;
    state.presetOpen = false;
    return true;
  }
  state.deletePresetConfirm = false;
  if (inFooter && x >= kPresetX + kPresetW - 78 &&
      x <= kPresetX + kPresetW - 10) {
    state.presetOpen = false;
    return true;
  }
  if (inBar || x < kPresetX || x > kPresetX + kPresetW || y < kListY ||
      y > kListY + kListVisible * kListRow + kListFooterH + 10) {
    state.presetOpen = false;
  }
  return true;
}

void Panel::applyPreset(Shared& shared, PanelState& state, int index) const {
  if (index < 0 || index >= presetCount()) {
    return;
  }
  if (index >= kPresetCount) {
    const int user = index - kPresetCount;
    if (user < 0 || user >= static_cast<int>(userPresets_.size())) {
      return;
    }
    const UserPreset& p = userPresets_[static_cast<size_t>(user)];
    for (uint32_t i = 0; i < P_COUNT && i < p.values.size(); ++i) {
      shared.set(i, p.values[i]);
    }
    shared.requestReload();
    state.presetIndex = index;
    state.presetDirty = false;
    return;
  }
  const Preset& p = kPresets[index];
  // Start from the neutral defaults so a preset cannot inherit an octave,
  // drive or output EQ left over from the previous preset.
  for (uint32_t param = 0; param < P_COUNT; ++param) {
    shared.set(param, kParamSpecs[param].def);
  }
  shared.set(P_MODEL, static_cast<double>(p.model));
  for (const PresetValue& v : p.values) {
    if (v.param >= 0 && v.param < static_cast<int>(P_COUNT)) {
      shared.set(static_cast<uint32_t>(v.param), v.value);
    }
  }
  shared.requestReload();
  state.presetIndex = index;
  state.presetDirty = false;
}

int Panel::presetCount() const {
  return kPresetCount + static_cast<int>(userPresets_.size());
}

const char* Panel::presetName(int index) const {
  if (index >= kPresetCount) {
    const int user = index - kPresetCount;
    if (user >= 0 && user < static_cast<int>(userPresets_.size())) {
      return userPresets_[static_cast<size_t>(user)].name.c_str();
    }
    return "";
  }
  if (index >= 0 && index < kPresetCount) {
    return kPresets[index].name;
  }
  return "";
}

void Panel::reloadUserPresets() {
  userPresets_ = loadUserPresets(userPresetDir());
}

bool Panel::commitPresetName(Shared& shared, PanelState& state) {
  const std::string name = sanitizePresetName(state.presetText);
  if (name.empty()) {
    return false;
  }
  if (state.savePreset) {
    if (!saveUserPreset(userPresetDir(), name, shared)) {
      return false;
    }
    reloadUserPresets();
    int index = -1;
    for (int i = 0; i < static_cast<int>(userPresets_.size()); ++i) {
      if (userPresets_[static_cast<size_t>(i)].name == name) {
        index = kPresetCount + i;
        break;
      }
    }
    state.presetIndex = index;
    state.presetDirty = false;
  } else if (state.renamePreset) {
    const int user = state.presetIndex - kPresetCount;
    if (user < 0 || user >= static_cast<int>(userPresets_.size())) {
      return false;
    }
    const std::string oldName = userPresets_[static_cast<size_t>(user)].name;
    const std::vector<double> values =
        userPresets_[static_cast<size_t>(user)].values;
    if (!renameUserPreset(userPresetDir(), oldName, name, values)) {
      return false;
    }
    reloadUserPresets();
    int index = -1;
    for (int i = 0; i < static_cast<int>(userPresets_.size()); ++i) {
      if (userPresets_[static_cast<size_t>(i)].name == name) {
        index = kPresetCount + i;
        break;
      }
    }
    state.presetIndex = index;
  }
  state.savePreset = false;
  state.renamePreset = false;
  state.deletePresetConfirm = false;
  state.presetOpen = false;
  state.presetText.clear();
  state.presetTextFresh = false;
  return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Panel::draw(cairo_t* cr, Shared& shared, const PanelState& state) {
  shared.tunerOn.store(state.page == kTunerPage, std::memory_order_relaxed);
  layout(shared, state);
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);

  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.055, 0.055, 0.06);
  cairo_paint(cr);

  drawHeader(cr, shared, state);
  drawTabs(cr, shared);

  // Faceplate.
  cairo_pattern_t* body = cairo_pattern_create_linear(0, 122, 0, 604);
  cairo_pattern_add_color_stop_rgb(body, 0.0, skin.bodyTop.r,
                                   skin.bodyTop.g, skin.bodyTop.b);
  cairo_pattern_add_color_stop_rgb(body, 1.0, skin.bodyBottom.r,
                                   skin.bodyBottom.g, skin.bodyBottom.b);
  roundRect(cr, 16, 122, 1008, 482, 6);
  cairo_set_source(cr, body);
  cairo_fill(cr);
  cairo_pattern_destroy(body);

  if (grain_) {
    cairo_pattern_t* grain = cairo_pattern_create_for_surface(grain_);
    cairo_pattern_set_extend(grain, CAIRO_EXTEND_REPEAT);
    cairo_save(cr);
    cairo_rectangle(cr, 17, 123, 1006, 480);
    cairo_clip(cr);
    cairo_set_source(cr, grain);
    cairo_paint_with_alpha(cr, 0.018);
    cairo_restore(cr);
    cairo_pattern_destroy(grain);
  }

  // Screws.
  for (double sx : {32.0, 1008.0}) {
    for (double sy : {138.0, 588.0}) {
      cairo_arc(cr, sx, sy, 4.5, 0, 2 * kPi);
      setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.25), 0.9);
      cairo_fill(cr);
    }
  }

  if (state.page == 0) {
    drawAmpPage(cr, shared, state);
  } else if (state.page == 1) {
    drawPedalPage(cr, shared, state);
  } else if (state.page == 3) {
    drawNamPage(cr, shared, state);
  } else if (state.page == 4) {
    drawOutPage(cr, shared, state);
  } else if (state.page == kTunerPage) {
    drawTunerPage(cr, shared, state);
  } else {
    drawCabPage(cr, shared, state);
  }

  drawFooter(cr, shared, state);

  for (const Widget& w : widgets_) {
    drawWidget(cr, w, shared, state, model);
  }

  if (state.presetOpen) {
    drawPresetList(cr, shared, state);
  }
  if (state.fileBrowserOpen) {
    drawFileBrowser(cr, shared, state);
  }
  if (editing_) {
    drawTextEntry(cr, shared, model);
  }

  cairo_restore(cr);
}

void Panel::drawHeader(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  (void)state;
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  cairo_pattern_t* bar = cairo_pattern_create_linear(0, 0, 0, 62);
  cairo_pattern_add_color_stop_rgb(bar, 0.0, skin.headerBg.r * 1.35,
                                   skin.headerBg.g * 1.35,
                                   skin.headerBg.b * 1.35);
  cairo_pattern_add_color_stop_rgb(bar, 1.0, skin.headerBg.r * 0.75,
                                   skin.headerBg.g * 0.75,
                                   skin.headerBg.b * 0.75);
  cairo_rectangle(cr, 0, 0, kPanelWidth, 62);
  cairo_set_source(cr, bar);
  cairo_fill(cr);
  cairo_pattern_destroy(bar);

  setRgb(cr, {1, 1, 1}, 0.08);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, 0.5);
  cairo_line_to(cr, kPanelWidth, 0.5);
  cairo_stroke(cr);
  setRgb(cr, {0, 0, 0}, 0.55);
  cairo_move_to(cr, 0, 61.5);
  cairo_line_to(cr, kPanelWidth, 61.5);
  cairo_stroke(cr);

  text(cr, "BASS SUITE", 520, 38, 19, 1, 0.82, 0.84, 0.86);
}

void Panel::drawTabs(cairo_t* cr, const Shared& shared) const {
  (void)shared;
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
  cairo_rectangle(cr, 0, 62, kPanelWidth, 54);
  cairo_fill(cr);
}

void Panel::drawAmpPage(cairo_t* cr, const Shared& shared,
                        const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  drawCabWindow(cr, shared, state, model);
  drawLeds(cr, state, model);

  // Cab label in the right column.
  const std::string userIr = selectedUserIrName(shared, state);
  const char* cabLabel = cab == 1 && !userIr.empty() ? userIr.c_str()
                                                     : cabName(model, cab, cabSet);
  text(cr, "CABINET", 900, 180, 10, 1, 0.45, 0.47, 0.50);
  text(cr, cabLabel, 900, 202, 13, 1, 0.86, 0.88, 0.90);
  text(cr, cabSubtitle(model, cab, cabSet), 900, 220, 9.5, 1, 0.42, 0.44, 0.47);

  // Output limiter caption, above its pill and knob.

  // Side level meters.
  drawLevelMeter(cr, 20, 150, 16, 352, state.inLevelL, state.inLevelR, model,
                 true, -60.0, 0.0);
  drawLevelMeter(cr, 986, 150, 16, 352, state.outLevelL, state.outLevelR,
                 model, false, -60.0, 3.0);
}

void Panel::drawCabWindow(cairo_t* cr, const Shared& shared,
                          const PanelState& state, int model) const {
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  const Skin skin = skinFor(model);
  roundRect(cr, 170, 136, 570, 204, 5);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.55));
  cairo_fill(cr);
  setRgb(cr, {0, 0, 0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 170.5, 136.5, 569, 203, 5);
  cairo_stroke(cr);

  // Grille cloth.
  cairo_save(cr);
  cairo_rectangle(cr, 174, 140, 562, 196);
  cairo_clip(cr);
  cairo_set_source_rgba(cr, skin.accent.r, skin.accent.g, skin.accent.b, 0.10);
  cairo_rectangle(cr, 174, 140, 562, 196);
  cairo_fill(cr);
  cairo_set_source_rgba(cr, 1, 1, 1, 0.035);
  cairo_set_line_width(cr, 1.0);
  for (double x = 176; x < 736; x += 7) {
    cairo_move_to(cr, x, 140);
    cairo_line_to(cr, x, 336);
    cairo_stroke(cr);
  }
  for (double y = 142; y < 336; y += 7) {
    cairo_move_to(cr, 174, y);
    cairo_line_to(cr, 736, y);
    cairo_stroke(cr);
  }
  cairo_restore(cr);

  // Header strip inside the window, like Comp Suite's meter mode bar.
  roundRect(cr, 178, 142, 554, 24, 3);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.35));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.08);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 178.5, 142.5, 553, 23, 3);
  cairo_stroke(cr);
  const std::string userIr = selectedUserIrName(shared, state);
  text(cr, cab == 1 && !userIr.empty() ? userIr.c_str()
                                       : cabName(model, cab, cabSet),
       190, 159, 12, 0, 0.88, 0.90, 0.92);
  static const char* kMicNames[3] = {"DYNAMIC", "RIBBON", "BLEND"};
  const int mic = std::min(std::max(static_cast<int>(
                                        shared.get(P_CAB_MIC) + 0.5),
                                    0),
                           2);
  char micLabel[32];
  std::snprintf(micLabel, sizeof(micLabel), "%s MIC", kMicNames[mic]);
  text(cr, micLabel, 720, 159, 9, 2, 0.55, 0.57, 0.60);

  // The grille carries the active amp's logo instead of generic speaker
  // circles: a large Jost ExtraBold wordmark, shadowed like a badge, with the
  // active cabinet name in small type underneath.
  std::string logo = modelName(model);
  for (char& ch : logo) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  cairo_set_font_face(cr, font_);
  double logoSize = 60.0;
  cairo_set_font_size(cr, logoSize);
  cairo_text_extents_t logoExt{};
  cairo_text_extents(cr, logo.c_str(), &logoExt);
  const double logoMaxWidth = 465.0;
  if (logoExt.x_advance > logoMaxWidth) {
    logoSize *= logoMaxWidth / logoExt.x_advance;
  }
  const double logoBase = 247.0;
  text(cr, logo.c_str(), 456.0, logoBase + 2.0, logoSize, 1, 0.01, 0.01,
       0.02);
  text(cr, logo.c_str(), 455.0, logoBase, logoSize, 1, 0.91, 0.92, 0.90);

  cairo_set_source_rgba(cr, skin.accent.r, skin.accent.g, skin.accent.b,
                        0.45);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 415.0, 272.0);
  cairo_line_to(cr, 495.0, 272.0);
  cairo_stroke(cr);

  // Input / output bars inside the window.
  auto bar = [&](double x, double y, double w, double v, Rgb c) {
    cairo_rectangle(cr, x, y, w, 5);
    cairo_set_source_rgb(cr, 0.04, 0.04, 0.05);
    cairo_fill(cr);
    cairo_rectangle(cr, x, y, w * clamp01(v), 5);
    setRgb(cr, c);
    cairo_fill(cr);
  };
  text(cr, "IN", 196, 320, 9, 1, 0.55, 0.57, 0.60);
  bar(206, 315, 200, state.inLevel, {0.30, 0.75, 0.42});
  text(cr, "OUT", 516, 320, 9, 1, 0.55, 0.57, 0.60);
  bar(526, 315, 200, state.outLevel, {0.90, 0.70, 0.30});
}

void Panel::drawPedalPage(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  (void)shared;
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  const Skin skin = skinFor(model);
  text(cr, "PEDALBOARD", 520, 150, 18, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  char sub[160];
  std::snprintf(sub, sizeof(sub), "%s  ·  %s", modelName(model),
                cabName(model, cab, cabSet));
  text(cr, sub, 520, 169, 10.5, 1, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);

  struct Card {
    double x;
    const char* title;
    Rgb color;
  };
  const Card cards[3] = {{40, "COMP", {0.18, 0.34, 0.46}},
                         {370, "OCTAVE", {0.34, 0.28, 0.44}},
                         {700, "DRIVE", {0.42, 0.28, 0.18}}};
  for (const Card& c : cards) {
    roundRect(cr, c.x, 185, 300, 380, 8);
    cairo_pattern_t* pat = cairo_pattern_create_linear(0, 185, 0, 565);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, c.color.r * 1.35,
                                     c.color.g * 1.35, c.color.b * 1.35);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, c.color.r * 0.55,
                                     c.color.g * 0.55, c.color.b * 0.55);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
    setRgb(cr, {0, 0, 0}, 0.35);
    cairo_set_line_width(cr, 1.0);
    roundRect(cr, c.x + 0.5, 185.5, 299, 379, 8);
    cairo_stroke(cr);
    text(cr, c.title, c.x + 150, 218, 15, 1, 0.93, 0.93, 0.90);
  }

  text(cr, "BASS  >  COMP  >  OCTAVE  >  DRIVE  >  AMP  >  CAB", 520, 592, 11,
       1, skin.legend.r, skin.legend.g, skin.legend.b);
  (void)state;
}

void Panel::drawCabPage(cairo_t* cr, const Shared& shared,
                        const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  const Skin skin = skinFor(model);
  text(cr, "CABINET", 520, 150, 18, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  char sub[160];
  std::snprintf(sub, sizeof(sub), "%s  ·  %s", modelName(model),
                cabName(model, cab, cabSet));
  text(cr, sub, 520, 169, 10.5, 1, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);

  text(cr, "FACTORY", 60, 190, 10, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  text(cr, "USER", 310, 190, 10, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  text(cr, "FACTORY CABS", 603, 285, 10, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, "Select any cab for the current amp.", 603, 552, 9, 1,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);

  roundRect(cr, kUserBoxX, kUserBoxY, kUserBoxW, kUserBoxH, 6);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.35));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.10);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kUserBoxX + 0.5, kUserBoxY + 0.5, kUserBoxW - 1,
            kUserBoxH - 1, 6);
  cairo_stroke(cr);
  text(cr, "USER IR FILES", 76, kUserBoxY + 20, 11, 0, skin.legend.r,
       skin.legend.g, skin.legend.b);
  text(cr, userIrDir().c_str(), 76, kUserBoxY + 35, 8, 0, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  roundRect(cr, kUserBoxX + kUserBoxW - 94, kUserBoxY + 10, 82, 20, 3);
  setRgb(cr, mix(skin.btnOff, {1, 1, 1}, 0.06));
  cairo_fill(cr);
  setRgb(cr, {0, 0, 0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kUserBoxX + kUserBoxW - 93.5, kUserBoxY + 10.5, 81, 19, 3);
  cairo_stroke(cr);
  text(cr, "BROWSE", kUserBoxX + kUserBoxW - 53, kUserBoxY + 24, 9.5, 1,
       skin.btnText.r, skin.btnText.g, skin.btnText.b);

  const int selected = static_cast<int>(
      shared.get(P_USER_IR_0 + std::min(std::max(model, 0), 3)) + 0.5);
  const int files = static_cast<int>(state.userIrs.size());
  const int rows = std::min(kUserRows, files + 1);
  for (int row = 0; row < rows; ++row) {
    const int index = row + state.userIrScroll;
    if (index > files) {
      break;
    }
    const double ry = kUserRowY + row * kUserRowH;
    if (index == selected) {
      roundRect(cr, kUserBoxX + 4, ry + 1, kUserBoxW - 8, kUserRowH - 2, 3);
      setRgb(cr, skin.accent, 0.35);
      cairo_fill(cr);
    }
    char name[80];
    if (index == 0) {
      std::snprintf(name, sizeof(name), "None");
    } else {
      std::snprintf(name, sizeof(name), "%s",
                    shorten(state.userIrs[static_cast<size_t>(index - 1)], 38)
                        .c_str());
    }
    text(cr, name, kUserBoxX + 12, ry + 14, 9.5, 0,
         index == selected ? skin.btnTextOn.r : skin.legend.r,
         index == selected ? skin.btnTextOn.g : skin.legend.g,
         index == selected ? skin.btnTextOn.b : skin.legend.b);
  }
  if (files == 0) {
    text(cr, "Drop WAV files in this folder.", 76, kUserRowY + 26, 9, 0,
         skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  }

  text(cr, "MIC", 760, 190, 10, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);

  if (cab == 1) {
    const std::string userIr = selectedUserIrName(shared, state);
    text(cr, userIr.empty() ? "User IR — none" : userIr.c_str(), 885, 262, 13,
         1, skin.legend.r, skin.legend.g, skin.legend.b);
  } else {
    text(cr, cabName(model, cab, cabSet), 850, 262, 13, 1, skin.legend.r,
         skin.legend.g, skin.legend.b);
  }
  text(cr, "Factory cabs are CC0 and embedded.", 850, 282, 9.5, 1,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
}

void Panel::drawOutPage(cairo_t* cr, const Shared& shared,
                        const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  const Skin skin = skinFor(model);
  text(cr, "OUTPUT", 520, 150, 18, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  char sub[160];
  std::snprintf(sub, sizeof(sub), "%s  ·  %s", modelName(model),
                cabName(model, cab, cabSet));
  text(cr, sub, 520, 169, 10.5, 1, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);

  // Output EQ response.
  const double x = 160.0, y = 340.0, w = 720.0, h = 140.0;
  roundRect(cr, x, y, w, h, 4);
  cairo_set_source_rgba(cr, 0.025, 0.027, 0.030, 0.96);
  cairo_fill(cr);
  setRgb(cr, skin.tabOn, 0.65);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 4);
  cairo_stroke(cr);

  const double low = (shared.get(P_OUT_EQ_LOW) - 0.5) * 24.0;
  const double lowMid = (shared.get(P_OUT_EQ_LOW_MID) - 0.5) * 24.0;
  const double highMid = (shared.get(P_OUT_EQ_HIGH_MID) - 0.5) * 24.0;
  const double high = (shared.get(P_OUT_EQ_HIGH) - 0.5) * 24.0;
  const double hp = outHpfHz(static_cast<float>(shared.get(P_OUT_HPF)));
  const double lp = outLpfHz(static_cast<float>(shared.get(P_OUT_LPF)));

  auto bell = [](double f, double fc, double q) {
    const double t = std::log2(f / fc);
    return std::exp(-(t * t) / (2.0 * q * q));
  };
  auto curveDb = [&](double f) {
    double d = low / (1.0 + (f / 80.0) * (f / 80.0));
    d += high * (f * f) / (f * f + 6000.0 * 6000.0);
    d += lowMid * bell(f, 250.0, 0.9);
    d += highMid * bell(f, 1200.0, 0.9);
    d -= 12.0 * std::log10(1.0 + (hp / f) * (hp / f));
    d -= 12.0 * std::log10(1.0 + (f / lp) * (f / lp));
    return std::clamp(d, -18.0, 18.0);
  };

  const double cy = y + h * 0.5;
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.18);
  for (double f : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0,
                   5000.0, 10000.0, 20000.0}) {
    const double u = std::log(f / 20.0) / std::log(1000.0);
    const double gx = x + w * u;
    cairo_move_to(cr, gx, y);
    cairo_line_to(cr, gx, y + h);
    cairo_stroke(cr);
  }
  for (int i = -1; i <= 1; ++i) {
    const double gy = cy - (h * 0.4) * i;
    setRgb(cr, skin.legendDim, i == 0 ? 0.40 : 0.16);
    cairo_move_to(cr, x, gy);
    cairo_line_to(cr, x + w, gy);
    cairo_stroke(cr);
  }

  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.knobIndicator, 0.95);
  for (int i = 0; i < static_cast<int>(w); ++i) {
    const double u = i / (w - 1.0);
    const double f = 20.0 * std::pow(1000.0, u);
    const double py = cy - (h * 0.4) * (curveDb(f) / 18.0);
    if (i == 0) cairo_move_to(cr, x + i, py);
    else cairo_line_to(cr, x + i, py);
  }
  cairo_stroke(cr);

  text(cr, "4-BAND EQ", x + 10, y + 14, 9, 0, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  text(cr, "20", x + 4, y + h - 6, 8, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  text(cr, "100", x + w * 0.26, y + h - 6, 8, 1, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  text(cr, "1k", x + w * 0.566, y + h - 6, 8, 1, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  text(cr, "10k", x + w * 0.90, y + h - 6, 8, 2, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);

  text(cr, "LIMITER", 160, 515, 9.5, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);

  drawLevelMeter(cr, 20, 150, 16, 352, state.inLevelL, state.inLevelR, model,
                 true, -60.0, 0.0);
  drawLevelMeter(cr, 986, 150, 16, 352, state.outLevelL, state.outLevelR,
                 model, false, -60.0, 3.0);
}

void Panel::drawTunerPage(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  (void)state;
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  text(cr, "CHROMATIC TUNER", kPanelWidth * 0.5, 158, 18, 1, skin.legend.r,
       skin.legend.g, skin.legend.b);
  text(cr, "INPUT PITCH — NO COMP / OCTAVE / DRIVE", kPanelWidth * 0.5, 177,
       9.5, 1, skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);

  roundRect(cr, 160, 195, 720, 360, 6);
  cairo_set_source_rgba(cr, 0.02, 0.024, 0.028, 0.96);
  cairo_fill(cr);
  setRgb(cr, skin.tabOn, 0.55);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 160.5, 195.5, 719, 359, 6);
  cairo_stroke(cr);

  const int note = shared.tunerNote.load(std::memory_order_relaxed);
  const float hz = shared.tunerHz.load(std::memory_order_relaxed);
  const float cents = shared.tunerCents.load(std::memory_order_relaxed);
  const float confidence =
      shared.tunerConfidence.load(std::memory_order_relaxed);
  const bool has = note >= 0 && confidence > 0.55f;
  static const char* kNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                   "F#", "G",  "G#", "A",  "A#", "B"};
  char noteText[24] = "—";
  if (has) {
    const int pc = note % 12;
    const int octave = note / 12 - 1;
    std::snprintf(noteText, sizeof(noteText), "%s%d", kNames[pc], octave);
  }
  const bool inTune = has && std::fabs(cents) < 5.0f;
  const Rgb noteColor = !has ? skin.legendDim
                             : (inTune ? Rgb{0.30, 0.86, 0.48} : skin.tabOn);
  text(cr, noteText, kPanelWidth * 0.5, 360, 96, 1, noteColor.r, noteColor.g,
       noteColor.b);
  char line[80];
  if (has) {
    std::snprintf(line, sizeof(line), "%+.1f cents   ·   %.1f Hz", cents, hz);
  } else {
    std::snprintf(line, sizeof(line), "Play a note");
  }
  text(cr, line, kPanelWidth * 0.5, 400, 12, 1,
       has ? skin.legend.r : skin.legendDim.r,
       has ? skin.legend.g : skin.legendDim.g,
       has ? skin.legend.b : skin.legendDim.b);

  const double cx = kPanelWidth * 0.5;
  const double rx = 250.0;
  const double ry = 460.0;
  setRgb(cr, skin.legendDim, 0.25);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, cx - rx, ry);
  cairo_line_to(cr, cx + rx, ry);
  cairo_stroke(cr);
  for (int c = -50; c <= 50; c += 10) {
    const double x = cx + rx * c / 50.0;
    const bool major = c == 0;
    setRgb(cr, major ? skin.tabOn : skin.legendDim, major ? 0.9 : 0.45);
    cairo_set_line_width(cr, major ? 2.0 : 1.0);
    cairo_move_to(cr, x, ry - (major ? 14.0 : 8.0));
    cairo_line_to(cr, x, ry + (major ? 14.0 : 8.0));
    cairo_stroke(cr);
    char tick[8];
    std::snprintf(tick, sizeof(tick), "%+d", c);
    text(cr, tick, x, ry + 30, 8.5, 1, skin.legendDim.r, skin.legendDim.g,
         skin.legendDim.b);
  }
  if (has) {
    const double x =
        cx + rx * std::clamp(static_cast<double>(cents), -50.0, 50.0) / 50.0;
    setRgb(cr, noteColor);
    cairo_rectangle(cr, x - 5, ry - 20, 10, 40);
    cairo_fill(cr);
  }
  const char* status = !has ? "WAITING FOR SIGNAL"
                            : (inTune ? "IN TUNE" : (cents < 0 ? "TUNE UP" : "TUNE DOWN"));
  text(cr, status, kPanelWidth * 0.5, 510, 13, 1, noteColor.r, noteColor.g,
       noteColor.b);
  text(cr, "Chromatic  ·  ±50 cents  ·  input before comp / octave / drive",
       kPanelWidth * 0.5, 535, 8.5, 1, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
}

void Panel::drawFooter(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const int cab = static_cast<int>(shared.get(P_CAB) + 0.5);
  const int cabSet = cabSetFrom(shared);
  const Skin skin = skinFor(model);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_rectangle(cr, 0, 604, kPanelWidth, 36);
  cairo_fill(cr);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, 604.5);
  cairo_line_to(cr, kPanelWidth, 604.5);
  cairo_stroke(cr);

  char buf[256];
  if (state.page == 1) {
    std::snprintf(buf, sizeof(buf), "PEDALBOARD  ·  %s", modelName(model));
  } else if (state.page == 2) {
    std::snprintf(buf, sizeof(buf), "CABINET  ·  %s  ·  %s", modelName(model),
                  cabName(model, cab, cabSet));
  } else if (state.page == 3) {
    const int index = static_cast<int>(shared.get(P_NAM_MODEL) + 0.5);
    const std::string name =
        index > 0 && index <= static_cast<int>(state.namModels.size())
            ? state.namModels[static_cast<size_t>(index - 1)]
            : "none";
    std::snprintf(buf, sizeof(buf), "NAM MODEL  ·  %s", name.c_str());
  } else if (state.page == 4) {
    std::snprintf(buf, sizeof(buf), "OUTPUT  ·  EQ / HPF / LPF / LIMITER");
  } else if (state.page == kTunerPage) {
    std::snprintf(buf, sizeof(buf), "TUNER  ·  CHROMATIC INPUT");
  } else {
    std::snprintf(buf, sizeof(buf), "%s  ·  %s", skin.caption,
                  cabName(model, cab, cabSet));
  }
  text(cr, buf, kPanelWidth * 0.5, 629, 13.5, 1, 0.86, 0.88, 0.90);
  text(cr, "v" BASS_SUITE_VERSION, 1016, 631, 9, 2, 0.38, 0.40, 0.43);
}

void Panel::drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                       const PanelState& state, int model) const {
  switch (w.kind) {
    case Kind::Knob:
      drawKnob(cr, w, knobValue(shared, w), model);
      break;
    case Kind::Button: {
      bool lit = false;
      if (w.param < P_COUNT) {
        const ParamSpec& spec = kParamSpecs[w.param];
        lit = spec.stepped
                  ? std::fabs(shared.get(w.param) - w.value) < 0.5
                  : shared.get(w.param) > 0.5;
        if (w.param == P_CAB_SET) {
          lit = shared.get(P_CAB) < 0.5 &&
                std::fabs(shared.get(P_CAB_SET) - w.value) < 0.5;
        }
      }
      drawButton(cr, w, lit, model);
      break;
    }
    case Kind::ModelTab:
      drawModelTab(cr, w, model == static_cast<int>(w.value), model);
      break;
    case Kind::Toggle:
      drawToggle(cr, w, shared.get(w.param) > 0.5, model);
      break;
    case Kind::Footswitch:
      drawFootswitch(cr, w, shared.get(w.param) > 0.5, model);
      break;
    case Kind::Pill: {
      bool on = isLit(shared, state, w);
      Widget ww = w;
      if (w.param == P_NAM_ON) {
        ww.label = on ? "NAM ON" : "AMP ON";
      } else if (w.param == P_GATE_ON) {
        ww.label = on ? "GATE ON" : "GATE OFF";
      } else if (w.param == P_AMP_BYPASS) {
        ww.label = on ? "BYPASS ON" : "AMP ON";
      }
      drawPill(cr, ww, on, model);
      break;
    }
    case Kind::PagePill:
      drawPill(cr, w, state.page == static_cast<int>(w.value), model);
      break;
    case Kind::PresetBar:
      drawPresetBar(cr, shared, state);
      break;
    case Kind::Power: {
      const bool on = shared.get(P_POWER) > 0.5;
      cairo_arc(cr, w.x, w.y, w.r, 0, 2 * kPi);
      setRgb(cr, {0.12, 0.12, 0.12});
      cairo_fill(cr);
      cairo_arc(cr, w.x, w.y, w.r - 1.5, 0, 2 * kPi);
      setRgb(cr, on ? Rgb{0.36, 0.86, 0.46} : Rgb{0.42, 0.42, 0.42});
      cairo_set_line_width(cr, 2.2);
      cairo_new_path(cr);
      cairo_arc(cr, w.x, w.y, 7, -kPi / 2 + 0.6, -kPi / 2 + 2 * kPi - 0.6);
      cairo_stroke(cr);
      cairo_move_to(cr, w.x, w.y - 9);
      cairo_line_to(cr, w.x, w.y - 1);
      cairo_stroke(cr);
      break;
    }
  }
}

void Panel::drawKnob(cairo_t* cr, const Widget& w, double value,
                     int model) const {
  const Skin skin = skinFor(model);
  const double cx = w.x;
  const double cy = w.y;
  const double r = w.r;

  setRgb(cr, skin.legendDim, 0.55);
  cairo_set_line_width(cr, 1.0);
  for (int i = 0; i <= 10; ++i) {
    const double a = knobAngle(i / 10.0);
    cairo_move_to(cr, cx + std::cos(a) * r * 1.10,
                  cy + std::sin(a) * r * 1.10);
    cairo_line_to(cr, cx + std::cos(a) * r * 1.19,
                  cy + std::sin(a) * r * 1.19);
    cairo_stroke(cr);
  }

  const ParamSpec& spec = kParamSpecs[w.param];
  if (w.scale.empty() && !spec.stepped) {
    const double a0 = knobAngle(0.0);
    const double a1 = knobAngle(1.0);
    text(cr, "\u2212", cx + std::cos(a0) * (r * 1.44),
         cy + std::sin(a0) * (r * 1.44) + 4, 12, 1, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
    text(cr, "+", cx + std::cos(a1) * (r * 1.44),
         cy + std::sin(a1) * (r * 1.44) + 4, 12, 1, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }

  for (size_t i = 0; i < w.scale.size(); ++i) {
    const double f = w.scale.size() > 1
                         ? static_cast<double>(i) / (w.scale.size() - 1)
                         : 0.0;
    const double a = knobAngle(f);
    text(cr, w.scale[i], cx + std::cos(a) * (r * w.scaleRadius),
         cy + std::sin(a) * (r * w.scaleRadius) + 4, 10, 1, skin.legend.r,
         skin.legend.g, skin.legend.b);
  }

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
  cairo_arc(cr, cx + 0.5, cy + 1.5, r * 1.03, 0, 2 * kPi);
  cairo_fill(cr);

  cairo_pattern_t* body = cairo_pattern_create_linear(0, cy - r, 0, cy + r);
  const Rgb top = mix(skin.knobSkirt, {1, 1, 1}, 0.16);
  const Rgb bot = mix(skin.knobSkirt, {0, 0, 0}, 0.30);
  cairo_pattern_add_color_stop_rgb(body, 0.0, top.r, top.g, top.b);
  cairo_pattern_add_color_stop_rgb(body, 1.0, bot.r, bot.g, bot.b);
  cairo_arc(cr, cx, cy, r, 0, 2 * kPi);
  cairo_set_source(cr, body);
  cairo_fill(cr);
  cairo_pattern_destroy(body);

  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
  cairo_arc(cr, cx, cy, r - 0.5, 0, 2 * kPi);
  cairo_stroke(cr);

  cairo_pattern_t* cap = cairo_pattern_create_radial(
      cx - r * 0.35, cy - r * 0.40, r * 0.05, cx, cy, r * 0.98);
  const Rgb hi = mix(skin.knobCap, {1, 1, 1}, 0.24);
  cairo_pattern_add_color_stop_rgb(cap, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(cap, 0.75, skin.knobCap.r,
                                   skin.knobCap.g, skin.knobCap.b);
  cairo_pattern_add_color_stop_rgb(cap, 1.0, skin.knobCapEdge.r,
                                   skin.knobCapEdge.g, skin.knobCapEdge.b);
  cairo_arc(cr, cx, cy, r * 0.82, 0, 2 * kPi);
  cairo_set_source(cr, cap);
  cairo_fill(cr);
  cairo_pattern_destroy(cap);

  if (skin.knurled) {
    cairo_set_line_width(cr, 0.8);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    for (int k = 0; k < 60; ++k) {
      const double a = k * 2 * kPi / 60.0;
      cairo_move_to(cr, cx + std::cos(a) * r * 0.86,
                    cy + std::sin(a) * r * 0.86);
      cairo_line_to(cr, cx + std::cos(a) * r * 0.99,
                    cy + std::sin(a) * r * 0.99);
      cairo_stroke(cr);
    }
  }

  const double vmax = w.steps > 1 ? w.steps - 1 : spec.max;
  const double span = vmax - spec.min;
  const double frac =
      span > 0.0 ? (std::min(value, vmax) - spec.min) / span : 0.0;
  const double a = knobAngle(clamp01(frac));
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.knobIndicator);
  cairo_move_to(cr, cx + std::cos(a) * r * 0.26,
                cy + std::sin(a) * r * 0.26);
  cairo_line_to(cr, cx + std::cos(a) * r * 0.78,
                cy + std::sin(a) * r * 0.78);
  cairo_stroke(cr);

  const double labelY =
      w.labelBelow ? cy + r * 1.44 + 20 : cy - r * 1.44 - 18;
  text(cr, w.label, cx, labelY, 10.5, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
}

void Panel::drawButton(cairo_t* cr, const Widget& w, bool lit,
                       int model) const {
  const Skin skin = skinFor(model);
  const Rgb base = lit ? skin.btnOn : skin.btnOff;
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, w.y, 0, w.y + w.h);
  const Rgb hi = mix(base, {1, 1, 1}, lit ? 0.10 : 0.07);
  const Rgb lo = mix(base, {0, 0, 0}, 0.10);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, lo.r, lo.g, lo.b);
  roundRect(cr, w.x, w.y, w.w, w.h, 3);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);
  setRgb(cr, mix(base, lit ? Rgb{1, 1, 1} : Rgb{0, 0, 0}, 0.35), 0.9);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 3);
  cairo_stroke(cr);
  const Rgb tc = lit ? skin.btnTextOn : skin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 4, 10.5, 1, tc.r,
       tc.g, tc.b);
}

void Panel::drawModelTab(cairo_t* cr, const Widget& w, bool lit,
                         int model) const {
  const Skin skin = skinFor(model);
  const Rgb base = lit ? skin.tabOn : skin.tabOff;
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, w.y, 0, w.y + w.h);
  const Rgb hi = mix(base, {1, 1, 1}, lit ? 0.14 : 0.06);
  const Rgb lo = mix(base, {0, 0, 0}, 0.18);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, lo.r, lo.g, lo.b);
  roundRect(cr, w.x, w.y, w.w, w.h, 4);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);
  setRgb(cr, {0, 0, 0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
  cairo_stroke(cr);
  const Rgb tc = lit ? skin.tabTextOn : skin.tabTextOff;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 5, 12, 1, tc.r, tc.g,
       tc.b);
}

void Panel::drawToggle(cairo_t* cr, const Widget& w, bool on,
                       int model) const {
  const Skin skin = skinFor(model);
  roundRect(cr, w.x - w.w * 0.5, w.y, w.w, w.h, 4);
  setRgb(cr, mix(skin.knobSkirt, {0, 0, 0}, 0.3));
  cairo_fill(cr);
  const double leverY = on ? w.y + 12 : w.y + w.h - 12;
  cairo_arc(cr, w.x, leverY, 8, 0, 2 * kPi);
  setRgb(cr, skin.knobCap);
  cairo_fill(cr);
  const char* top = w.buttonText.size() >= 1 ? w.buttonText[0] : w.label;
  const char* bottom = w.buttonText.size() >= 2 ? w.buttonText[1] : "";
  text(cr, top, w.x, w.y - 8, 10, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, bottom, w.x, w.y + w.h + 16, 10, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
}

void Panel::drawPill(cairo_t* cr, const Widget& w, bool on, int model) const {
  const Skin skin = skinFor(model);
  roundRect(cr, w.x, w.y, w.w, w.h, 5);
  setRgb(cr, on ? skin.btnOn : skin.btnOff);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, {0, 0, 0}, 0.35);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 5);
  cairo_stroke(cr);
  const Rgb tc = on ? skin.btnTextOn : skin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.5, 9.5, 1, tc.r,
       tc.g, tc.b);
}

void Panel::drawFootswitch(cairo_t* cr, const Widget& w, bool on,
                           int model) const {
  const Skin skin = skinFor(model);
  cairo_arc(cr, w.x, w.y, w.r + 4, 0, 2 * kPi);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.35));
  cairo_fill(cr);
  cairo_pattern_t* pat = cairo_pattern_create_radial(
      w.x - w.r * 0.3, w.y - w.r * 0.35, w.r * 0.1, w.x, w.y, w.r);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, 0.72, 0.74, 0.76);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.24, 0.25, 0.27);
  cairo_arc(cr, w.x, w.y, w.r, 0, 2 * kPi);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);
  cairo_arc(cr, w.x, w.y, w.r * 0.55, 0, 2 * kPi);
  setRgb(cr, {0.10, 0.10, 0.11});
  cairo_fill(cr);

  cairo_arc(cr, w.x, w.y - w.r - 12, 5, 0, 2 * kPi);
  setRgb(cr, on ? skin.accent : mix(skin.accent, {0, 0, 0}, 0.75));
  cairo_fill(cr);
  text(cr, w.label, w.x, w.y + w.r + 20, 10, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
}

void Panel::drawLevelMeter(cairo_t* cr, double x, double y, double w,
                           double h, double levelL, double levelR, int model,
                           bool labelsRight, double minDb, double maxDb) const {
  const Skin skin = skinFor(model);
  const double gap = 4.0;
  const double pairWidth = w * 2.0 + gap;
  for (int ch = 0; ch < 2; ++ch) {
    const double bx = x + ch * (w + gap);
    cairo_rectangle(cr, bx, y, w, h);
    cairo_set_source_rgb(cr, 0.045, 0.045, 0.05);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, bx + 0.5, y + 0.5, w - 1, h - 1);
    cairo_stroke(cr);

    const double v = clamp01(ch == 0 ? levelL : levelR);
    const double top = y + h * (1.0 - v);
    if (v > 0.0) {
      cairo_pattern_t* pat = cairo_pattern_create_linear(0, top, 0, y + h);
      cairo_pattern_add_color_stop_rgb(pat, 0.0, 0.92, 0.25, 0.20);
      cairo_pattern_add_color_stop_rgb(pat, 0.18, 0.92, 0.78, 0.26);
      cairo_pattern_add_color_stop_rgb(pat, 0.45, 0.30, 0.82, 0.42);
      cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.18, 0.62, 0.30);
      cairo_rectangle(cr, bx + 1, top, w - 2, y + h - top);
      cairo_set_source(cr, pat);
      cairo_fill(cr);
      cairo_pattern_destroy(pat);
    }
  }

  // Tick marks and dB labels sit on the inner side of each meter pair, as in
  // Comp Suite.
  const double tickX = labelsRight ? x + pairWidth : x;
  const double labelX = labelsRight ? tickX + 7.0 : tickX - 7.0;
  const int align = labelsRight ? 0 : 2;
  static const double kMarks[] = {3.0, 0.0, -6.0, -12.0,
                                  -24.0, -36.0, -48.0};
  for (double m : kMarks) {
    if (m < minDb - 0.01 || m > maxDb + 0.01) {
      continue;
    }
    const double ty = y + h * (1.0 - dbToNorm(m, minDb, maxDb));
    setRgb(cr, skin.legendDim, 0.55);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, tickX + (labelsRight ? -3.0 : 3.0), ty);
    cairo_line_to(cr, tickX, ty);
    cairo_stroke(cr);
    char buf[16];
    std::snprintf(buf, sizeof(buf), m > 0.0 ? "+%.0f" : "%.0f", m);
    text(cr, buf, labelX, ty + 3.0, 7.5, align, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }
  const char* label = labelsRight ? "IN" : "OUT";
  text(cr, label, x + w, y + h + 16, 9, 1, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
}

void Panel::drawLeds(cairo_t* cr, const PanelState& state, int model) const {
  (void)model;
  const double db = state.outPeakDb;
  // Twelve output segments from -30 dBFS to +3 dBFS.  The column echoes the
  // LED ladder in Comp Suite while showing amp output level.
  for (int i = 0; i < 12; ++i) {
    const double threshold = -30.0 + i * 3.0;
    const bool on = db >= threshold;
    Rgb c;
    if (i < 6) {
      c = {0.27, 0.88, 0.42};
    } else if (i < 9) {
      c = {0.96, 0.82, 0.29};
    } else {
      c = {1.0, 0.30, 0.24};
    }
    const double y = 146.0 + static_cast<double>(11 - i) * 16.0;
    roundRect(cr, 754, y, 16, 12, 2);
    if (on) {
      setRgb(cr, c);
    } else {
      setRgb(cr, {c.r * 0.18, c.g * 0.18, c.b * 0.18});
    }
    cairo_fill(cr);
  }
  text(cr, "OUT", 762, 344, 8, 1, 0.52, 0.54, 0.57);
}

bool Panel::importUserIr(Shared& shared, PanelState& state,
                         const std::string& path) const {
  std::error_code ec;
  const std::filesystem::path source(path);
  if (!std::filesystem::is_regular_file(source, ec)) {
    return false;
  }
  const std::string name = source.filename().string();
  if (!isWavName(name)) {
    return false;
  }
  const std::string dir = userIrDir();
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return false;
  }
  const std::filesystem::path target =
      std::filesystem::path(dir) / source.filename();
  bool same = false;
  if (std::filesystem::exists(target, ec)) {
    same = std::filesystem::equivalent(source, target, ec);
  }
  if (!same) {
    std::filesystem::copy_file(source, target,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
      return false;
    }
  }

  state.userIrs = listUserIrFiles(dir);
  int index = 0;
  for (size_t i = 0; i < state.userIrs.size(); ++i) {
    if (state.userIrs[i] == name) {
      index = static_cast<int>(i) + 1;
      break;
    }
  }
  if (index == 0) {
    return false;
  }
  const int model =
      std::min(std::max(static_cast<int>(shared.get(P_MODEL) + 0.5), 0), 3);
  shared.set(P_CAB, 1.0);
  shared.set(P_USER_IR_0 + model, static_cast<double>(index));
  markEdited(state);
  const int maxScroll =
      std::max(0, static_cast<int>(state.userIrs.size() + 1) - kUserRows);
  state.userIrScroll = std::min(
      std::max(index - kUserRows / 2, 0), maxScroll);
  return true;
}

void Panel::openFileBrowser(PanelState& state, bool nam) const {
  std::error_code ec;
  const std::string dir = nam ? userNamDir() : userIrDir();
  std::filesystem::create_directories(dir, ec);
  state.fileBrowserDir = dir;
  state.fileBrowserNam = nam;
  state.fileBrowserOpen = true;
  state.fileBrowserScroll = 0;
  refreshFileBrowser(state);
}

void Panel::refreshFileBrowser(PanelState& state) const {
  state.fileBrowserEntries.clear();
  std::error_code ec;
  const std::filesystem::path dir(state.fileBrowserDir);
  if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
    return;
  }
  const std::filesystem::path parent = dir.parent_path();
  if (!parent.empty() && parent != dir) {
    state.fileBrowserEntries.push_back({"..", true, true});
  }
  std::vector<FileEntry> dirs;
  std::vector<FileEntry> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    const std::string name = entry.path().filename().string();
    if (name.empty() || name[0] == '.') {
      continue;
    }
    if (entry.is_directory(ec)) {
      dirs.push_back({name, true, false});
    } else if (entry.is_regular_file(ec) &&
               (state.fileBrowserNam ? isNamName(name) : isWavName(name))) {
      files.push_back({name, false, false});
    }
  }
  auto byName = [](const FileEntry& a, const FileEntry& b) {
    return a.name < b.name;
  };
  std::sort(dirs.begin(), dirs.end(), byName);
  std::sort(files.begin(), files.end(), byName);
  state.fileBrowserEntries.insert(state.fileBrowserEntries.end(),
                                  dirs.begin(), dirs.end());
  state.fileBrowserEntries.insert(state.fileBrowserEntries.end(),
                                  files.begin(), files.end());
}

void Panel::drawFileBrowser(cairo_t* cr, const Shared& shared,
                            const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_paint(cr);

  roundRect(cr, kFbX, kFbY, kFbW, kFbH, 7);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.18));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.14);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kFbX + 0.5, kFbY + 0.5, kFbW - 1, kFbH - 1, 7);
  cairo_stroke(cr);

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
  cairo_rectangle(cr, kFbX + 1, kFbY + 1, kFbW - 2, 36);
  cairo_fill(cr);
  text(cr, state.fileBrowserNam ? "NAM MODELS" : "USER IR FILES",
       kFbX + 16, kFbY + 24, 10, 0, skin.btnText.r, skin.btnText.g,
       skin.btnText.b);
  text(cr, "CANCEL", kFbX + kFbW - 16, kFbY + 24, 10, 2, skin.accent.r,
       skin.accent.g, skin.accent.b);
  setRgb(cr, {1, 1, 1}, 0.10);
  cairo_move_to(cr, kFbX + 1, kFbY + 37.5);
  cairo_line_to(cr, kFbX + kFbW - 1, kFbY + 37.5);
  cairo_stroke(cr);

  const int count = static_cast<int>(state.fileBrowserEntries.size());
  if (count == 0) {
    text(cr, "No folders or WAV files here.", kFbX + 18, kFbY + 70, 11, 0,
         skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  }
  for (int row = 0; row < kFbRows; ++row) {
    const int i = state.fileBrowserScroll + row;
    if (i < 0 || i >= count) {
      break;
    }
    const FileEntry& e = state.fileBrowserEntries[static_cast<size_t>(i)];
    const double y = kFbRowY + row * kFbRowH;
    if ((i & 1) != 0) {
      cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.025);
      cairo_rectangle(cr, kFbX + 8, y, kFbW - 16, kFbRowH);
      cairo_fill(cr);
    }
    const char* kind = e.parent ? "UP" : (e.directory ? "DIR" : "WAV");
    text(cr, kind, kFbX + 18, y + 14, 8.5, 0, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
    text(cr, shorten(e.name, 72).c_str(), kFbX + 58, y + 14, 10.5, 0,
         skin.legend.r, skin.legend.g, skin.legend.b);
  }

  if (count > kFbRows) {
    const double trackY = kFbRowY;
    const double trackH = kFbRows * kFbRowH;
    const double thumbH = std::max(12.0, trackH * kFbRows / count);
    const int maxScroll = count - kFbRows;
    const double thumbY =
        trackY + (trackH - thumbH) *
                     (maxScroll > 0 ? state.fileBrowserScroll / double(maxScroll)
                                    : 0.0);
    cairo_rectangle(cr, kFbX + kFbW - 7, trackY, 2.0, trackH);
    setRgb(cr, {0, 0, 0}, 0.45);
    cairo_fill(cr);
    cairo_rectangle(cr, kFbX + kFbW - 7, thumbY, 2.0, thumbH);
    setRgb(cr, skin.accent, 0.8);
    cairo_fill(cr);
  }

  text(cr, state.fileBrowserNam
              ? "Click a folder to open it. Click a .nam or WAV to import it."
              : "Click a folder to open it. Click a WAV to import it.",
       kFbX + 18, kFbY + kFbH - 16, 9.5, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  cairo_restore(cr);
}

bool Panel::handleFileBrowserClick(Shared& shared, PanelState& state,
                                   double x, double y) {
  if (x < kFbX || x > kFbX + kFbW || y < kFbY || y > kFbY + kFbH) {
    state.fileBrowserOpen = false;
    return true;
  }
  if (y <= kFbY + 38 && x >= kFbX + kFbW - 80) {
    state.fileBrowserOpen = false;
    return true;
  }
  if (y < kFbRowY || y > kFbRowY + kFbRows * kFbRowH) {
    return true;
  }
  const int row = static_cast<int>((y - kFbRowY) / kFbRowH) +
                  state.fileBrowserScroll;
  const int count = static_cast<int>(state.fileBrowserEntries.size());
  if (row < 0 || row >= count) {
    return true;
  }
  const FileEntry entry = state.fileBrowserEntries[static_cast<size_t>(row)];
  const std::filesystem::path dir(state.fileBrowserDir);
  if (entry.parent) {
    const std::filesystem::path parent = dir.parent_path();
    if (!parent.empty() && parent != dir) {
      state.fileBrowserDir = parent.string();
      state.fileBrowserScroll = 0;
      refreshFileBrowser(state);
    }
    return true;
  }
  if (entry.directory) {
    state.fileBrowserDir = (dir / entry.name).string();
    state.fileBrowserScroll = 0;
    refreshFileBrowser(state);
    return true;
  }
  const std::string path = (dir / entry.name).string();
  const bool ok = state.fileBrowserNam ? importNamModel(shared, state, path)
                                       : importUserIr(shared, state, path);
  if (ok) {
    state.fileBrowserOpen = false;
  }
  return true;
}

bool Panel::importNamModel(Shared& shared, PanelState& state,
                           const std::string& path) const {
  std::error_code ec;
  const std::filesystem::path source(path);
  if (!std::filesystem::is_regular_file(source, ec) ||
      !isNamName(source.filename().string())) {
    return false;
  }
  const std::string dir = userNamDir();
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return false;
  }
  const std::filesystem::path target =
      std::filesystem::path(dir) / source.filename();
  bool same = false;
  if (std::filesystem::exists(target, ec)) {
    same = std::filesystem::equivalent(source, target, ec);
  }
  if (!same) {
    std::filesystem::copy_file(source, target,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
      return false;
    }
  }
  state.namModels = listNamFiles(dir);
  const std::string name = source.filename().string();
  int index = 0;
  for (size_t i = 0; i < state.namModels.size(); ++i) {
    if (state.namModels[i] == name) {
      index = static_cast<int>(i) + 1;
      break;
    }
  }
  if (index == 0) {
    return false;
  }
  shared.set(P_NAM_MODEL, static_cast<double>(index));
  shared.set(P_NAM_ON, 1.0);
  markEdited(state);
  const int maxScroll =
      std::max(0, static_cast<int>(state.namModels.size() + 1) - kUserRows);
  state.namScroll = std::min(std::max(index - kUserRows / 2, 0), maxScroll);
  return true;
}

bool Panel::importDroppedFile(Shared& shared, PanelState& state,
                              const std::string& path) const {
  std::string ext = std::filesystem::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (ext == ".nam") {
    return importNamModel(shared, state, path);
  }
  if (ext == ".wav") {
    if (state.page == 3) {
      return importNamModel(shared, state, path);
    }
    return importUserIr(shared, state, path);
  }
  return false;
}

void Panel::drawNamPage(cairo_t* cr, const Shared& shared,
                        const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  text(cr, "NAM MODEL", 520, 150, 18, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, "Neural Amp Modeler capture", 520, 169, 10.5, 1, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);

  roundRect(cr, kUserBoxX, kUserBoxY, kUserBoxW, kUserBoxH, 6);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.35));
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.10);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kUserBoxX + 0.5, kUserBoxY + 0.5, kUserBoxW - 1,
            kUserBoxH - 1, 6);
  cairo_stroke(cr);
  text(cr, "NAM MODELS", 76, kUserBoxY + 20, 11, 0, skin.legend.r,
       skin.legend.g, skin.legend.b);
  text(cr, userNamDir().c_str(), 76, kUserBoxY + 35, 8, 0, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  roundRect(cr, kUserBoxX + kUserBoxW - 94, kUserBoxY + 10, 82, 20, 3);
  setRgb(cr, mix(skin.btnOff, {1, 1, 1}, 0.06));
  cairo_fill(cr);
  setRgb(cr, {0, 0, 0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kUserBoxX + kUserBoxW - 93.5, kUserBoxY + 10.5, 81, 19, 3);
  cairo_stroke(cr);
  text(cr, "BROWSE", kUserBoxX + kUserBoxW - 53, kUserBoxY + 24, 9.5, 1,
       skin.btnText.r, skin.btnText.g, skin.btnText.b);

  const int selected =
      static_cast<int>(shared.get(P_NAM_MODEL) + 0.5);
  const int files = static_cast<int>(state.namModels.size());
  const int rows = std::min(kUserRows, files + 1);
  for (int row = 0; row < rows; ++row) {
    const int index = row + state.namScroll;
    if (index > files) {
      break;
    }
    const double ry = kUserRowY + row * kUserRowH;
    if (index == selected) {
      roundRect(cr, kUserBoxX + 4, ry + 1, kUserBoxW - 8, kUserRowH - 2, 3);
      setRgb(cr, skin.accent, 0.35);
      cairo_fill(cr);
    }
    char name[80];
    if (index == 0) {
      std::snprintf(name, sizeof(name), "None");
    } else {
      std::snprintf(name, sizeof(name), "%s",
                    shorten(state.namModels[static_cast<size_t>(index - 1)],
                            38)
                        .c_str());
    }
    text(cr, name, kUserBoxX + 12, ry + 14, 9.5, 0,
         index == selected ? skin.btnTextOn.r : skin.legend.r,
         index == selected ? skin.btnTextOn.g : skin.legend.g,
         index == selected ? skin.btnTextOn.b : skin.legend.b);
  }
  if (files == 0) {
    text(cr, "Drop .nam files in this folder.", 76, kUserRowY + 26, 9, 0,
         skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  }

  text(cr, "NAM replaces the preamp and power amp.", 900, 520, 10, 1,
       skin.legend.r, skin.legend.g, skin.legend.b);
  text(cr, "Comp, Drive, cab and output stay active.", 900, 538, 9.5, 1,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  text(cr, "Load a .nam model or NAM WAV.", 900, 556, 9.5, 1,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
}

void Panel::beginTextEntry(Shared& shared, PanelState& state, double x,
                           double y) {
  (void)shared;
  state.presetOpen = false;
  state.savePreset = false;
  state.renamePreset = false;
  state.deletePresetConfirm = false;
  state.presetText.clear();
  state.presetTextFresh = false;
  for (const Widget& w : widgets_) {
    if (!hitWidget(w, x, y)) {
      continue;
    }
    if (w.kind != Kind::Knob) {
      return;
    }
    editing_ = true;
    editParam_ = w.param;
    editText_.clear();
    editBoxX_ = std::clamp(x + 16.0, 4.0, kPanelWidth - kEditW - 4.0);
    editBoxY_ = y - kEditH - 14.0;
    if (editBoxY_ < 4.0) {
      editBoxY_ = std::min(y + 20.0, kPanelHeight - kEditH - 4.0);
    }
    return;
  }
}

bool Panel::hitTextEntry(double x, double y) const {
  return editing_ && x >= editBoxX_ && x <= editBoxX_ + kEditW &&
         y >= editBoxY_ && y <= editBoxY_ + kEditH;
}

bool Panel::textKey(Shared& shared, PanelState& state, int key, char ch) {
  if (state.savePreset || state.renamePreset) {
    if (key == kTextEnter) {
      return commitPresetName(shared, state);
    }
    if (key == kTextEscape) {
      state.savePreset = false;
      state.renamePreset = false;
      state.presetText.clear();
      state.presetTextFresh = false;
      state.presetOpen = false;
      return true;
    }
    if (key == kTextBackspace) {
      if (state.presetTextFresh) {
        state.presetText.clear();
        state.presetTextFresh = false;
      } else if (!state.presetText.empty()) {
        state.presetText.pop_back();
      }
      return true;
    }
    if (ch >= 32 && ch < 127) {
      if (state.presetTextFresh) {
        state.presetText.clear();
        state.presetTextFresh = false;
      }
      if (state.presetText.size() < 60) {
        state.presetText.push_back(ch);
      }
    }
    return true;
  }
  if (!editing_) {
    return false;
  }
  if (key == kTextEnter) {
    return commitTextEntry(shared, state);
  }
  if (key == kTextEscape) {
    editing_ = false;
    editText_.clear();
    return true;
  }
  if (key == kTextBackspace) {
    if (!editText_.empty()) {
      editText_.pop_back();
    }
    return true;
  }
  if (ch >= 32 && ch < 127 && editText_.size() < 24) {
    editText_.push_back(ch);
  }
  return true;
}

bool Panel::commitTextEntry(Shared& shared, PanelState& state) {
  if (!editing_) {
    return false;
  }
  if (editText_.empty()) {
    editing_ = false;
    return false;
  }
  double value = 0.0;
  if (!parseParamValue(shared, editParam_, editText_, &value)) {
    return false;
  }
  shared.set(editParam_, value);
  markEdited(state);
  editing_ = false;
  editText_.clear();
  return true;
}

void Panel::drawTextEntry(cairo_t* cr, const Shared& shared, int model) const {
  if (!editing_) {
    return;
  }
  const Skin skin = skinFor(model);
  const ParamSpec& spec = kParamSpecs[editParam_];
  const std::string current =
      formatEditValue(shared, editParam_, shared.get(editParam_));
  const std::string shown = (editText_.empty() ? current : editText_) + "|";
  roundRect(cr, editBoxX_, editBoxY_, kEditW, kEditH, 4);
  cairo_set_source_rgba(cr, 0.035, 0.04, 0.05, 0.98);
  cairo_fill(cr);
  setRgb(cr, skin.tabOn, 0.9);
  cairo_set_line_width(cr, 1.2);
  roundRect(cr, editBoxX_ + 0.5, editBoxY_ + 0.5, kEditW - 1, kEditH - 1, 4);
  cairo_stroke(cr);
  text(cr, spec.name, editBoxX_ + 8, editBoxY_ + 12, 8.0, 0,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  const Rgb gold = {0.98, 0.80, 0.32};
  text(cr, shown.c_str(), editBoxX_ + 8, editBoxY_ + 26, 10.5, 0, gold.r,
       gold.g, gold.b);
}

std::vector<Panel::WidgetRect> Panel::widgetRects() const {
  std::vector<WidgetRect> out;
  out.reserve(widgets_.size());
  for (const Widget& w : widgets_) {
    WidgetRect r;
    r.param = w.param;
    r.label = w.label;
    r.knob = w.kind == Kind::Knob;
    r.toggle = w.kind == Kind::Toggle || w.kind == Kind::Footswitch;
    r.r = w.r;
    if (w.kind == Kind::Knob) {
      r.x = w.x - w.r;
      r.y = w.y - w.r;
      r.w = w.r * 2.0;
      r.h = w.r * 2.0;
    } else if (w.kind == Kind::Toggle) {
      r.x = w.x - w.w * 0.5;
      r.y = w.y;
      r.w = w.w;
      r.h = w.h;
    } else {
      r.x = w.x;
      r.y = w.y;
      r.w = w.w;
      r.h = w.h;
    }
    out.push_back(r);
  }
  return out;
}

} // namespace bass
