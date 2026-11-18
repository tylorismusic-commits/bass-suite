// Bass Suite — LV2 plugin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Control ports mirror the CLAP parameter ids: port = kPortFirstParam + id.
// Meter ports are appended after the parameters.

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "dsp/crossfade_engine.hpp"
#include "dsp/nam_library.hpp"
#include "dsp/user_ir.hpp"
#include "params.hpp"
#include "presets.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/bass-suite";

enum Port : uint32_t {
  PORT_AUDIO_IN_L = 0,
  PORT_AUDIO_IN_R = 1,
  PORT_AUDIO_OUT_L = 2,
  PORT_AUDIO_OUT_R = 3,
  PORT_FIRST_PARAM = 4,
  PORT_OUT_PEAK_L = PORT_FIRST_PARAM + bass::P_COUNT,
  PORT_OUT_PEAK_R,
  PORT_IN_PEAK_L,
  PORT_IN_PEAK_R,
  PORT_OUT_VU,
  PORT_LATENCY,
  PORT_COUNT
};

struct Plugin {
  float sampleRate = 48000.0f;
  float lastPort[bass::P_COUNT] = {};
  bool portsSeen = false;
  bass::Shared shared;
  bass::CrossfadeEngine engine;
  bass::UserIrLibrary userIr;
  bass::NamLibrary nam;
  bass::Meters meters;

  const LV2_URID_Map* map = nullptr;
  const LV2_State_Make_Path* makePath = nullptr;
  const LV2_State_Free_Path* freePath = nullptr;
  uint32_t uridParams = 0;
  uint32_t uridUserIrs = 0;
  uint32_t uridChunk = 0;


  std::unique_ptr<float*[]> ports;

  float* audioInL = nullptr;
  float* audioInR = nullptr;
  float* audioOutL = nullptr;
  float* audioOutR = nullptr;
  float* outPeakL = nullptr;
  float* outPeakR = nullptr;
  float* inPeakL = nullptr;
  float* inPeakR = nullptr;
  float* outVu = nullptr;
  float* latency = nullptr;
};

LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                       const char* bundlePath,
                       const LV2_Feature* const* features) {
  auto* p = new Plugin();
  p->sampleRate = static_cast<float>(rate);
  applyStartupPreset(p->shared);
  for (const LV2_Feature* const* f = features; f && *f; ++f) {
    if (!std::strcmp((*f)->URI, LV2_URID__map)) {
      p->map = static_cast<const LV2_URID_Map*>(f[0]->data);
    } else if (!std::strcmp((*f)->URI, LV2_STATE__makePath)) {
      p->makePath = static_cast<const LV2_State_Make_Path*>(f[0]->data);
    } else if (!std::strcmp((*f)->URI, LV2_STATE__freePath)) {
      p->freePath = static_cast<const LV2_State_Free_Path*>(f[0]->data);
    }
  }
  if (p->map && p->map->map) {
    p->uridParams = p->map->map(p->map->handle, "urn:bass-suite:params");
    p->uridUserIrs = p->map->map(p->map->handle, "urn:bass-suite:user-irs");
    p->uridChunk = p->map->map(p->map->handle, LV2_ATOM__Chunk);
  }
  p->engine.prepare(p->sampleRate);
  if (bundlePath) {
    p->engine.loadFactoryCabs(std::string(bundlePath) + "/irs");
  }
  p->userIr.start(bass::userIrDir(), p->sampleRate);
  p->nam.start(bass::userNamDir(), p->sampleRate, 4096);
  p->engine.setUserIrBank(p->userIr.bank());
  p->engine.setControls(bass::controlsFrom(p->shared));
  return p;
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
  auto* p = static_cast<Plugin*>(handle);
  switch (port) {
    case PORT_AUDIO_IN_L: p->audioInL = static_cast<float*>(data); break;
    case PORT_AUDIO_IN_R: p->audioInR = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_L: p->audioOutL = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_R: p->audioOutR = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_L: p->outPeakL = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_R: p->outPeakR = static_cast<float*>(data); break;
    case PORT_IN_PEAK_L: p->inPeakL = static_cast<float*>(data); break;
    case PORT_IN_PEAK_R: p->inPeakR = static_cast<float*>(data); break;
    case PORT_OUT_VU: p->outVu = static_cast<float*>(data); break;
    case PORT_LATENCY: p->latency = static_cast<float*>(data); break;
    default:
      if (!p->ports) {
        p->ports = std::make_unique<float*[]>(PORT_COUNT);
        std::memset(p->ports.get(), 0, sizeof(float*) * PORT_COUNT);
      }
      if (port < PORT_COUNT) {
        p->ports[port] = static_cast<float*>(data);
      }
      break;
  }
}


void activate(LV2_Handle handle) {
  auto* p = static_cast<Plugin*>(handle);
  p->userIr.setSampleRate(p->sampleRate);
  p->nam.setHostRate(p->sampleRate);
  p->engine.setUserIrBank(p->userIr.bank());
  p->engine.reset();
  p->portsSeen = false;
  if (p->latency) {
    *p->latency = static_cast<float>(p->engine.latencySamples());
  }
}

