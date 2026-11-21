// Bass Suite — CLAP plugin
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/all.h>

#ifndef SUITE_MACOS_COCOA
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>
#endif
#include <dlfcn.h>
#include <sys/time.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <cstdarg>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "dsp/crossfade_engine.hpp"
#include "dsp/common.hpp"
#include "dsp/nam_library.hpp"
#include "dsp/user_ir.hpp"
#include "params.hpp"
#include "presets.hpp"
#include "version.hpp"
#include "ui/geometry.hpp"
#include "ui/panel.hpp"
#ifndef SUITE_MACOS_COCOA
#include "ui/xdnd.hpp"
#else
#include <memory>

#include "ui/cocoa_view.hpp"
#endif

namespace bass {
namespace {

// Set BASS_DEBUG=1 to trace the editor thread to /tmp/basssuite-ui.log.
bool debugOn() {
  static const bool on = std::getenv("BASS_DEBUG") != nullptr;
  return on;
}

void dbg(const char* fmt, ...) {
  if (!debugOn()) {
    return;
  }
  FILE* f = std::fopen("/tmp/basssuite-ui.log", "a");
  if (!f) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

constexpr const char* kPluginId = "com.tylorismusic.basssuite";
constexpr const char* kPluginName = "Bass Suite";
constexpr const char* kVendor = "tylorismusic";

std::string bundleResourceDir() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&bundleResourceDir), &info) && info.dli_fname) {
    std::string path = info.dli_fname;
    const size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
      path = path.substr(0, slash);
    }
#ifdef SUITE_MACOS_COCOA
    // Resources live in Contents/Resources, next to Contents/MacOS.
    return path + "/../Resources/BassSuite.resources";
#else
    return path + "/BassSuite.resources";
#endif
  }
  return "BassSuite.resources";
}

// ---------------------------------------------------------------------------
// macOS editor: the panel lives in a Cocoa NSView created by the host.
// ---------------------------------------------------------------------------

#ifdef SUITE_MACOS_COCOA
class Editor {
public:
  explicit Editor(Shared& shared) : shared_(shared) {}

  // Called when the user changes a control, so the host can ask for a
  // process call and pick up the new values.
  std::function<void()> requestFlush;

  double scale() const { return scale_.load(std::memory_order_relaxed); }
  UiSize size() const { return sizeForScale(scale()); }

  bool open(void* parent) {
    if (!cocoa_) {
      panel_.loadFont(bundleResourceDir());
      cocoa_ = std::make_unique<CocoaView>();
      cocoa_->setContent(&panel_, &shared_, &state_);
    }
    if (parent) cocoa_->attach(parent);
    return true;
  }

  void close() { cocoa_.reset(); }

  void resize(double scale) {
    scale_.store(clampScale(scale), std::memory_order_relaxed);
    if (cocoa_) {
      const UiSize s = size();
      cocoa_->resize(static_cast<int>(s.width), static_cast<int>(s.height));
    }
  }

  ~Editor() { close(); }

private:
  Shared& shared_;
  PanelState state_;
  Panel panel_;
  std::atomic<double> scale_{1.0};
  std::unique_ptr<CocoaView> cocoa_;
};
#else
// ---------------------------------------------------------------------------
// X11 editor.  Owns the window and the Cairo surface, and runs its own
// thread so the host's audio and main threads are never blocked on drawing.
// ---------------------------------------------------------------------------

class Editor {
public:
  explicit Editor(Shared& shared) : shared_(shared) {}

  // Called when the user changes a control, so the host can ask for a
  // process call and pick up the new values.
  std::function<void()> requestFlush;

  double scale() const { return scale_.load(std::memory_order_relaxed); }
  UiSize size() const { return sizeForScale(scale()); }

  // Host thread: record the request only.  All Xlib calls happen on the
  // editor thread, because Xlib is not thread-safe and calling
  // XInitThreads() after the host has used Xlib is undefined behaviour
  // (that made the editor fail to load in REAPER).
  void resize(double scale) {
    scale_.store(clampScale(scale), std::memory_order_relaxed);
    resizePending_.store(true, std::memory_order_relaxed);
  }

  bool open(void* parent) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (thread_.joinable()) {
      return opened_;
    }
    parentWindow_ =
        parent ? static_cast<Window>(reinterpret_cast<uintptr_t>(parent)) : 0;
    opened_ = false;
    openFailed_ = false;
    quitting_ = false;
    thread_ = std::thread([this] { run(); });
    cv_.wait_for(lock, std::chrono::seconds(3),
                 [this] { return opened_ || openFailed_; });
    return opened_;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      quitting_ = true;
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    opened_ = false;
  }

  ~Editor() { close(); }

