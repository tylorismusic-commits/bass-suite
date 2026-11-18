// Bass Suite — panel render smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/panel.hpp"

#include <cairo.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "params.hpp"
#include "presets.hpp"
#include "user_presets.hpp"

int main() {
  const std::filesystem::path config =
      "/tmp/opencode/bass-suite-test-config";
  std::filesystem::remove_all(config);
  setenv("XDG_CONFIG_HOME", config.string().c_str(), 1);

  bass::Panel panel;
  bass::Shared shared;
  bass::PanelState state;

  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bass::kPanelWidth,
                                 bass::kPanelHeight);
  cairo_t* cr = cairo_create(surface);

  // A stopped process stream must decay the meters instead of freezing them
  // (this is what happens when the host bypasses the plug-in).
  shared.inPeakLDb.store(0.0f);
  shared.inPeakRDb.store(0.0f);
  shared.outPeakLDb.store(0.0f);
  shared.outPeakRDb.store(0.0f);
  shared.outPeakDb.store(0.0f);
  shared.meterSerial.store(1);
  panel.tick(shared, state);
  if (!(state.inLevelL > 0.1f)) {
    std::fprintf(stderr, "meter did not respond to a fresh process block\n");
    return 1;
  }
  for (int i = 0; i < 80; ++i) {
    panel.tick(shared, state);
  }
  if (state.inLevelL > 0.05f || state.outLevelL > 0.05f ||
      state.outPeakDb > -100.0f) {
    std::fprintf(stderr, "meters did not decay after process stopped\n");
    return 1;
  }

  for (int page = 0; page < 6; ++page) {
    state.page = page;
    panel.tick(shared, state);
    panel.draw(cr, shared, state);
  }

  // Every model, every page, must draw without a cairo error.
  for (int model = 0; model < 4; ++model) {
    shared.set(bass::P_MODEL, static_cast<double>(model));
    for (int page = 0; page < 6; ++page) {
      state.page = page;
      panel.draw(cr, shared, state);
    }
  }

  // User presets: save the current bass, recall it, rename it, delete it.
  {
    const uint32_t saved = bass::P_GAIN;
    shared.set(saved, 0.23);
    state.page = 0;
    state.presetOpen = false;

    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 113, 295, false);  // SAVE
    panel.pointerUp();
    if (!state.savePreset) {
      std::fprintf(stderr, "SAVE did not open the preset name field\n");
      return 1;
    }
    for (const char* c = "Test Bass"; *c; ++c) {
      panel.textKey(shared, state, 0, *c);
    }
    panel.textKey(shared, state, bass::Panel::kTextEnter, 0);
    if (panel.presetCount() != bass::kPresetCount + 1 ||
        std::string(panel.presetName(bass::kPresetCount)) != "Test Bass") {
      std::fprintf(stderr, "saving did not add a named user preset\n");
      return 1;
    }
    if (!std::filesystem::exists(bass::userPresetDir() /
                                 "Test Bass.bass-suite")) {
      std::fprintf(stderr, "saved preset file is missing\n");
      return 1;
    }
    shared.set(saved, 0.91);
    panel.applyPreset(shared, state, bass::kPresetCount);
    if (std::fabs(shared.get(saved) - 0.23) > 1e-6) {
      std::fprintf(stderr, "user preset did not recall its value\n");
      return 1;
    }

    // The browser lists user presets after the factory bank.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    state.presetScroll = bass::kPresetCount + 1 - 10;
    const int row = bass::kPresetCount - state.presetScroll;
    shared.set(saved, 0.99);
    panel.pointerDown(shared, state, 200, 54.0 + 5.0 + row * 22.0 + 11.0,
                      false);
    panel.pointerUp();
    if (state.presetIndex != bass::kPresetCount ||
        std::fabs(shared.get(saved) - 0.23) > 1e-6) {
      std::fprintf(stderr, "preset browser did not load the user preset\n");
      return 1;
    }

    // Rename the selected user preset.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 188, 295, false);  // RENAME
    panel.pointerUp();
    if (!state.renamePreset) {
      std::fprintf(stderr, "RENAME did not open the preset name field\n");
      return 1;
    }
    for (const char* c = "Renamed Bass"; *c; ++c) {
      panel.textKey(shared, state, 0, *c);
    }
    panel.textKey(shared, state, bass::Panel::kTextEnter, 0);
    if (std::string(panel.presetName(bass::kPresetCount)) !=
            "Renamed Bass" ||
        !std::filesystem::exists(bass::userPresetDir() /
                                 "Renamed Bass.bass-suite") ||
        std::filesystem::exists(bass::userPresetDir() /
                                "Test Bass.bass-suite")) {
      std::fprintf(stderr, "renaming did not move the preset file\n");
      return 1;
    }

    {
      bass::Panel reloaded;
      if (reloaded.presetCount() != bass::kPresetCount + 1 ||
          std::string(reloaded.presetName(bass::kPresetCount)) !=
              "Renamed Bass") {
        std::fprintf(stderr, "user presets did not reload from disk\n");
        return 1;
      }
    }

    // Delete asks for confirmation, then removes the file.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 268, 295, false);
    panel.pointerUp();
    if (!state.deletePresetConfirm) {
      std::fprintf(stderr, "DELETE did not ask for confirmation\n");
      return 1;
    }
    panel.pointerDown(shared, state, 268, 295, false);
    panel.pointerUp();
    if (panel.presetCount() != bass::kPresetCount ||
        std::filesystem::exists(bass::userPresetDir() /
                                "Renamed Bass.bass-suite")) {
      std::fprintf(stderr, "DELETE did not remove the file\n");
      return 1;
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return 0;
}
