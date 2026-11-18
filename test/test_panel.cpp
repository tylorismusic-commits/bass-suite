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
  std::filesystem::create_directories(config / "home");
  setenv("XDG_CONFIG_HOME", config.string().c_str(), 1);
  setenv("HOME", (config / "home").string().c_str(), 1);

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

  // User presets: save, duplicate, recall, dirty tracking, conflict
  // feedback, rename, export, import, rescan and delete.
  {
    const uint32_t saved = bass::P_GAIN;
    auto indexOf = [&](const char* name) {
      for (int i = 0; i < panel.presetCount(); ++i) {
        if (std::string(panel.presetName(i)) == name) return i;
      }
      return -1;
    };
    shared.set(saved, 0.17);
    state.page = 0;
    state.presetOpen = false;

    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 117, 295, false);  // SAVE
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

    // Duplicate the selected user preset.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 208, 295, false);  // DUPLICATE
    panel.pointerUp();
    if (panel.presetCount() != bass::kPresetCount + 2 ||
        indexOf("Test Bass copy") < 0 ||
        state.presetIndex != indexOf("Test Bass copy")) {
      std::fprintf(stderr, "DUPLICATE did not create a copy\n");
      return 1;
    }
    if (!std::filesystem::exists(bass::userPresetDir() /
                                 "Test Bass copy.bass-suite")) {
      std::fprintf(stderr, "duplicated preset file is missing\n");
      return 1;
    }

    // Delete the duplicate.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 198, 325, false);  // DELETE
    panel.pointerUp();
    if (!state.deletePresetConfirm) {
      std::fprintf(stderr, "DELETE did not ask for confirmation\n");
      return 1;
    }
    panel.pointerDown(shared, state, 198, 325, false);  // CONFIRM
    panel.pointerUp();
    if (panel.presetCount() != bass::kPresetCount + 1 ||
        indexOf("Test Bass copy") >= 0) {
      std::fprintf(stderr, "DELETE did not remove the duplicate\n");
      return 1;
    }

    // Recall the user preset and verify dirty tracking against host changes.
    panel.applyPreset(shared, state, indexOf("Test Bass"));
    if (std::fabs(shared.get(saved) - 0.17) > 1e-6) {
      std::fprintf(stderr, "user preset did not recall its value\n");
      return 1;
    }
    shared.set(saved, 0.91);
    panel.tick(shared, state);
    if (!state.presetDirty) {
      std::fprintf(stderr, "host change did not mark the preset dirty\n");
      return 1;
    }
    shared.set(saved, 0.17);
    panel.tick(shared, state);
    if (state.presetDirty) {
      std::fprintf(stderr, "restoring the value did not clear dirty\n");
      return 1;
    }

    // Rename to an existing name must be rejected with inline feedback.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 117, 295, false);  // SAVE another
    panel.pointerUp();
    for (const char* c = "Other Bass"; *c; ++c) {
      panel.textKey(shared, state, 0, *c);
    }
    panel.textKey(shared, state, bass::Panel::kTextEnter, 0);
    const int other = indexOf("Other Bass");
    if (other < 0) {
      std::fprintf(stderr, "second preset was not saved\n");
      return 1;
    }
    panel.applyPreset(shared, state, other);
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 118, 325, false);  // RENAME
    panel.pointerUp();
    if (!state.renamePreset) {
      std::fprintf(stderr, "RENAME did not open the preset name field\n");
      return 1;
    }
    for (const char* c = "Test Bass"; *c; ++c) {
      panel.textKey(shared, state, 0, *c);
    }
    panel.textKey(shared, state, bass::Panel::kTextEnter, 0);
    if (state.presetError.empty() ||
        indexOf("Other Bass") < 0 || indexOf("Test Bass") < 0) {
      std::fprintf(stderr, "rename conflict was not reported\n");
      return 1;
    }

    // Rename to a free name.
    state.presetText = "Renamed Bass";
    panel.textKey(shared, state, bass::Panel::kTextEnter, 0);
    if (indexOf("Renamed Bass") < 0 || indexOf("Other Bass") >= 0) {
      std::fprintf(stderr, "renaming did not move the preset\n");
      return 1;
    }

    // Export the selected preset to the documents folder.
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    panel.pointerDown(shared, state, 286, 325, false);  // EXPORT
    panel.pointerUp();
    if (!std::filesystem::exists(bass::userPresetExportDir() /
                                 "Renamed Bass.bass-suite")) {
      std::fprintf(stderr, "EXPORT did not write the preset file\n");
      return 1;
    }

    // Drag-and-drop import of a preset file, renamed to avoid a collision.
    const std::filesystem::path external = config / "external";
    std::filesystem::create_directories(external);
    std::vector<double> imported;
    for (uint32_t i = 0; i < bass::P_COUNT; ++i) {
      imported.push_back(shared.get(i));
    }
    imported[bass::P_GAIN] = 0.44;
    if (!bass::writeUserPreset(external, "Test Bass", imported)) {
      std::fprintf(stderr, "could not write the import fixture\n");
      return 1;
    }
    if (!panel.importDroppedFile(
            shared, state, (external / "Test Bass.bass-suite").string())) {
      std::fprintf(stderr, "dropped preset was not imported\n");
      return 1;
    }
    const int importedIndex = indexOf("Test Bass (2)");
    if (importedIndex < 0 || std::fabs(shared.get(saved) - 0.44) > 1e-6) {
      std::fprintf(stderr, "import did not load the dropped preset\n");
      return 1;
    }
    if (!std::filesystem::exists(bass::userPresetDir() /
                                 "Test Bass (2).bass-suite")) {
      std::fprintf(stderr, "imported preset file is missing\n");
      return 1;
    }

    // A preset written by another instance appears when the list is opened.
    const int beforeRescan = panel.presetCount();
    bass::saveUserPreset(bass::userPresetDir(), "External Bass", shared);
    state.presetOpen = false;
    panel.pointerDown(shared, state, 200, 32, false);
    panel.pointerUp();
    if (panel.presetCount() != beforeRescan + 1 ||
        indexOf("External Bass") < 0) {
      std::fprintf(stderr, "opening the list did not rescan presets\n");
      return 1;
    }
    state.presetOpen = false;

    {
      bass::Panel reloaded;
      if (reloaded.presetCount() != panel.presetCount()) {
        std::fprintf(stderr, "user presets did not reload from disk\n");
        return 1;
      }
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return 0;
}