private:
  void run() {
    dbg("thread start");
    Display* dpy = XOpenDisplay(nullptr);
    dbg("display=%p", static_cast<void*>(dpy));
    if (!dpy) {
      std::lock_guard<std::mutex> lock(mutex_);
      openFailed_ = true;
      cv_.notify_all();
      return;
    }
    const int screen = DefaultScreen(dpy);
    const Window parent = parentWindow_ ? parentWindow_ : RootWindow(dpy, screen);
    UiSize current = size();

    XSetWindowAttributes attrs{};
    // Button1MotionMask is essential: PointerMotionMask only reports motion
    // with no button held, so dragging a knob would never deliver events.
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | Button1MotionMask | ButtonMotionMask |
                       KeyPressMask | PropertyChangeMask;
    const Window win =
        XCreateWindow(dpy, parent, 0, 0, current.width, current.height, 0,
                      CopyFromParent, InputOutput, CopyFromParent, CWEventMask,
                      &attrs);

    const UiSize minSize = sizeForScale(kMinUiScale);
    const UiSize maxSize = sizeForScale(kMaxUiScale);
    XSizeHints hints{};
    hints.flags = PAspect | PMinSize | PMaxSize;
    hints.min_aspect.x = hints.max_aspect.x = kPanelWidth;
    hints.min_aspect.y = hints.max_aspect.y = kPanelHeight;
    hints.min_width = static_cast<int>(minSize.width);
    hints.min_height = static_cast<int>(minSize.height);
    hints.max_width = static_cast<int>(maxSize.width);
    hints.max_height = static_cast<int>(maxSize.height);
    XSetWMNormalHints(dpy, win, &hints);
    XStoreName(dpy, win, kPluginName);
    XMapRaised(dpy, win);
    XFlush(dpy);

    panel_.loadFont(bundleResourceDir());
    xdnd_.init(dpy, win, [this](const std::string& path) {
      panel_.importDroppedFile(shared_, state_, path);
    });
    {
      std::lock_guard<std::mutex> lock(mutex_);
      display_ = dpy;
      window_ = win;
      opened_ = true;
      cv_.notify_all();
    }

    // Use the window's own visual and depth.  Hosts often give the plugin a
    // parent with a non-default visual (ARGB), and cairo refuses to draw on a
    // surface whose visual does not match the drawable.
    XWindowAttributes wa{};
    XGetWindowAttributes(dpy, win, &wa);
    dbg("window=%lu depth=%d visual=%p size=%dx%d", win, wa.depth,
        static_cast<void*>(wa.visual), current.width, current.height);
    cairo_surface_t* windowSurface =
        cairo_xlib_surface_create(dpy, win, wa.visual, current.width, current.height);
    dbg("xlib surface status=%s",
        cairo_status_to_string(cairo_surface_status(windowSurface)));
    const cairo_format_t imageFormat =
        wa.depth == 32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    cairo_surface_t* backing = nullptr;
    cairo_t* cr = nullptr;

    auto rebuildBacking = [&]() {
      if (cr) {
        cairo_destroy(cr);
        cr = nullptr;
      }
      if (backing) {
        cairo_surface_destroy(backing);
        backing = nullptr;
      }
      backing = cairo_image_surface_create(imageFormat, current.width,
                                           current.height);
      cr = cairo_create(backing);
    };
    rebuildBacking();

    while (true) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quitting_) {
          break;
        }
      }
      const UiSize want = size();
      if (want.width != current.width || want.height != current.height) {
        current = want;
        XResizeWindow(dpy, win, current.width, current.height);
        cairo_xlib_surface_set_size(windowSurface, current.width, current.height);
        rebuildBacking();
      }
      while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        handleEvent(ev);
      }
      panel_.tick(shared_, state_);
      cairo_save(cr);
      cairo_scale(cr, scale(), scale());
      panel_.draw(cr, shared_, state_);
      cairo_restore(cr);
      cairo_surface_flush(backing);
      cairo_t* xcr = cairo_create(windowSurface);
      cairo_set_source_surface(xcr, backing, 0, 0);
      cairo_paint(xcr);
      cairo_destroy(xcr);
      // Required: cairo's Xlib backend buffers the composite, and without
      // this flush nothing reaches the server (the window stays black).
      cairo_surface_flush(windowSurface);
      XFlush(dpy);
      static int frames = 0;
      if (debugOn() && (frames++ % 120) == 0) {
        XWindowAttributes ga;
        XGetWindowAttributes(dpy, win, &ga);
        dbg("frame %d geom=%dx%d mapped=%d", frames, ga.width, ga.height,
            ga.map_state == IsViewable);
      }
      if (requestFlush && shared_.anyDirty()) {
        requestFlush();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    if (cr) {
      cairo_destroy(cr);
    }
    if (backing) {
      cairo_surface_destroy(backing);
    }
    if (windowSurface) {
      cairo_surface_destroy(windowSurface);
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      display_ = nullptr;
      window_ = 0;
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }

  // Event coordinates are physical pixels; the panel works in logical units.
  void handleEvent(const XEvent& ev) {
    if (xdnd_.handle(ev)) {
      return;
    }
    const double s = scale();
    switch (ev.type) {
      case ButtonPress: {
        if (display_ && window_) {
          XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
          XFlush(display_);
        }
        const bool fine = (ev.xbutton.state & ShiftMask) != 0;
        const double x = ev.xbutton.x / s;
        const double y = ev.xbutton.y / s;
        if (ev.xbutton.button == Button4) {
          panel_.wheel(shared_, state_, x, y, 1.0);
        } else if (ev.xbutton.button == Button5) {
          panel_.wheel(shared_, state_, x, y, -1.0);
        } else if (ev.xbutton.button == Button1) {
          panel_.pointerDown(shared_, state_, x, y, fine);
        } else if (ev.xbutton.button == Button3) {
          panel_.beginTextEntry(shared_, state_, x, y);
        }
        break;
      }
      case ButtonRelease:
        panel_.pointerUp();
        break;
      case KeyPress: {
        char buf[8] = {};
        KeySym sym = 0;
        XKeyEvent key = ev.xkey;
        const int n = XLookupString(&key, buf, sizeof(buf), &sym, nullptr);
        if (sym == XK_Return || sym == XK_KP_Enter) {
          panel_.textKey(shared_, state_, Panel::kTextEnter, 0);
        } else if (sym == XK_Escape) {
          panel_.textKey(shared_, state_, Panel::kTextEscape, 0);
        } else if (sym == XK_BackSpace) {
          panel_.textKey(shared_, state_, Panel::kTextBackspace, 0);
        } else if (n > 0) {
          panel_.textKey(shared_, state_, 0, buf[0]);
        }
        break;
      }
      case MotionNotify: {
        const bool fine = (ev.xmotion.state & ShiftMask) != 0;
        panel_.pointerMove(shared_, state_, ev.xmotion.x / s, ev.xmotion.y / s,
                           fine);
        break;
      }
      default:
        break;
    }
  }

  Shared& shared_;
  PanelState state_;
  Panel panel_;
  XdndDropTarget xdnd_;
  std::atomic<double> scale_{1.0};
  std::atomic<bool> resizePending_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  Display* display_ = nullptr;
  Window window_ = 0;
  Window parentWindow_ = 0;
  bool opened_ = false;
  bool openFailed_ = false;
  bool quitting_ = false;
  std::thread thread_;
};
#endif

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------

