// Bass Suite — typed parameter values
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "params.hpp"

namespace bass {

inline std::string lowerText(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

inline std::string trimText(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

inline std::string formatParamValue(const Shared& shared, uint32_t id,
                                   double value) {
  (void)shared;
  const ParamSpec& s = kParamSpecs[id];
  if (s.labels.count > 0) {
    const int i = std::clamp(
        static_cast<int>(std::lround(value - s.min)), 0, s.labels.count - 1);
    return s.labels.names[i];
  }
  char buf[64];
  switch (id) {
    case P_INPUT:
    case P_OUTPUT:
      std::snprintf(buf, sizeof(buf), "%+.1f dB", (value - 0.5) * 24.0);
      return buf;
    case P_MASTER:
      std::snprintf(buf, sizeof(buf), "%.1f dB", -18.0 + 18.0 * value);
      return buf;
    case P_CAB_LEVEL:
      std::snprintf(buf, sizeof(buf), "%+.1f dB",
                    cabLevelDb(static_cast<float>(value)));
      return buf;
    case P_CAB_LOWCUT:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    cabLowCutHz(static_cast<float>(value)));
      return buf;
    case P_CAB_HIGHCUT:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    cabHighCutHz(static_cast<float>(value)));
      return buf;
    case P_LIMITER_THRESH:
      std::snprintf(buf, sizeof(buf), "%.1f dB", -24.0 + 24.0 * value);
      return buf;
    case P_NAM_LEVEL:
      std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0 + 24.0 * value);
      return buf;
    case P_GATE_THRESH:
      std::snprintf(buf, sizeof(buf), "%.0f dBFS", -80.0 + 60.0 * value);
      return buf;
    case P_GATE_DECAY:
      std::snprintf(buf, sizeof(buf), "%.0f ms", 20.0 + 980.0 * value);
      return buf;
    case P_NAM_MODEL:
    case P_USER_IR_0:
    case P_USER_IR_1:
    case P_USER_IR_2:
    case P_USER_IR_3:
    case P_OUT_EQ_LOW:
    case P_OUT_EQ_LOW_MID:
    case P_OUT_EQ_HIGH_MID:
    case P_OUT_EQ_HIGH:
      std::snprintf(buf, sizeof(buf), "%+.1f dB", (value - 0.5) * 24.0);
      return buf;
    case P_OUT_HPF:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    outHpfHz(static_cast<float>(value)));
      return buf;
    case P_OUT_LPF:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    outLpfHz(static_cast<float>(value)));
      return buf;
    default:
      break;
  }
  if (s.toggled) {
    return value > 0.5 ? "On" : "Off";
  }
  if (s.min >= 0.0 && s.max <= 1.0) {
    // Continuous panel knobs read on the printed 0..10 scale.
    std::snprintf(buf, sizeof(buf), "%.1f", value * 10.0);
    return buf;
  }
  std::snprintf(buf, sizeof(buf), "%.0f", value);
  return buf;
}

inline std::string formatEditValue(const Shared& shared, uint32_t id,
                                   double value) {
  return formatParamValue(shared, id, value);
}

inline bool parseParamValue(const Shared&, uint32_t id,
                            const std::string& raw, double* out) {
  const ParamSpec& s = kParamSpecs[id];
  const std::string text = lowerText(trimText(raw));
  if (text.empty()) {
    return false;
  }
  for (int i = 0; i < s.labels.count; ++i) {
    if (text == lowerText(s.labels.names[i])) {
      *out = s.min + i;
      return true;
    }
  }
  if (text == "on" || text == "true" || text == "yes") {
    *out = 1.0;
    return true;
  }
  if (text == "off" || text == "false" || text == "no") {
    *out = 0.0;
    return true;
  }
  if (text == "none") {
    *out = 0.0;
    return true;
  }

  char* end = nullptr;
  const double numeric = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) {
    return false;
  }
  const std::string suffix = trimText(std::string(end));
  double value = numeric;
  if (suffix == "db") {
    switch (id) {
      case P_INPUT:
      case P_OUTPUT: value = numeric / 24.0 + 0.5; break;
      case P_MASTER: value = (numeric + 18.0) / 18.0; break;
      case P_CAB_LEVEL: value = (numeric + 24.0) / 30.0; break;
      case P_LIMITER_THRESH: value = (numeric + 24.0) / 24.0; break;
      case P_NAM_LEVEL: value = (numeric + 12.0) / 24.0; break;
      case P_GATE_THRESH: value = (numeric + 80.0) / 60.0; break;
      default: break;
    }
  } else if (suffix == "dbfs") {
    if (id == P_GATE_THRESH) {
      value = (numeric + 80.0) / 60.0;
    }
  } else if (suffix == "hz") {
    if (id == P_OUT_HPF) {
      value = std::log(std::max(numeric, 1.0) / 20.0) / std::log(15.0);
    } else if (id == P_OUT_LPF) {
      value = std::log(std::max(numeric, 1.0) / 2000.0) / std::log(10.0);
    } else if (id == P_CAB_LOWCUT) {
      value = std::log(std::max(numeric, 1.0) / 20.0) / std::log(15.0);
    } else if (id == P_CAB_HIGHCUT) {
      value = std::log(std::max(numeric, 1.0) / 2000.0) / std::log(10.0);
    }
  } else if (suffix == "ms") {
    switch (id) {
      case P_GATE_DECAY: value = (numeric - 20.0) / 980.0; break;
      default: break;
    }
  } else if (suffix == "s" || suffix == "sec") {
    if (id == P_GATE_DECAY) {
      value = (numeric * 1000.0 - 20.0) / 980.0;
    }
  } else if (suffix == "%" || suffix == "pct") {
    if (s.max <= 1.0) {
      value = numeric / 100.0;
    }
  } else if (suffix.empty()) {
    switch (id) {
      case P_INPUT:
      case P_OUTPUT: value = numeric / 24.0 + 0.5; break;
      case P_MASTER: value = (numeric + 18.0) / 18.0; break;
      case P_CAB_LEVEL: value = (numeric + 24.0) / 30.0; break;
      case P_LIMITER_THRESH: value = (numeric + 24.0) / 24.0; break;
      case P_NAM_LEVEL: value = (numeric + 12.0) / 24.0; break;
      case P_GATE_THRESH: value = (numeric + 80.0) / 60.0; break;
      case P_GATE_DECAY: value = (numeric - 20.0) / 980.0; break;
      case P_CAB_LOWCUT:
        value = std::log(std::max(numeric, 1.0) / 20.0) / std::log(15.0);
        break;
      case P_CAB_HIGHCUT:
        value = std::log(std::max(numeric, 1.0) / 2000.0) / std::log(10.0);
        break;
      default:
        if (s.min >= 0.0 && s.max <= 1.0) {
          value = numeric <= 10.0 ? numeric / 10.0 : numeric / 100.0;
        }
        break;
    }
  }
  value = std::min(std::max(value, s.min), s.max);
  if (s.stepped) {
    value = std::round(value);
  }
  *out = value;
  return true;
}

} // namespace bass
