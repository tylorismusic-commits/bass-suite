// Bass Suite — panel render smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/panel.hpp"

#include <cairo.h>
#include <cmath>

int main() {
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

  for (int page = 0; page < 4; ++page) {
    state.page = page;
    panel.tick(shared, state);
    panel.draw(cr, shared, state);
  }

  // Every model, every page, must draw without a cairo error.
  for (int model = 0; model < 7; ++model) {
    shared.set(bass::P_MODEL, static_cast<double>(model));
    for (int page = 0; page < 4; ++page) {
      state.page = page;
      panel.draw(cr, shared, state);
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return 0;
}