struct Plugin {
  clap_plugin_t plugin{};
  const clap_host_t* host = nullptr;
  Shared shared;
  bass::CrossfadeEngine engine;
  Editor editor{shared};
  UserIrLibrary userIr;
  NamLibrary nam;
  double sampleRate = 48000.0;
  Meters meters;
  std::atomic<bool> active{false};
};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION,
    kPluginId,
    kPluginName,
    kVendor,
    "https://github.com/tylorismusic-commits/bass-suite",
    "",
    "",
    kVersion,
    "Four bass amps, a cab section, compressor, drive and a NAM capture page in one plugin.",
    nullptr,
};

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                 CLAP_PLUGIN_FEATURE_STEREO, nullptr};

void writeName(char* dst, size_t size, const char* src) {
  if (size == 0) {
    return;
  }
  std::strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

Plugin* self(const clap_plugin_t* p) {
  return static_cast<Plugin*>(p->plugin_data);
}

int currentModel(const Plugin* s) {
  return static_cast<int>(s->shared.get(P_MODEL) + 0.5);
}

bool paramAppliesToModel(uint32_t param, int model) {
  switch (param) {
    case P_USER_IR_0:
    case P_USER_IR_1:
    case P_USER_IR_2:
    case P_USER_IR_3:
      return param == P_USER_IR_0 + static_cast<uint32_t>(model);
    case P_RESONANCE:
      return model == 2 || model == 3;
    case P_PRESENCE:
      return model != 0;
    default:
      return true;
  }
}

uint32_t paramsCount(const clap_plugin_t*) { return P_COUNT; }

bool paramsInfo(const clap_plugin_t* p, uint32_t index,
                clap_param_info_t* info) {
  if (index >= P_COUNT) {
    return false;
  }
  const ParamSpec& s = kParamSpecs[index];
  std::memset(info, 0, sizeof(*info));
  info->id = index;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  if (s.stepped) {
    info->flags |= CLAP_PARAM_IS_STEPPED;
  }
  if (s.hidden || !paramAppliesToModel(index, currentModel(self(p)))) {
    info->flags |= CLAP_PARAM_IS_HIDDEN;
  }
  writeName(info->name, sizeof(info->name), s.name);
  writeName(info->module, sizeof(info->module), s.module);
  info->min_value = s.min;
  info->max_value = s.max;
  info->default_value = s.def;
  return true;
}

bool paramsValue(const clap_plugin_t* p, clap_id id, double* value) {
  if (id >= P_COUNT) {
    return false;
  }
  *value = self(p)->shared.get(id);
  return true;
}

const char* labelForValue(const ParamSpec& spec, double value) {
  if (spec.labels.count <= 0) {
    return nullptr;
  }
  const int i = std::clamp(static_cast<int>(value + 0.5), 0,
                           spec.labels.count - 1);
  return spec.labels.names[i];
}

bool isOn(double value) { return value > 0.5; }

bool paramsValueToText(const clap_plugin_t* p, clap_id id, double value,
                       char* out, uint32_t size) {
  if (id >= P_COUNT || size == 0) {
    return false;
  }
  if (id == P_NAM_MODEL) {
    const int index = std::clamp(static_cast<int>(value + 0.5), 0, 63);
    if (index == 0) {
      std::snprintf(out, size, "None");
    } else {
      const std::string name = self(p)->nam.nameForIndex(index);
      std::snprintf(out, size, "%s", name.empty() ? "None" : name.c_str());
    }
    return true;
  }
  if (id >= P_USER_IR_0 && id <= P_USER_IR_3) {
    const int index = std::clamp(static_cast<int>(value + 0.5), 0, kUserIrMax);
    if (index == 0) {
      std::snprintf(out, size, "None");
    } else {
      const std::string name = self(p)->userIr.nameForIndex(index);
      std::snprintf(out, size, "%s", name.empty() ? "None" : name.c_str());
    }
    return true;
  }
  const ParamSpec& spec = kParamSpecs[id];
  if (const char* label = labelForValue(spec, value)) {
    std::snprintf(out, size, "%s", label);
    return true;
  }
  switch (id) {
    case P_CAB_LEVEL:
      std::snprintf(out, size, "%+.1f dB",
                    cabLevelDb(static_cast<float>(value)));
      return true;
    case P_CAB_LOWCUT:
      std::snprintf(out, size, "%.0f Hz",
                    cabLowCutHz(static_cast<float>(value)));
      return true;
    case P_CAB_HIGHCUT:
      std::snprintf(out, size, "%.0f Hz",
                    cabHighCutHz(static_cast<float>(value)));
      return true;
    case P_LIMITER_THRESH:
      std::snprintf(out, size, "%.1f dB", -24.0 + 24.0 * value);
      return true;
    case P_NAM_LEVEL:
      std::snprintf(out, size, "%+.1f dB", -12.0 + 24.0 * value);
      return true;
    case P_GATE_THRESH:
      std::snprintf(out, size, "%.0f dBFS", -80.0 + 60.0 * value);
      return true;
    case P_GATE_DECAY:
      std::snprintf(out, size, "%.0f ms", 20.0 + 980.0 * value);
      return true;
    case P_OUT_EQ_LOW:
    case P_OUT_EQ_LOW_MID:
    case P_OUT_EQ_HIGH_MID:
    case P_OUT_EQ_HIGH:
      std::snprintf(out, size, "%+.1f dB", (value - 0.5) * 24.0);
      return true;
    case P_OUT_HPF:
      std::snprintf(out, size, "%.0f Hz", outHpfHz(static_cast<float>(value)));
      return true;
    case P_OUT_LPF:
      std::snprintf(out, size, "%.0f Hz", outLpfHz(static_cast<float>(value)));
      return true;
      return true;
    default:
      break;
  }
  if (spec.toggled) {
    std::snprintf(out, size, "%s", isOn(value) ? "On" : "Off");
    return true;
  }
  std::snprintf(out, size, "%.0f%%", value * 100.0);
  return true;
}

bool ieq(const char* a, const char* b) {
  while (*a && *b) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

bool parseNumber(const char* text, double* out) {
  char* end = nullptr;
  const double v = std::strtod(text, &end);
  if (end == text) {
    return false;
  }
  *out = v;
  return true;
}

bool paramsTextToValue(const clap_plugin_t* p, clap_id id, const char* text,
                       double* value) {
  (void)p;
  if (id >= P_COUNT || !text) {
    return false;
  }
  const ParamSpec& spec = kParamSpecs[id];
  if (spec.labels.count > 0) {
    for (int i = 0; i < spec.labels.count; ++i) {
      if (ieq(text, spec.labels.names[i])) {
        *value = i;
        return true;
      }
    }
  }
  if (spec.toggled) {
    if (ieq(text, "on") || ieq(text, "true") || ieq(text, "yes")) {
      *value = 1.0;
      return true;
    }
    if (ieq(text, "off") || ieq(text, "false") || ieq(text, "no")) {
      *value = 0.0;
      return true;
    }
  }
  double v = 0.0;
  if (!parseNumber(text, &v)) {
    return false;
  }
  switch (id) {
    case P_CAB_LEVEL:
      v = (v + 24.0) / 30.0;
      break;
    case P_CAB_LOWCUT:
      v = std::log(std::max(v, 20.0) / 20.0) / std::log(15.0);
      break;
    case P_CAB_HIGHCUT:
      v = std::log(std::max(v, 2000.0) / 2000.0) / std::log(10.0);
      break;
    case P_OUT_EQ_LOW:
    case P_OUT_EQ_LOW_MID:
    case P_OUT_EQ_HIGH_MID:
    case P_OUT_EQ_HIGH:
      v = v / 24.0 + 0.5;
      break;
    case P_OUT_HPF:
      v = std::log(std::max(v, 20.0) / 20.0) / std::log(15.0);
      break;
    case P_OUT_LPF:
      v = std::log(std::max(v, 2000.0) / 2000.0) / std::log(10.0);
      break;
    default:
      break;
  }
  if (std::strchr(text, '%')) {
    v /= 100.0;
  }
  *value = std::clamp(v, spec.min, spec.max);
  if (spec.stepped) {
    *value = std::round(*value);
  }
  return true;
}

void applyEvents(Plugin* s, const clap_input_events_t* in) {
  if (!in) {
    return;
  }
  const uint32_t n = in->size(in);
  for (uint32_t i = 0; i < n; ++i) {
    const clap_event_header_t* h = in->get(in, i);
    if (!h) {
      continue;
    }
    if (h->type == CLAP_EVENT_PARAM_VALUE) {
      const auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
      if (e->param_id < P_COUNT) {
        s->shared.setFromHost(e->param_id, e->value);
      }
    } else if (h->type == CLAP_EVENT_PARAM_MOD) {
      const auto* e = reinterpret_cast<const clap_event_param_mod_t*>(h);
      if (e->param_id < P_COUNT) {
        s->shared.setFromHost(e->param_id, e->amount);
      }
    }
  }
}

void emitDirty(Plugin* s, const clap_output_events_t* out) {
  if (!out) {
    return;
  }
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    if (!s->shared.takeDirty(i)) {
      continue;
    }
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.flags = 0;
    ev.param_id = i;
    ev.cookie = nullptr;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = s->shared.get(i);
    out->try_push(out, &ev.header);
  }
}

void paramsFlush(const clap_plugin_t* p, const clap_input_events_t* in,
                 const clap_output_events_t* out) {
  Plugin* s = self(p);
  applyEvents(s, in);
  emitDirty(s, out);
}

const clap_plugin_params_t kParamsExt = {
    paramsCount, paramsInfo, paramsValue, paramsValueToText, paramsTextToValue,
    paramsFlush,
};

// --- State ------------------------------------------------------------------

struct StateHeader {
  uint32_t version;
  uint32_t count;
};

struct UserIrStateHeader {
  uint32_t version;
  uint32_t count;
};

bool stateSave(const clap_plugin_t* p, const clap_ostream_t* stream) {
  Plugin* s = self(p);
  if (!stream) {
    return false;
  }
  const StateHeader header{1u, static_cast<uint32_t>(P_COUNT)};
  double values[P_COUNT];
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    values[i] = s->shared.get(i);
  }
  const double scale = s->editor.scale();
  if (stream->write(stream, &header, sizeof(header)) !=
      static_cast<int64_t>(sizeof(header))) {
    return false;
  }
  if (stream->write(stream, values, sizeof(values)) !=
      static_cast<int64_t>(sizeof(values))) {
    return false;
  }
  if (stream->write(stream, &scale, sizeof(scale)) !=
      static_cast<int64_t>(sizeof(scale))) {
    return false;
  }

  // Portable user IRs: embed the selected WAV files after the parameter
  // block.  Older session files simply end here and still load.
  std::vector<std::pair<int, std::pair<std::string, std::vector<unsigned char>>>>
      irs;
  for (int amp = 0; amp < 4; ++amp) {
    const int index = static_cast<int>(
        s->shared.get(P_USER_IR_0 + static_cast<uint32_t>(amp)) + 0.5);
    std::string name;
    std::vector<unsigned char> bytes;
    if (index > 0 && s->userIr.bytesForIndex(index, name, bytes)) {
      irs.push_back({amp, {name, std::move(bytes)}});
    }
  }
  const UserIrStateHeader irHeader{1u, static_cast<uint32_t>(irs.size())};
  if (stream->write(stream, &irHeader, sizeof(irHeader)) !=
      static_cast<int64_t>(sizeof(irHeader))) {
    return false;
  }
  for (const auto& item : irs) {
    const uint32_t amp = static_cast<uint32_t>(item.first);
    const uint32_t nameLen = static_cast<uint32_t>(item.second.first.size());
    const uint32_t dataLen =
        static_cast<uint32_t>(item.second.second.size());
    if (stream->write(stream, &amp, sizeof(amp)) !=
            static_cast<int64_t>(sizeof(amp)) ||
        stream->write(stream, &nameLen, sizeof(nameLen)) !=
            static_cast<int64_t>(sizeof(nameLen)) ||
        stream->write(stream, item.second.first.data(), nameLen) !=
            static_cast<int64_t>(nameLen) ||
        stream->write(stream, &dataLen, sizeof(dataLen)) !=
            static_cast<int64_t>(sizeof(dataLen)) ||
        stream->write(stream, item.second.second.data(), dataLen) !=
            static_cast<int64_t>(dataLen)) {
      return false;
    }
  }
  return true;
}

