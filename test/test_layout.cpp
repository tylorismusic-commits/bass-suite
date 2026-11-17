// Bass Suite — panel layout and page smoke tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <vector>

#include <cairo.h>

#include "params.hpp"
#include "ui/panel.hpp"

namespace {

constexpr double kPlateX = 16.0;
constexpr double kPlateY = 122.0;
constexpr double kPlateW = 1008.0;
constexpr double kPlateH = 482.0;

bool hasParam(const std::vector<bass::Panel::WidgetRect>& rects, uint32_t p) {
  for (const auto& r : rects) {
    if (r.param == p) return true;
  }
  return false;
}

int countParam(const std::vector<bass::Panel::WidgetRect>& rects, uint32_t p) {
  int n = 0;
  for (const auto& r : rects) {
    if (r.param == p) ++n;
  }
  return n;
}

bool overlaps(const bass::Panel::WidgetRect& a,
              const bass::Panel::WidgetRect& b) {
  const double ax2 = a.x + a.w, ay2 = a.y + a.h;
  const double bx2 = b.x + b.w, by2 = b.y + b.h;
  return a.x < bx2 - 0.5 && b.x < ax2 - 0.5 && a.y < by2 - 0.5 &&
         b.y < ay2 - 0.5;
}

} // namespace

int main() {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bass::kPanelWidth,
                                 bass::kPanelHeight);
  cairo_t* cr = cairo_create(surface);
  bass::Panel panel;
  bass::Shared shared;
  bass::PanelState state;

  if (std::fabs(shared.get(bass::P_MODEL) - 2.0) > 0.01) {
    std::fprintf(stderr, "startup model is not SVT\n");
    return 1;
  }
  if (shared.get(bass::P_OD_ON) > 0.5) {
    std::fprintf(stderr, "drive should start off\n");
    return 1;
  }

  for (int page = 0; page <= 4; ++page) {
    state.page = page;
    panel.draw(cr, shared, state);
    const auto rects = panel.widgetRects();
    for (const auto& r : rects) {
      if (r.w <= 0.0 || r.h <= 0.0 || r.y < kPlateY) continue;
      if (r.x < kPlateX - 0.5 || r.y < kPlateY - 0.5 ||
          r.x + r.w > kPlateX + kPlateW + 0.5 ||
          r.y + r.h > kPlateY + kPlateH + 0.5) {
        std::fprintf(stderr, "page %d: '%s' is off the plate\n", page, r.label);
        return 1;
      }
    }
    for (size_t i = 0; i < rects.size(); ++i) {
      for (size_t j = i + 1; j < rects.size(); ++j) {
        if (!overlaps(rects[i], rects[j])) continue;
        std::fprintf(stderr, "page %d: '%s' overlaps '%s'\n", page,
                     rects[i].label, rects[j].label);
        return 1;
      }
    }
  }

  state.page = 0;
  panel.draw(cr, shared, state);
  auto rects = panel.widgetRects();
  if (countParam(rects, bass::P_MODEL) != 4) {
    std::fprintf(stderr, "expected 4 model tabs\n");
    return 1;
  }
  if (!hasParam(rects, bass::P_GAIN) || !hasParam(rects, bass::P_BASS) ||
      !hasParam(rects, bass::P_MASTER)) {
    std::fprintf(stderr, "amp page is missing core knobs\n");
    return 1;
  }

  state.page = 1;
  panel.draw(cr, shared, state);
  rects = panel.widgetRects();
  if (!hasParam(rects, bass::P_COMP_AMOUNT) ||
      !hasParam(rects, bass::P_COMP_MIX) ||
      !hasParam(rects, bass::P_OCT_LEVEL) ||
      !hasParam(rects, bass::P_OD_DRIVE) || !hasParam(rects, bass::P_OD_MIX)) {
    std::fprintf(stderr, "pedal page is missing Comp/Octave/Drive controls\n");
    return 1;
  }

  state.page = 2;
  panel.draw(cr, shared, state);
  rects = panel.widgetRects();
  if (countParam(rects, bass::P_CAB_SET) != 6 || !hasParam(rects, bass::P_CAB_MIC) ||
      !hasParam(rects, bass::P_CAB_LEVEL)) {
    std::fprintf(stderr, "cab page is missing controls\n");
    return 1;
  }

  state.page = 3;
  panel.draw(cr, shared, state);
  rects = panel.widgetRects();
  if (!hasParam(rects, bass::P_NAM_ON) || !hasParam(rects, bass::P_NAM_LEVEL)) {
    std::fprintf(stderr, "NAM page is missing controls\n");
    return 1;
  }

  state.page = 4;
  panel.draw(cr, shared, state);
  rects = panel.widgetRects();
  if (!hasParam(rects, bass::P_OUT_EQ_LOW) ||
      !hasParam(rects, bass::P_OUT_EQ_HIGH) ||
      !hasParam(rects, bass::P_OUT_HPF) ||
      !hasParam(rects, bass::P_OUT_LPF) ||
      !hasParam(rects, bass::P_OUTPUT) ||
      !hasParam(rects, bass::P_LIMITER_THRESH)) {
    std::fprintf(stderr, "OUT page is missing controls\n");
    return 1;
  }

  // A factory-cab row selects that cab and forces the factory path.
  state.page = 2;
  shared.set(bass::P_CAB, 1.0);
  shared.set(bass::P_CAB_SET, 0.0);
  panel.draw(cr, shared, state);
  panel.pointerDown(shared, state, 620.0, 389.0, false);
  panel.pointerUp();
  if (shared.get(bass::P_CAB) > 0.5 ||
      std::fabs(shared.get(bass::P_CAB_SET) - 2.0) > 0.01) {
    std::fprintf(stderr, "cab row did not select 2x12: cab %.1f set %.1f\n",
                 shared.get(bass::P_CAB), shared.get(bass::P_CAB_SET));
    return 1;
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  std::printf("layout ok\n");
  return 0;
}