void deactivate(LV2_Handle) {}

void run(LV2_Handle handle, uint32_t frames) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->audioInL || !p->audioOutL) {
    return;
  }
  if (!p->audioInR) {
    p->audioInR = p->audioInL;
  }
  if (!p->audioOutR) {
    p->audioOutR = p->audioOutL;
  }

  if (p->ports) {
    for (uint32_t i = 0; i < bass::P_COUNT; ++i) {
      float* v = p->ports[PORT_FIRST_PARAM + i];
      if (!v) {
        continue;
      }
      if (!p->portsSeen || *v != p->lastPort[i]) {
        p->shared.setFromHost(i, static_cast<double>(*v));
        p->lastPort[i] = *v;
      }
    }
    p->portsSeen = true;
  }
  p->nam.setSelected(static_cast<int>(p->shared.get(bass::P_NAM_MODEL) + 0.5));
  p->engine.setUserIrBank(p->userIr.bank());
  p->engine.setNamModel(p->nam.model());
  p->engine.setControls(bass::controlsFrom(p->shared));

  if (p->audioOutL != p->audioInL) {
    std::memcpy(p->audioOutL, p->audioInL, frames * sizeof(float));
  }
  if (p->audioOutR != p->audioInR) {
    std::memcpy(p->audioOutR, p->audioInR, frames * sizeof(float));
  }

  p->engine.process(p->audioOutL, p->audioOutR, static_cast<int>(frames),
                    p->meters);

  if (p->outPeakL) *p->outPeakL = p->meters.outPeakLDb;
  if (p->outPeakR) *p->outPeakR = p->meters.outPeakRDb;
  if (p->inPeakL) *p->inPeakL = p->meters.inPeakLDb;
  if (p->inPeakR) *p->inPeakR = p->meters.inPeakRDb;
  if (p->outVu) *p->outVu = p->meters.outVuDb;
  if (p->latency) {
    *p->latency = static_cast<float>(p->engine.latencySamples());
  }
  p->shared.inPeakDb.store(p->meters.inPeakDb, std::memory_order_relaxed);
  p->shared.outPeakDb.store(p->meters.outPeakDb, std::memory_order_relaxed);
  p->shared.inPeakLDb.store(p->meters.inPeakLDb, std::memory_order_relaxed);
  p->shared.inPeakRDb.store(p->meters.inPeakRDb, std::memory_order_relaxed);
  p->shared.outPeakLDb.store(p->meters.outPeakLDb, std::memory_order_relaxed);
  p->shared.outPeakRDb.store(p->meters.outPeakRDb, std::memory_order_relaxed);
  p->shared.outVuDb.store(p->meters.outVuDb, std::memory_order_relaxed);
  p->shared.tunerHz.store(p->meters.tunerHz, std::memory_order_relaxed);
  p->shared.tunerCents.store(p->meters.tunerCents, std::memory_order_relaxed);
  p->shared.tunerConfidence.store(p->meters.tunerConfidence, std::memory_order_relaxed);
  p->shared.tunerNote.store(p->meters.tunerNote, std::memory_order_relaxed);
  p->shared.meterSerial.fetch_add(1, std::memory_order_relaxed);
}

struct Lv2UserIrHeader {
  uint32_t version;
  uint32_t count;
};