bool stateLoad(const clap_plugin_t* p, const clap_istream_t* stream) {
  Plugin* s = self(p);
  if (!stream) {
    return false;
  }
  StateHeader header{};
  if (stream->read(stream, &header, sizeof(header)) !=
          static_cast<int64_t>(sizeof(header)) ||
      header.version != 1u || header.count == 0 || header.count > 4096) {
    return false;
  }
  std::vector<double> values(header.count);
  if (stream->read(stream, values.data(),
                   static_cast<uint64_t>(header.count) * sizeof(double)) !=
      static_cast<int64_t>(header.count * sizeof(double))) {
    return false;
  }
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    s->shared.setFromHost(i, i < header.count ? values[i]
                                               : kParamSpecs[i].def);
  }
  double scale = 0.0;
  if (stream->read(stream, &scale, sizeof(scale)) ==
          static_cast<int64_t>(sizeof(scale)) &&
      scale > 0.0) {
    s->editor.resize(scale);
  }

  UserIrStateHeader irHeader{};
  if (stream->read(stream, &irHeader, sizeof(irHeader)) !=
          static_cast<int64_t>(sizeof(irHeader)) ||
      irHeader.version != 1u || irHeader.count > 4u) {
    return true;
  }
  for (uint32_t i = 0; i < irHeader.count; ++i) {
    uint32_t amp = 0;
    uint32_t nameLen = 0;
    uint32_t dataLen = 0;
    if (stream->read(stream, &amp, sizeof(amp)) !=
            static_cast<int64_t>(sizeof(amp)) ||
        amp >= 4u ||
        stream->read(stream, &nameLen, sizeof(nameLen)) !=
            static_cast<int64_t>(sizeof(nameLen)) ||
        nameLen > 1024u) {
      return false;
    }
    std::string name(nameLen, '\0');
    if (nameLen &&
        stream->read(stream, name.data(), nameLen) !=
            static_cast<int64_t>(nameLen)) {
      return false;
    }
    if (stream->read(stream, &dataLen, sizeof(dataLen)) !=
            static_cast<int64_t>(sizeof(dataLen)) ||
        dataLen > 64u * 1024u * 1024u) {
      return false;
    }
    std::vector<unsigned char> bytes(dataLen);
    if (dataLen && stream->read(stream, bytes.data(), dataLen) !=
                       static_cast<int64_t>(dataLen)) {
      return false;
    }
    const int index = s->userIr.importWav(name, bytes, static_cast<int>(amp));
    if (index > 0) {
      s->shared.setFromHost(P_USER_IR_0 + amp,
                            static_cast<double>(index));
    }
  }
  s->shared.requestReload();
  return true;
}

