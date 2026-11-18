// Bass Suite — render every faceplate/page to PNG without a host
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   bass_ui_preview <font-dir> <out-dir> [scale]

#include "ui/panel.hpp"
#include "presets.hpp"

#include <cairo.h>
#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  const std::string fontDir =
      argc > 1 ? argv[1] : "assets/fonts";
  const std::string outDir =
      argc > 2 ? argv[2] : "/tmp/basssuite-preview";
  const double scale = argc > 3 ? std::atof(argv[3]) : 1.0;
  const int cabSet = argc > 4 ? std::atoi(argv[4]) : 0;
  if (scale < 0.5 || scale > 2.5) {
    std::fprintf(stderr, "scale must be 0.5..2.5\n");
    return 2;
  }
  mkdir(outDir.c_str(), 0755);

  bass::Panel panel;
  panel.loadFont(fontDir);
  bass::Shared shared;
  bass::applyStartupPreset(shared);
  if (cabSet > 0) {
    shared.set(bass::P_CAB, 1.0);
    shared.set(bass::P_CAB_SET, static_cast<double>(cabSet));
  }
  bass::PanelState state;

  const int w = static_cast<int>(bass::kPanelWidth * scale);
  const int h = static_cast<int>(bass::kPanelHeight * scale);
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t* cr = cairo_create(surface);

  for (int model = 0; model < 4; ++model) {
    shared.set(bass::P_MODEL, static_cast<double>(model));
    for (int page = 0; page < 6; ++page) {
      state.page = page;
      if (page == 5) {
        shared.tunerNote.store(33);
        shared.tunerHz.store(55.0f);
        shared.tunerCents.store(4.1f);
        shared.tunerConfidence.store(0.90f);
      }
      panel.tick(shared, state);
      cairo_save(cr);
      cairo_scale(cr, scale, scale);
      panel.draw(cr, shared, state);
      cairo_restore(cr);
      char path[512];
      std::snprintf(path, sizeof(path), "%s/amp%d_page%d.png", outDir.c_str(),
                    model, page);
      cairo_surface_write_to_png(surface, path);
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  std::printf("bass_ui_preview: wrote %s\n", outDir.c_str());
  return 0;
}
