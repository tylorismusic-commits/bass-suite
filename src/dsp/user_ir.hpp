// Bass Suite — user impulse-response library
// SPDX-License-Identifier: GPL-3.0-or-later
//
// User IRs are ordinary WAV files in a per-user directory.  A small watcher
// thread keeps the loaded bank in sync with the directory, so both the plugin
// and the UI see the same sorted list without doing file I/O on the audio
// thread.

#pragma once

#include "dsp/atomic_shared.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cab_convolver.hpp"

namespace bass {

constexpr int kUserIrMax = 63;
constexpr int kAmpCount = 4;

struct UserIrEntry {
  std::string name;
  std::string path;
  std::vector<unsigned char> bytes;
  CabIR ir;
  std::filesystem::file_time_type mtime{};
  std::uintmax_t size = 0;
  int sampleRate = 0;
};

struct UserIrBank {
  std::vector<UserIrEntry> entries;
};

inline std::string userIrDir() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && *xdg) {
    return std::string(xdg) + "/bass-suite/irs/user";
  }
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/.local/share/bass-suite/irs/user";
  }
  return "bass-suite/irs/user";
}

inline bool isWavName(const std::string& name) {
  if (name.size() < 4) {
    return false;
  }
  const std::string ext = name.substr(name.size() - 4);
  char a = ext[0], b = ext[1], c = ext[2], d = ext[3];
  auto lower = [](char ch) {
    return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
  };
  return lower(a) == '.' && lower(b) == 'w' && lower(c) == 'a' &&
         lower(d) == 'v';
}

// Sorted by filename so the UI list and the parameter indices agree.
inline std::vector<std::string> listUserIrFiles(const std::string& dir) {
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
    if (isWavName(name)) {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());
  if (static_cast<int>(names.size()) > kUserIrMax) {
    names.resize(static_cast<size_t>(kUserIrMax));
  }
  return names;
}

class UserIrLibrary {
public:
  UserIrLibrary() = default;
  ~UserIrLibrary() { stop(); }
  UserIrLibrary(const UserIrLibrary&) = delete;
  UserIrLibrary& operator=(const UserIrLibrary&) = delete;