const clap_plugin_state_t kStateExt = {stateSave, stateLoad};

// --- Audio ports ------------------------------------------------------------

uint32_t audioPortsCount(const clap_plugin_t*, bool) { return 1; }

bool audioPortsInfo(const clap_plugin_t*, uint32_t index, bool isInput,
                    clap_audio_port_info_t* info) {
  if (index != 0) {
    return false;
  }
  std::memset(info, 0, sizeof(*info));
  info->id = isInput ? 0u : 1u;
  writeName(info->name, sizeof(info->name), isInput ? "Input" : "Output");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}

const clap_plugin_audio_ports_t kAudioPortsExt = {audioPortsCount,
                                                  audioPortsInfo};

// --- GUI --------------------------------------------------------------------

bool guiIsApiSupported(const clap_plugin_t*, const char* api, bool isFloating) {
#ifdef SUITE_MACOS_COCOA
  (void)isFloating;
  return api && std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#else
  return !isFloating && api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif
}

bool guiGetPreferredApi(const clap_plugin_t*, const char** api,
                        bool* isFloating) {
#ifdef SUITE_MACOS_COCOA
  *api = CLAP_WINDOW_API_COCOA;
#else
  *api = CLAP_WINDOW_API_X11;
#endif
  *isFloating = false;
  return true;
}

