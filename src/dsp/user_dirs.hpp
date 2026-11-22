// Per-user data locations for NAM captures and impulse responses.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// macOS uses ~/Library/Application Support, Linux the XDG data directory.  A
// setup that already keeps files in the old ~/.local/share location on macOS
// keeps using it, so nothing disappears after the move.
#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace suite {

inline std::string dataDir(const char* product, const char* legacy) {
#ifdef __APPLE__
  const char* home = std::getenv("HOME");
  if (home && *home) {
    const std::string modern =
        std::string(home) + "/Library/Application Support/" + product;
    const std::string old = std::string(home) + "/.local/share/" + legacy;
    std::error_code ec;
    if (!std::filesystem::exists(modern, ec) &&
        std::filesystem::exists(old, ec)) {
      return old;
    }
    return modern;
  }
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && *xdg) {
    return std::string(xdg) + "/" + legacy;
  }
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/.local/share/" + legacy;
  }
#endif
  return legacy;
}

// Creates the directories a plug-in writes into, so a fresh install has a
// folder to drop captures and IRs into.
inline void ensureDirs(const std::string& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
}

} // namespace suite
