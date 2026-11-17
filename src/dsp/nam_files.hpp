// Bass Suite — NAM folder helpers (UI-safe, no Eigen/Nam DSP dependency)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace bass {

inline std::string userNamDir() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && *xdg) {
    return std::string(xdg) + "/bass-suite/nam";
  }
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/.local/share/bass-suite/nam";
  }
  return "bass-suite/nam";
}

inline bool isNamName(const std::string& name) {
  auto ext = std::filesystem::path(name).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext == ".nam" || ext == ".wav";
}

inline std::vector<std::string> listNamFiles(const std::string& dir) {
  std::vector<std::string> names;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    return names;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    const std::string name = entry.path().filename().string();
    if (isNamName(name)) {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());
  if (names.size() > 63) {
    names.resize(63);
  }
  return names;
}

} // namespace bass