bool guiCreate(const clap_plugin_t*, const char*, bool) { return true; }

void guiDestroy(const clap_plugin_t* p) { self(p)->editor.close(); }

bool guiSetScale(const clap_plugin_t*, double) { return false; }

bool guiGetSize(const clap_plugin_t* p, uint32_t* width, uint32_t* height) {
  const UiSize s = self(p)->editor.size();
  *width = s.width;
  *height = s.height;
  return true;
}

bool guiCanResize(const clap_plugin_t*) { return true; }

bool guiGetResizeHints(const clap_plugin_t*, clap_gui_resize_hints_t* hints) {
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = true;
  hints->aspect_ratio_width = kPanelWidth;
  hints->aspect_ratio_height = kPanelHeight;
  return true;
}

bool guiAdjustSize(const clap_plugin_t*, uint32_t* width, uint32_t* height) {
  const UiSize s = snapSize(*width, *height);
  *width = s.width;
  *height = s.height;
  return true;
}

bool guiSetSize(const clap_plugin_t* p, uint32_t width, uint32_t height) {
  self(p)->editor.resize(scaleForRequest(width, height));
  return true;
}

bool guiSetParent(const clap_plugin_t* p, const clap_window_t* window) {
#ifdef SUITE_MACOS_COCOA
  return self(p)->editor.open(window->cocoa);
#else
  return self(p)->editor.open(
      window->x11 ? reinterpret_cast<void*>(static_cast<uintptr_t>(window->x11))
                  : nullptr);
#endif
}

