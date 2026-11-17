// Bass Suite — CLAP NAM loader integration test
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/clap.h>

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#include "params.hpp"

namespace {

const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
void hostRequestRestart(const clap_host_t*) {}
void hostRequestProcess(const clap_host_t*) {}
void hostRequestCallback(const clap_host_t*) {}

uint32_t eventsSize(const clap_input_events_t* events) {
  return static_cast<uint32_t>(
      static_cast<const std::vector<clap_event_param_value_t>*>(events->ctx)
          ->size());
}
const clap_event_header_t* eventsGet(const clap_input_events_t* events,
                                     uint32_t index) {
  const auto* list =
      static_cast<const std::vector<clap_event_param_value_t>*>(events->ctx);
  if (index >= list->size()) {
    return nullptr;
  }
  return reinterpret_cast<const clap_event_header_t*>(&(*list)[index]);
}
bool outPush(const clap_output_events_t*, const clap_event_header_t*) {
  return true;
}

clap_event_param_value_t paramEvent(clap_id id, double value) {
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.param_id = id;
  ev.value = value;
  return ev;
}

} // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "build/clap/BassSuite.clap";
  const std::string base = "/tmp/bass-suite-clap-nam-" + std::to_string(getpid());
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(base + "/bass-suite/nam", ec);
  std::filesystem::copy_file(NAM_TEST_MODEL,
                             base + "/bass-suite/nam/wavenet.nam",
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    std::fprintf(stderr, "could not stage NAM model\n");
    return 1;
  }
  setenv("XDG_DATA_HOME", base.c_str(), 1);

  void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return 1;
  }
  auto* entry = static_cast<const clap_plugin_entry_t*>(
      dlsym(lib, "clap_entry"));
  if (!entry || !entry->init || !entry->init(path)) {
    std::fprintf(stderr, "clap entry init failed\n");
    return 1;
  }
  auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  const clap_plugin_descriptor_t* desc =
      factory ? factory->get_plugin_descriptor(factory, 0) : nullptr;
  if (!desc) {
    std::fprintf(stderr, "factory/descriptor missing\n");
    return 1;
  }

  clap_host_t host{};
  host.clap_version = CLAP_VERSION;
  host.name = "bass-suite-nam-test";
  host.vendor = "tylorismusic";
  host.url = "https://example.invalid";
  host.version = "0.1.0";
  host.get_extension = hostGetExtension;
  host.request_restart = hostRequestRestart;
  host.request_process = hostRequestProcess;
  host.request_callback = hostRequestCallback;

  const clap_plugin_t* plugin = factory->create_plugin(factory, &host, desc->id);
  if (!plugin || !plugin->init(plugin)) {
    std::fprintf(stderr, "create/init failed\n");
    return 1;
  }
  auto* params = static_cast<const clap_plugin_params_t*>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  if (!params) {
    std::fprintf(stderr, "params missing\n");
    return 1;
  }
  std::vector<clap_event_param_value_t> events;
  events.push_back(paramEvent(bass::P_NAM_ON, 1.0));
  events.push_back(paramEvent(bass::P_NAM_MODEL, 1.0));
  events.push_back(paramEvent(bass::P_CAB_BYPASS, 1.0));
  clap_input_events_t in{&events, eventsSize, eventsGet};
  clap_output_events_t out{nullptr, outPush};
  params->flush(plugin, &in, &out);

  if (!plugin->activate(plugin, 48000.0, 1, 512) ||
      !plugin->start_processing(plugin)) {
    std::fprintf(stderr, "activate/start failed\n");
    return 1;
  }

  std::vector<float> l(512, 0.0f), r(512, 0.0f);
  float* inCh[2] = {l.data(), r.data()};
  float* outCh[2] = {l.data(), r.data()};
  clap_audio_buffer_t inBuf{};
  inBuf.data32 = inCh;
  inBuf.channel_count = 2;
  clap_audio_buffer_t outBuf{};
  outBuf.data32 = outCh;
  outBuf.channel_count = 2;
  clap_input_events_t noEvents{nullptr, [](const clap_input_events_t*) {
                                 return uint32_t(0);
                               },
                               [](const clap_input_events_t*, uint32_t)
                                   -> const clap_event_header_t* { return nullptr; }};
  clap_process_t proc{};
  proc.frames_count = 512;
  proc.audio_inputs = &inBuf;
  proc.audio_inputs_count = 1;
  proc.audio_outputs = &outBuf;
  proc.audio_outputs_count = 1;
  proc.in_events = &noEvents;
  proc.out_events = &out;

  // First block lets the wrapper publish the selected NAM index; the loader
  // then parses the model on its worker thread.
  plugin->process(plugin, &proc);
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  double energy = 0.0;
  for (int block = 0; block < 16; ++block) {
    for (int i = 0; i < 512; ++i) {
      l[i] = r[i] = 0.08f * std::sin(
          2.0 * 3.14159265358979 * 220.0 * (block * 512 + i) / 48000.0);
    }
    if (plugin->process(plugin, &proc) == CLAP_PROCESS_ERROR) {
      std::fprintf(stderr, "process failed\n");
      return 1;
    }
    for (int i = 0; i < 512; ++i) {
      if (!std::isfinite(l[i]) || !std::isfinite(r[i])) {
        std::fprintf(stderr, "non-finite NAM output\n");
        return 1;
      }
      energy += static_cast<double>(l[i]) * l[i];
    }
  }
  if (energy <= 0.0) {
    std::fprintf(stderr, "NAM path produced silence\n");
    return 1;
  }

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::filesystem::remove_all(base, ec);
  std::printf("amp_clap_nam: ok\n");
  return 0;
}
