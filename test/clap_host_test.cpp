// Bass Suite — CLAP host smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/clap.h>

#include <dlfcn.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "params.hpp"

namespace {

const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
void hostRequestRestart(const clap_host_t*) {}
void hostRequestProcess(const clap_host_t*) {}
void hostRequestCallback(const clap_host_t*) {}

uint32_t inSize(const clap_input_events_t*) { return 0; }
const clap_event_header_t* inGet(const clap_input_events_t*, uint32_t) {
  return nullptr;
}
bool outPush(const clap_output_events_t*, const clap_event_header_t*) {
  return true;
}

int64_t streamWrite(const clap_ostream_t* stream, const void* buffer,
                    uint64_t size) {
  auto* out = static_cast<std::vector<unsigned char>*>(stream->ctx);
  const auto* p = static_cast<const unsigned char*>(buffer);
  out->insert(out->end(), p, p + size);
  return static_cast<int64_t>(size);
}

int64_t streamRead(const clap_istream_t* stream, void* buffer, uint64_t size) {
  auto* in = static_cast<std::vector<unsigned char>*>(stream->ctx);
  if (in->empty()) {
    return 0;
  }
  const uint64_t n = std::min<uint64_t>(size, in->size());
  std::memcpy(buffer, in->data(), n);
  in->erase(in->begin(), in->begin() + static_cast<long>(n));
  return static_cast<int64_t>(n);
}

} // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "build/clap/BassSuite.clap";
  void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return 1;
  }
  auto* entry = static_cast<const clap_plugin_entry_t*>(
      dlsym(lib, "clap_entry"));
  if (!entry || !entry->init || !entry->get_factory) {
    std::fprintf(stderr, "clap_entry missing\n");
    return 1;
  }
  if (!entry->init(path)) {
    std::fprintf(stderr, "entry init failed\n");
    return 1;
  }
  auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  if (!factory) {
    std::fprintf(stderr, "factory missing\n");
    return 1;
  }
  if (factory->get_plugin_count(factory) != 1) {
    std::fprintf(stderr, "unexpected plugin count\n");
    return 1;
  }
  const clap_plugin_descriptor_t* desc =
      factory->get_plugin_descriptor(factory, 0);
  if (!desc || std::strcmp(desc->id, "com.tylorismusic.basssuite") != 0) {
    std::fprintf(stderr, "unexpected descriptor\n");
    return 1;
  }

  clap_host_t host{};
  host.clap_version = CLAP_VERSION;
  host.name = "bass-suite-test";
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
  if (!params || params->count(plugin) != bass::P_COUNT) {
    std::fprintf(stderr, "params extension missing or wrong count\n");
    return 1;
  }
  clap_param_info_t info{};
  if (!params->get_info(plugin, bass::P_MODEL, &info) ||
      std::strcmp(info.name, "Amp") != 0) {
    std::fprintf(stderr, "model parameter info wrong\n");
    return 1;
  }
  char text[64];
  if (!params->value_to_text(plugin, bass::P_MODEL, 2.0, text, sizeof(text)) ||
      std::strcmp(text, "SVT") != 0) {
    std::fprintf(stderr, "model value text wrong: %s\n", text);
    return 1;
  }

  auto* state = static_cast<const clap_plugin_state_t*>(
      plugin->get_extension(plugin, CLAP_EXT_STATE));
  if (!state) {
    std::fprintf(stderr, "state extension missing\n");
    return 1;
  }
  std::vector<unsigned char> saved;
  clap_ostream_t os{&saved, streamWrite};
  if (!state->save(plugin, &os)) {
    std::fprintf(stderr, "state save failed\n");
    return 1;
  }
  clap_istream_t is{&saved, streamRead};
  if (!state->load(plugin, &is)) {
    std::fprintf(stderr, "state load failed\n");
    return 1;
  }

  if (!plugin->activate(plugin, 48000.0, 1, 512)) {
    std::fprintf(stderr, "activate failed\n");
    return 1;
  }
  if (!plugin->start_processing(plugin)) {
    std::fprintf(stderr, "start_processing failed\n");
    return 1;
  }

  std::vector<float> l(256, 0.0f);
  std::vector<float> r(256, 0.0f);
  for (int i = 0; i < 256; ++i) {
    l[i] = 0.1f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
    r[i] = l[i];
  }
  float* inCh[2] = {l.data(), r.data()};
  float* outCh[2] = {l.data(), r.data()};
  clap_audio_buffer_t inBuf{};
  inBuf.data32 = inCh;
  inBuf.channel_count = 2;
  clap_audio_buffer_t outBuf{};
  outBuf.data32 = outCh;
  outBuf.channel_count = 2;
  clap_input_events_t inEvents{nullptr, inSize, inGet};
  clap_output_events_t outEvents{nullptr, outPush};
  clap_process_t proc{};
  proc.frames_count = 256;
  proc.audio_inputs = &inBuf;
  proc.audio_inputs_count = 1;
  proc.audio_outputs = &outBuf;
  proc.audio_outputs_count = 1;
  proc.in_events = &inEvents;
  proc.out_events = &outEvents;

  for (int block = 0; block < 8; ++block) {
    if (plugin->process(plugin, &proc) == CLAP_PROCESS_ERROR) {
      std::fprintf(stderr, "process failed\n");
      return 1;
    }
  }
  for (int i = 0; i < 256; ++i) {
    if (!std::isfinite(l[i]) || !std::isfinite(r[i])) {
      std::fprintf(stderr, "non-finite output\n");
      return 1;
    }
  }

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::printf("amp_clap_host: ok\n");
  return 0;
}