bool guiSetTransient(const clap_plugin_t*, const clap_window_t*) {
  return false;
}

void guiSuggestTitle(const clap_plugin_t*, const char*) {}

bool guiShow(const clap_plugin_t*) { return true; }

bool guiHide(const clap_plugin_t*) { return true; }

const clap_plugin_gui_t kGuiExt = {
    guiIsApiSupported, guiGetPreferredApi, guiCreate, guiDestroy, guiSetScale,
    guiGetSize, guiCanResize, guiGetResizeHints, guiAdjustSize, guiSetSize,
    guiSetParent, guiSetTransient, guiSuggestTitle, guiShow, guiHide,
};

// --- Latency ----------------------------------------------------------------

uint32_t latencyGet(const clap_plugin_t* p) {
  return static_cast<uint32_t>(self(p)->engine.latencySamples());
}

const clap_plugin_latency_t kLatencyExt = {latencyGet};

// --- Plugin lifecycle -------------------------------------------------------

bool pluginInit(const clap_plugin_t*) { return true; }

void pluginDestroy(const clap_plugin_t* p) { delete self(p); }

bool pluginActivate(const clap_plugin_t* p, double sampleRate, uint32_t,
                    uint32_t) {
  Plugin* s = self(p);
  const bool rateChanged = s->sampleRate != sampleRate;
  s->sampleRate = sampleRate;
  s->engine.prepare(static_cast<float>(sampleRate));
  // Re-load the factory cabs only when the host rate differs from the
  // constructor's 48 kHz load; a same-rate re-activation must not re-read
  // 48 WAVs on the audio start path.
  if (rateChanged) {
    s->engine.loadFactoryCabs(bundleResourceDir() + "/irs");
  }
  s->userIr.setSampleRate(static_cast<float>(sampleRate));
  s->nam.setHostRate(sampleRate);
  s->engine.setUserIrBank(s->userIr.bank());
  s->engine.setControls(controlsFrom(s->shared));
  s->active.store(true, std::memory_order_relaxed);
  return true;
}

void pluginDeactivate(const clap_plugin_t* p) {
  self(p)->active.store(false, std::memory_order_relaxed);
}

bool pluginStartProcessing(const clap_plugin_t*) { return true; }

void pluginStopProcessing(const clap_plugin_t*) {}

void pluginReset(const clap_plugin_t* p) { self(p)->engine.reset(); }

