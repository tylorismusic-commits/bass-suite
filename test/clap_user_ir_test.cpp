// Bass Suite — CLAP portable user-IR state test
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/clap.h>

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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

void writeWav(const std::string& path, const std::vector<float>& samples,
              int sampleRate) {
  std::ofstream out(path, std::ios::binary);
  const uint16_t channels = 1;
  const uint16_t bits = 16;
  const uint32_t dataSize =
      static_cast<uint32_t>(samples.size() * sizeof(int16_t));
  const uint32_t riffSize = 36u + dataSize;
  out.write("RIFF", 4);
  out.write(reinterpret_cast<const char*>(&riffSize), 4);
  out.write("WAVEfmt ", 8);
  const uint32_t fmtSize = 16;
  const uint16_t format = 1;
  const uint32_t rate = static_cast<uint32_t>(sampleRate);
  const uint32_t byteRate = rate * channels * bits / 8;
  const uint16_t blockAlign = channels * bits / 8;
  out.write(reinterpret_cast<const char*>(&fmtSize), 4);
  out.write(reinterpret_cast<const char*>(&format), 2);
  out.write(reinterpret_cast<const char*>(&channels), 2);
  out.write(reinterpret_cast<const char*>(&rate), 4);
  out.write(reinterpret_cast<const char*>(&byteRate), 4);
  out.write(reinterpret_cast<const char*>(&blockAlign), 2);
  out.write(reinterpret_cast<const char*>(&bits), 2);
  out.write("data", 4);
  out.write(reinterpret_cast<const char*>(&dataSize), 4);
  for (float f : samples) {
    const int16_t v = static_cast<int16_t>(
        std::min(std::max(f, -1.0f), 1.0f) * 32000.0f);
    out.write(reinterpret_cast<const char*>(&v), 2);
  }
}

clap_event_param_value_t paramEvent(clap_id id, double value) {
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.time = 0;
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

const clap_plugin_t* createPlugin(const clap_plugin_factory_t* factory,
                                  const clap_plugin_descriptor_t* desc,
                                  clap_host_t* host) {
  const clap_plugin_t* plugin = factory->create_plugin(factory, host, desc->id);
  if (!plugin || !plugin->init(plugin)) {
    return nullptr;
  }
  return plugin;
}

} // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "build/clap/BassSuite.clap";
  const std::string base = "/tmp/bass-suite-clap-ir-" + std::to_string(getpid());
  const std::string xdgA = base + "-a";
  const std::string xdgB = base + "-b";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(xdgA + "/bass-suite/irs/user", ec);
  std::filesystem::create_directories(xdgB + "/bass-suite/irs/user", ec);
  if (ec) {
    std::fprintf(stderr, "could not create temp dirs\n");
    return 1;
  }

  std::vector<float> ir(2048, 0.0f);
  for (size_t i = 0; i < ir.size(); ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    ir[i] = static_cast<float>(
        std::sin(2.0 * 3.14159265358979323846 * 900.0 * t) *
        std::exp(-t * 35.0));
  }
  ir[0] += 2.0f;
  writeWav(xdgA + "/bass-suite/irs/user/Embedded Cabinet.wav", ir, 48000);
  setenv("XDG_DATA_HOME", xdgA.c_str(), 1);

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
  host.name = "bass-suite-ir-test";
  host.vendor = "tylorismusic";
  host.url = "https://example.invalid";
  host.version = "0.1.0";
  host.get_extension = hostGetExtension;
  host.request_restart = hostRequestRestart;
  host.request_process = hostRequestProcess;
  host.request_callback = hostRequestCallback;

  const clap_plugin_t* first = createPlugin(factory, desc, &host);
  if (!first) {
    std::fprintf(stderr, "first create/init failed\n");
    return 1;
  }
  auto* firstParams = static_cast<const clap_plugin_params_t*>(
      first->get_extension(first, CLAP_EXT_PARAMS));
  auto* firstState = static_cast<const clap_plugin_state_t*>(
      first->get_extension(first, CLAP_EXT_STATE));
  if (!firstParams || !firstState) {
    std::fprintf(stderr, "extensions missing\n");
    return 1;
  }

  std::vector<clap_event_param_value_t> events;
  events.push_back(paramEvent(bass::P_CAB, 2.0));
  events.push_back(paramEvent(bass::P_USER_IR_1, 1.0));
  clap_input_events_t in{&events, eventsSize, eventsGet};
  clap_output_events_t out{nullptr, outPush};
  firstParams->flush(first, &in, &out);
  double selected = 0.0;
  if (!firstParams->get_value(first, bass::P_USER_IR_1, &selected) ||
      std::fabs(selected - 1.0) > 0.01) {
    std::fprintf(stderr, "user IR parameter did not stick\n");
    return 1;
  }

  std::vector<unsigned char> saved;
  clap_ostream_t os{&saved, streamWrite};
  if (!firstState->save(first, &os)) {
    std::fprintf(stderr, "state save failed\n");
    return 1;
  }
  const size_t baseState = sizeof(uint32_t) * 2 +
                           sizeof(double) * bass::P_COUNT + sizeof(double);
  if (saved.size() < baseState + ir.size() * sizeof(int16_t)) {
    std::fprintf(stderr, "state did not embed the user IR (%zu bytes)\n",
                 saved.size());
    return 1;
  }
  first->destroy(first);

  // Restore into a fresh instance with an empty user directory.
  setenv("XDG_DATA_HOME", xdgB.c_str(), 1);
  const clap_plugin_t* second = createPlugin(factory, desc, &host);
  if (!second) {
    std::fprintf(stderr, "second create/init failed\n");
    return 1;
  }
  auto* secondParams = static_cast<const clap_plugin_params_t*>(
      second->get_extension(second, CLAP_EXT_PARAMS));
  auto* secondState = static_cast<const clap_plugin_state_t*>(
      second->get_extension(second, CLAP_EXT_STATE));
  clap_istream_t is{&saved, streamRead};
  if (!secondState->load(second, &is)) {
    std::fprintf(stderr, "state load failed\n");
    return 1;
  }
  double restored = 0.0;
  if (!secondParams->get_value(second, bass::P_USER_IR_1, &restored) ||
      restored < 1.0) {
    std::fprintf(stderr, "restored user IR selection missing\n");
    return 1;
  }
  char text[256] = {};
  if (!secondParams->value_to_text(second, bass::P_USER_IR_1, restored, text,
                                   sizeof(text)) ||
      std::strcmp(text, "None") == 0) {
    std::fprintf(stderr, "restored user IR name missing\n");
    return 1;
  }
  const auto imported = std::filesystem::directory_iterator(
      xdgB + "/bass-suite/irs/user", ec);
  if (ec || imported == std::filesystem::directory_iterator()) {
    std::fprintf(stderr, "restored IR was not written to the user folder\n");
    return 1;
  }
  second->destroy(second);
  entry->deinit();
  dlclose(lib);
  std::filesystem::remove_all(base, ec);
  std::printf("amp_clap_user_ir: ok\n");
  return 0;
}