  void start(std::string dir, float sampleRate) {
    stop();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      dir_ = std::move(dir);
      sr_ = sampleRate;
    }
    if (!dir_.empty()) {
      std::error_code ec;
      std::filesystem::create_directories(dir_, ec);
    }
    rescanNow();
    quit_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this] { run(); });
  }

  void stop() {
    quit_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void setSampleRate(float sampleRate) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (std::fabs(sampleRate - sr_) < 0.5f) {
        return;
      }
      sr_ = sampleRate;
      stamps_.clear();
    }
    rescanNow();
  }

  std::shared_ptr<const UserIrBank> bank() const {
    return bank_.load();
  }

  std::vector<std::string> names() const {
    std::vector<std::string> out;
    const auto b = bank();
    if (b) {
      out.reserve(b->entries.size());
      for (const auto& e : b->entries) {
        out.push_back(e.name);
      }
    }
    return out;
  }

  std::string nameForIndex(int index) const {
    const auto b = bank();
    if (!b || index <= 0 ||
        index > static_cast<int>(b->entries.size())) {
      return {};
    }
    return b->entries[static_cast<size_t>(index - 1)].name;
  }

  bool bytesForIndex(int index, std::string& name,
                     std::vector<unsigned char>& out) const {
    const auto b = bank();
    if (!b || index <= 0 ||
        index > static_cast<int>(b->entries.size())) {
      return false;
    }
    const UserIrEntry& e = b->entries[static_cast<size_t>(index - 1)];
    if (e.bytes.empty()) {
      return false;
    }
    name = e.name;
    out = e.bytes;
    return true;
  }

  // Writes an embedded IR into the user directory and returns its 1-based
  // selection index after rescanning.
  int importWav(const std::string& name,
                const std::vector<unsigned char>& bytes, int amp) {
    if (bytes.empty()) {
      return 0;
    }
    std::string safe;
    for (char ch : name) {
      const unsigned char u = static_cast<unsigned char>(ch);
      if (std::isalnum(u) || ch == '-' || ch == '_' || ch == '.') {
        safe += ch;
      } else {
        safe += '-';
      }
    }
    if (safe.empty()) {
      safe = "imported.wav";
    }
    if (!isWavName(safe)) {
      safe += ".wav";
    }
    safe = "imported-" + std::to_string(std::min(std::max(amp, 0), kAmpCount - 1)) +
           "-" + safe;

    std::string dir;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      dir = dir_;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = dir + "/" + safe;
    {
      std::ofstream out(path, std::ios::binary | std::ios::trunc);
      if (!out) {
        return 0;
      }
      out.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
      if (!out) {
        return 0;
      }
    }
    rescanNow();
    const std::vector<std::string> list = names();
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i] == safe) {
        return static_cast<int>(i) + 1;
      }
    }
    return 0;
  }

  void rescanNow() {
    std::lock_guard<std::mutex> work(workMutex_);
    std::string dir;
    float sr = 48000.0f;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      dir = dir_;
      sr = sr_;
    }
    if (dir.empty()) {
      return;
    }
    const std::vector<std::string> names = listUserIrFiles(dir);
    const auto old = bank_.load();

    auto bank = std::make_shared<UserIrBank>();
    bank->entries.reserve(names.size());
    std::vector<Stamp> stamps;
    stamps.reserve(names.size());

    for (const std::string& name : names) {
      const std::string path = dir + "/" + name;
      std::error_code ec;
      const auto mtime = std::filesystem::last_write_time(path, ec);
      const auto size = std::filesystem::file_size(path, ec);
      if (ec) {
        continue;
      }
      stamps.push_back(Stamp{path, mtime, size});

      const UserIrEntry* previous = nullptr;
      if (old) {
        for (const auto& e : old->entries) {
          if (e.path == path) {
            previous = &e;
            break;
          }
        }
      }
      const int rate = static_cast<int>(std::lround(sr));
      const bool same = previous && previous->mtime == mtime &&
                        previous->size == size &&
                        previous->sampleRate == rate;
      if (same) {
        UserIrEntry e = *previous;
        bank->entries.push_back(std::move(e));
        continue;
      }

      UserIrEntry e;
      e.name = name;
      e.path = path;
      e.mtime = mtime;
      e.size = size;
      e.sampleRate = rate;
      // Keep one entry per listed filename, even if the WAV is malformed, so
      // the UI list and the parameter indices stay aligned.
      readFileBytes(path, e.bytes);
      if (readWavMono(path, e.ir.h, static_cast<int>(std::lround(sr)))) {
        if (e.ir.h.size() > 4096) {
          e.ir.h.resize(4096);
        }
        if (!e.ir.h.empty()) {
          normalizeIrAt1k(e.ir.h, static_cast<int>(std::lround(sr)));
        }
      } else {
        e.ir.h.clear();
      }
      bank->entries.push_back(std::move(e));
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stamps_ = std::move(stamps);
    }
    bank_.store(std::shared_ptr<const UserIrBank>(bank));
  }

private:
  struct Stamp {
    std::string path;
    std::filesystem::file_time_type mtime{};
    std::uintmax_t size = 0;
  };

  static bool readFileBytes(const std::string& path,
                            std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
      return false;
    }
    const std::streamoff size = in.tellg();
    if (size <= 0) {
      return false;
    }
    out.resize(static_cast<size_t>(size));
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(in);
  }

  void run() {
    while (!quit_.load(std::memory_order_relaxed)) {
      for (int i = 0; i < 20 && !quit_.load(std::memory_order_relaxed); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      if (!quit_.load(std::memory_order_relaxed)) {
        rescanNow();
      }
    }
  }

  mutable std::mutex mutex_;
  std::mutex workMutex_;
  std::string dir_;
  float sr_ = 48000.0f;
  std::thread thread_;
  std::atomic<bool> quit_{true};
  AtomicSharedPtr<const UserIrBank> bank_;
  std::vector<Stamp> stamps_;
};

} // namespace bass
