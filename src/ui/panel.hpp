// Bass Suite — panel.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cairo.h>

#include <string>
#include <vector>

#include "params.hpp"

namespace bass {

constexpr int kPanelWidth = 1040;
constexpr int kPanelHeight = 640;

// One row in the built-in IR file browser.
struct FileEntry {
  std::string name;
  bool directory = false;
  bool parent = false;
};

// UI-only state.  The page is not a host parameter and is not saved.
struct PanelState {
  int page = 0;  // 0 amp, 1 pedals, 2 cab, 3 NAM, 4 output
  int presetIndex = 0;
  bool presetDirty = false;
  bool presetOpen = false;
  int presetScroll = 0;
  float inLevel = 0.0f;
  float outLevel = 0.0f;
  float inLevelL = 0.0f;
  float inLevelR = 0.0f;
  float outLevelL = 0.0f;
  float outLevelR = 0.0f;
  float outPeakDb = -120.0f;
  uint32_t lastMeterSerial = 0;
  int meterIdleTicks = 0;
  std::vector<std::string> userIrs;
  int userIrScroll = 0;
  int userIrScanTicks = 60;

  bool fileBrowserOpen = false;
  bool fileBrowserNam = false;
  std::string fileBrowserDir;
  std::vector<FileEntry> fileBrowserEntries;
  int fileBrowserScroll = 0;

  std::vector<std::string> namModels;
  int namScroll = 0;
  int namScanTicks = 60;
};

class Panel {
public:
  Panel();
  ~Panel();
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;

  void loadFont(const std::string& dir);
  void tick(const Shared& shared, PanelState& state) const;
  void draw(cairo_t* cr, Shared& shared, const PanelState& state);

  bool pointerDown(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  bool pointerMove(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  void pointerUp();
  bool handlePresetClick(Shared& shared, PanelState& state, double x, double y);
  void applyPreset(Shared& shared, PanelState& state, int index) const;
  void drawPresetBar(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawPresetList(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const;
  void markEdited(PanelState& state) const { state.presetDirty = true; }
  bool wheel(Shared& shared, PanelState& state, double x, double y, double dy);
  void beginTextEntry(Shared& shared, PanelState& state, double x, double y);
  static constexpr int kTextEnter = 1;
  static constexpr int kTextEscape = 2;
  static constexpr int kTextBackspace = 3;
  bool textKey(Shared& shared, PanelState& state, int key, char ch);

  // Copies a WAV into the user IR folder and selects it for the current amp.
  bool importUserIr(Shared& shared, PanelState& state,
                    const std::string& path) const;
  // Copies a .nam/.wav model into the NAM folder and selects it.
  bool importNamModel(Shared& shared, PanelState& state,
                      const std::string& path) const;
  // Routes a drag-and-dropped file by extension/current page.
  bool importDroppedFile(Shared& shared, PanelState& state,
                         const std::string& path) const;

  bool dragging() const { return dragParam_ >= 0; }

  struct WidgetRect {
    uint32_t param = 0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    const char* label = "";
    bool knob = false;
    bool toggle = false;
  };
  std::vector<WidgetRect> widgetRects() const;

private:
  enum class Kind {
    Knob,
    Button,
    Toggle,
    ModelTab,
    Power,
    PagePill,
    Footswitch,
    PresetBar,
    Pill,
  };

  struct Widget {
    Kind kind;
    uint32_t param = 0;
    double value = 0.0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    const char* label = "";
    std::vector<const char*> scale;
    bool labelBelow = false;
    std::vector<const char*> buttonText;
    int steps = 0;
    double scaleRadius = 1.58;
  };

  void layout(const Shared& shared, const PanelState& state);
  void drawHeader(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawTabs(cairo_t* cr, const Shared& shared) const;
  void drawAmpPage(cairo_t* cr, const Shared& shared,
                   const PanelState& state) const;
  void drawPedalPage(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawCabPage(cairo_t* cr, const Shared& shared,
                   const PanelState& state) const;
  void drawOutPage(cairo_t* cr, const Shared& shared,
                   const PanelState& state) const;
  void drawFooter(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                  const PanelState& state, int model) const;
  void drawKnob(cairo_t* cr, const Widget& w, double value, int model) const;
  void drawButton(cairo_t* cr, const Widget& w, bool lit, int model) const;
  void drawModelTab(cairo_t* cr, const Widget& w, bool lit, int model) const;
  void drawToggle(cairo_t* cr, const Widget& w, bool on, int model) const;
  void drawPill(cairo_t* cr, const Widget& w, bool on, int model) const;
  void drawFootswitch(cairo_t* cr, const Widget& w, bool on, int model) const;
  void drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                      double levelL, double levelR, int model,
                      bool labelsRight, double minDb, double maxDb) const;
  void drawLeds(cairo_t* cr, const PanelState& state, int model) const;
  void drawTextEntry(cairo_t* cr, const Shared& shared, int model) const;
  bool commitTextEntry(Shared& shared, PanelState& state);
  bool hitTextEntry(double x, double y) const;
  void drawCabWindow(cairo_t* cr, const Shared& shared,
                     const PanelState& state, int model) const;
  void openFileBrowser(PanelState& state, bool nam = false) const;
  void refreshFileBrowser(PanelState& state) const;
  void drawFileBrowser(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const;
  void drawNamPage(cairo_t* cr, const Shared& shared,
                   const PanelState& state) const;
  void drawTunerPage(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  bool handleFileBrowserClick(Shared& shared, PanelState& state, double x,
                              double y);
  void text(cairo_t* cr, const char* s, double x, double y, double size,
            int align, double r, double g, double b) const;

  double knobValue(const Shared& shared, const Widget& w) const;
  bool isLit(const Shared& shared, const PanelState& state,
             const Widget& w) const;
  bool hitWidget(const Widget& w, double x, double y) const;

  std::vector<Widget> widgets_;
  int dragParam_ = -1;
  double dragStartY_ = 0.0;
  double dragStartValue_ = 0.0;

  int lastClickParam_ = -1;
  double lastClickTime_ = 0.0;
  static constexpr double kDoubleClickMs = 400.0;

  bool editing_ = false;
  uint32_t editParam_ = 0;
  std::string editText_;
  double editBoxX_ = 0.0;
  double editBoxY_ = 0.0;
  static constexpr double kEditW = 150.0;
  static constexpr double kEditH = 32.0;

  cairo_surface_t* grain_ = nullptr;
  void* ft_ = nullptr;
  void* ftFace_ = nullptr;
  cairo_font_face_t* font_ = nullptr;
  cairo_font_face_t* fallbackFont_ = nullptr;
};

} // namespace bass
