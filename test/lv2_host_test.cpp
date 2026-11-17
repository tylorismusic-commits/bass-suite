// Bass Suite — LV2 host smoke test (lilv)
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lilv/lilv.h>
#include <lv2/core/lv2.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>

namespace {

constexpr const char* kUri =
    "https://github.com/tylorismusic-commits/bass-suite";

} // namespace

int main(int argc, char** argv) {
  const char* bundle = argc > 1 ? argv[1] : "build/lv2/bass-suite.lv2";
  std::string searchPath = bundle;
  const size_t slash = searchPath.find_last_of('/');
  if (slash != std::string::npos) {
    searchPath = searchPath.substr(0, slash);
  }
  if (setenv("LV2_PATH", searchPath.c_str(), 1) != 0) {
    std::fprintf(stderr, "setenv failed\n");
    return 1;
  }

  LilvWorld* world = lilv_world_new();
  lilv_world_load_all(world);
  const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
  LilvNode* uri = lilv_new_uri(world, kUri);
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, uri);
  lilv_node_free(uri);
  if (!plugin) {
    std::fprintf(stderr, "plugin not found: %s\n", kUri);
    lilv_world_free(world);
    return 1;
  }

  const uint32_t nports = lilv_plugin_get_num_ports(plugin);
  std::vector<float> controls(nports, 0.0f);
  std::vector<float> audioL(256, 0.0f);
  std::vector<float> audioR(256, 0.0f);
  for (int i = 0; i < 256; ++i) {
    audioL[i] = 0.1f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
    audioR[i] = audioL[i];
  }

  auto setSymbol = [&](const char* symbol, float value) {
    LilvNode* sym = lilv_new_string(world, symbol);
    const LilvPort* port = lilv_plugin_get_port_by_symbol(plugin, sym);
    lilv_node_free(sym);
    if (port) {
      controls[lilv_port_get_index(plugin, port)] = value;
    }
  };
  setSymbol("model", 4.0f);
  setSymbol("power", 1.0f);
  setSymbol("input", 0.5f);
  setSymbol("gain", 0.55f);
  setSymbol("master", 0.45f);
  setSymbol("output", 0.5f);
  setSymbol("od_on", 1.0f);
  setSymbol("od_drive", 0.28f);
  setSymbol("od_tone", 0.5f);
  setSymbol("od_level", 0.55f);

  LilvInstance* instance = lilv_plugin_instantiate(plugin, 48000.0, nullptr);
  if (!instance) {
    std::fprintf(stderr, "instantiate failed\n");
    lilv_world_free(world);
    return 1;
  }

  const LilvNode* audioPort = lilv_new_uri(world, LV2_CORE__AudioPort);
  for (uint32_t i = 0; i < nports; ++i) {
    const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
    if (lilv_port_is_a(plugin, port, audioPort)) {
      if (i == 0) {
        lilv_instance_connect_port(instance, i, audioL.data());
      } else if (i == 1) {
        lilv_instance_connect_port(instance, i, audioR.data());
      } else if (i == 2) {
        lilv_instance_connect_port(instance, i, audioL.data());
      } else {
        lilv_instance_connect_port(instance, i, audioR.data());
      }
    } else {
      lilv_instance_connect_port(instance, i, &controls[i]);
    }
  }
  lilv_node_free(const_cast<LilvNode*>(audioPort));

  lilv_instance_activate(instance);
  for (int block = 0; block < 8; ++block) {
    lilv_instance_run(instance, 256);
  }
  lilv_instance_deactivate(instance);

  for (int i = 0; i < 256; ++i) {
    if (!std::isfinite(audioL[i]) || !std::isfinite(audioR[i])) {
      std::fprintf(stderr, "non-finite output\n");
      lilv_instance_free(instance);
      lilv_world_free(world);
      return 1;
    }
  }
  float peak = 0.0f;
  for (float v : audioL) {
    peak = std::max(peak, std::fabs(v));
  }
  if (peak <= 0.0f) {
    std::fprintf(stderr, "silent output\n");
    lilv_instance_free(instance);
    lilv_world_free(world);
    return 1;
  }

  lilv_instance_free(instance);
  lilv_world_free(world);
  std::printf("amp_lv2_host: ok (%u ports, peak %.4f)\n", nports, peak);
  return 0;
}