clap_process_status pluginProcess(const clap_plugin_t* p,
                                  const clap_process_t* proc) {
  Plugin* s = self(p);
  const uint32_t frames = proc->frames_count;
  applyEvents(s, proc->in_events);
  emitDirty(s, proc->out_events);

  if (proc->audio_inputs_count == 0 || proc->audio_outputs_count == 0) {
    return CLAP_PROCESS_CONTINUE;
  }
  const clap_audio_buffer_t& inBuf = proc->audio_inputs[0];
  const clap_audio_buffer_t& outBuf = proc->audio_outputs[0];
  if (!outBuf.data32 || !inBuf.data32 || !outBuf.data32[0] || !inBuf.data32[0]) {
    return CLAP_PROCESS_CONTINUE;
  }
  float* left = outBuf.data32[0];
  float* right = outBuf.channel_count > 1 && outBuf.data32[1] ? outBuf.data32[1]
                                                              : left;
  const float* inL = inBuf.data32[0];
  const float* inR =
      inBuf.channel_count > 1 && inBuf.data32[1] ? inBuf.data32[1] : inL;
  if (left != inL) {
    std::memcpy(left, inL, frames * sizeof(float));
  }
  if (right != inR) {
    std::memcpy(right, inR, frames * sizeof(float));
  }

  if (s->shared.takeReload()) {
    s->engine.requestReload();
  }
  s->nam.setSelected(static_cast<int>(s->shared.get(P_NAM_MODEL) + 0.5));
  s->engine.setUserIrBank(s->userIr.bank());
  s->engine.setNamModel(s->nam.model());
  s->engine.setControls(controlsFrom(s->shared));
  s->engine.process(left, right, static_cast<int>(frames), s->meters);

  s->shared.inPeakDb.store(s->meters.inPeakDb, std::memory_order_relaxed);
  s->shared.outPeakDb.store(s->meters.outPeakDb, std::memory_order_relaxed);
  s->shared.inPeakLDb.store(s->meters.inPeakLDb, std::memory_order_relaxed);
  s->shared.inPeakRDb.store(s->meters.inPeakRDb, std::memory_order_relaxed);
  s->shared.outPeakLDb.store(s->meters.outPeakLDb, std::memory_order_relaxed);
  s->shared.outPeakRDb.store(s->meters.outPeakRDb, std::memory_order_relaxed);
  s->shared.outVuDb.store(s->meters.outVuDb, std::memory_order_relaxed);
  s->shared.tunerHz.store(s->meters.tunerHz, std::memory_order_relaxed);
  s->shared.tunerCents.store(s->meters.tunerCents, std::memory_order_relaxed);
  s->shared.tunerConfidence.store(s->meters.tunerConfidence, std::memory_order_relaxed);
  s->shared.tunerNote.store(s->meters.tunerNote, std::memory_order_relaxed);
  s->shared.meterSerial.fetch_add(1, std::memory_order_relaxed);
  return CLAP_PROCESS_CONTINUE;
}

const void* pluginGetExtension(const clap_plugin_t*, const char* id) {
  if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
    return &kParamsExt;
  }
  if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
    return &kStateExt;
  }
  if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
    return &kAudioPortsExt;
  }
  if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
    return &kGuiExt;
  }
  if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) {
    return &kLatencyExt;
  }
  return nullptr;
}

void pluginOnMainThread(const clap_plugin_t*) {}

Plugin* createPlugin(const clap_host_t* host) {
  Plugin* s = new Plugin();
  s->host = host;
  applyStartupPreset(s->shared);
  s->plugin.desc = &kDescriptor;
  s->plugin.plugin_data = s;
  s->plugin.init = pluginInit;
  s->plugin.destroy = pluginDestroy;
  s->plugin.activate = pluginActivate;
  s->plugin.deactivate = pluginDeactivate;
  s->plugin.start_processing = pluginStartProcessing;
  s->plugin.stop_processing = pluginStopProcessing;
  s->plugin.reset = pluginReset;
  s->plugin.process = pluginProcess;
  s->plugin.get_extension = pluginGetExtension;
  s->plugin.on_main_thread = pluginOnMainThread;
  s->engine.prepare(48000.0f);
  s->engine.loadFactoryCabs(bundleResourceDir() + "/irs");
  s->userIr.start(userIrDir(), 48000.0f);
  s->nam.start(userNamDir(), 48000.0, 4096);
  s->engine.setUserIrBank(s->userIr.bank());
  s->engine.setControls(controlsFrom(s->shared));
  s->editor.requestFlush = [s] {
    if (s->host && s->host->request_process) {
      s->host->request_process(s->host);
    }
  };
  return s;
}

uint32_t factoryCount(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factoryDescriptor(const clap_plugin_factory_t*,
                                                  uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*,
                                   const clap_host_t* host, const char* id) {
  if (std::strcmp(id, kPluginId) != 0) {
    return nullptr;
  }
  return &createPlugin(host)->plugin;
}

const clap_plugin_factory_t kFactory = {factoryCount, factoryDescriptor,
                                        factoryCreate};

bool entryInit(const char*) { return true; }

void entryDeinit() {}

const void* entryGetFactory(const char* factoryId) {
  return std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory
                                                             : nullptr;
}

} // namespace
} // namespace bass

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    bass::entryInit,
    bass::entryDeinit,
    bass::entryGetFactory,
};