void writeStateWav(Plugin* p, int amp, const std::vector<unsigned char>& bytes) {
  if (!p->makePath || !p->makePath->path || bytes.empty()) {
    return;
  }
  char relative[64];
  std::snprintf(relative, sizeof(relative), "user-irs/amp-%d.wav", amp);
  char* path = p->makePath->path(p->makePath->handle, relative);
  if (!path) {
    return;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (out) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  if (p->freePath && p->freePath->free_path) {
    p->freePath->free_path(p->freePath->handle, path);
  } else {
    std::free(path);
  }
}

LV2_State_Status stateSave(LV2_Handle handle, LV2_State_Store_Function store,
                           LV2_State_Handle data, uint32_t flags,
                           const LV2_Feature* const*) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->map || !store || !p->uridParams) {
    return LV2_STATE_ERR_NO_FEATURE;
  }
  (void)flags;
  double values[bass::P_COUNT];
  for (uint32_t i = 0; i < bass::P_COUNT; ++i) {
    values[i] = p->shared.get(i);
  }
  store(data, p->uridParams, values, sizeof(values), p->uridChunk,
        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

  std::vector<unsigned char> blob;
  const size_t headerAt = blob.size();
  blob.resize(headerAt + sizeof(Lv2UserIrHeader));
  uint32_t count = 0;
  for (int amp = 0; amp < 4; ++amp) {
    const int index = static_cast<int>(
        p->shared.get(bass::P_USER_IR_0 + static_cast<uint32_t>(amp)) + 0.5);
    std::string name;
    std::vector<unsigned char> bytes;
    if (index <= 0 || !p->userIr.bytesForIndex(index, name, bytes)) {
      continue;
    }
    writeStateWav(p, amp, bytes);
    const uint32_t ampU = static_cast<uint32_t>(amp);
    const uint32_t nameLen = static_cast<uint32_t>(name.size());
    const uint32_t dataLen = static_cast<uint32_t>(bytes.size());
    const size_t at = blob.size();
    blob.resize(at + sizeof(ampU) + sizeof(nameLen) + nameLen +
                sizeof(dataLen) + dataLen);
    unsigned char* dst = blob.data() + at;
    std::memcpy(dst, &ampU, sizeof(ampU));
    dst += sizeof(ampU);
    std::memcpy(dst, &nameLen, sizeof(nameLen));
    dst += sizeof(nameLen);
    std::memcpy(dst, name.data(), nameLen);
    dst += nameLen;
    std::memcpy(dst, &dataLen, sizeof(dataLen));
    dst += sizeof(dataLen);
    std::memcpy(dst, bytes.data(), dataLen);
    ++count;
  }
  Lv2UserIrHeader header{1u, count};
  if (!blob.empty()) {
    std::memcpy(blob.data(), &header, sizeof(header));
    store(data, p->uridUserIrs, blob.data(), blob.size(), p->uridChunk,
          LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  }
  return LV2_STATE_SUCCESS;
}

LV2_State_Status stateRestore(LV2_Handle handle,
                              LV2_State_Retrieve_Function retrieve,
                              LV2_State_Handle data, uint32_t flags,
                              const LV2_Feature* const*) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->map || !retrieve || !p->uridParams) {
    return LV2_STATE_ERR_NO_FEATURE;
  }
  (void)flags;
  size_t size = 0;
  uint32_t type = 0;
  uint32_t valueFlags = 0;
  const void* params = retrieve(data, p->uridParams, &size, &type,
                                &valueFlags);
  if (params && size % sizeof(double) == 0 && size > 0 &&
      size <= sizeof(double) * 4096) {
    const size_t count = size / sizeof(double);
    const double* values = static_cast<const double*>(params);
    for (uint32_t i = 0; i < bass::P_COUNT; ++i) {
      p->shared.setFromHost(i, i < count ? values[i]
                                         : bass::kParamSpecs[i].def);
    }
  }

  const void* irData = p->uridUserIrs
                           ? retrieve(data, p->uridUserIrs, &size, &type,
                                      &valueFlags)
                           : nullptr;
  if (irData && size >= sizeof(Lv2UserIrHeader)) {
    const unsigned char* ptr = static_cast<const unsigned char*>(irData);
    Lv2UserIrHeader header{};
    std::memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);
    size_t remaining = size - sizeof(header);
    if (header.version == 1u && header.count <= 4u) {
      for (uint32_t i = 0; i < header.count; ++i) {
        uint32_t amp = 0;
        uint32_t nameLen = 0;
        uint32_t dataLen = 0;
        if (remaining < sizeof(amp) + sizeof(nameLen)) {
          break;
        }
        std::memcpy(&amp, ptr, sizeof(amp));
        ptr += sizeof(amp);
        std::memcpy(&nameLen, ptr, sizeof(nameLen));
        ptr += sizeof(nameLen);
        remaining -= sizeof(amp) + sizeof(nameLen);
        if (amp >= 4u || nameLen > 1024u || remaining < nameLen) {
          break;
        }
        std::string name(reinterpret_cast<const char*>(ptr), nameLen);
        ptr += nameLen;
        remaining -= nameLen;
        if (remaining < sizeof(dataLen)) {
          break;
        }
        std::memcpy(&dataLen, ptr, sizeof(dataLen));
        ptr += sizeof(dataLen);
        remaining -= sizeof(dataLen);
        if (dataLen > 64u * 1024u * 1024u || remaining < dataLen) {
          break;
        }
        std::vector<unsigned char> bytes(ptr, ptr + dataLen);
        ptr += dataLen;
        remaining -= dataLen;
        const int index = p->userIr.importWav(name, bytes, static_cast<int>(amp));
        if (index > 0) {
          p->shared.setFromHost(bass::P_USER_IR_0 + amp,
                                static_cast<double>(index));
        }
      }
    }
  }
  p->engine.setUserIrBank(p->userIr.bank());
  p->engine.setControls(bass::controlsFrom(p->shared));
  return LV2_STATE_SUCCESS;
}

const LV2_State_Interface kStateInterface = {stateSave, stateRestore};

const void* extensionData(const char* uri) {
  if (uri && !std::strcmp(uri, LV2_STATE__interface)) {
    return &kStateInterface;
  }
  return nullptr;
}

void cleanup(LV2_Handle handle) { delete static_cast<Plugin*>(handle); }

const LV2_Descriptor kDescriptor = {kPluginUri, instantiate, connectPort,
                                    activate,  run,         deactivate,
                                    cleanup,   extensionData};

} // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}
