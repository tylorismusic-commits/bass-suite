// Bass Suite — NAM model library and background loader
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "nam_files.hpp"
#include "nam_model.hpp"

namespace bass {

class NamLibrary {
public:
  NamLibrary() = default;
  ~NamLibrary() { stop(); }
  NamLibrary(const NamLibrary&) = delete;
  NamLibrary& operator=(const NamLibrary&) = delete;

  void start(std::string dir, double hostRate, int maxBlock) {
    stop();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      dir_ = std::move(dir);
    }
    hostRate_.store(hostRate, std::memory_order_relaxed);
    maxBlock_.store(maxBlock, std::memory_order_relaxed);
    requested_.store(0, std::memory_order_relaxed);
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    quit_.store(false, std::memory_order_relaxed);
    fail_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this] { run(); });
  }

  void stop() {
    quit_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) {
      thread_.join();
    }
    std::atomic_store(&model_, std::shared_ptr<NamDSP>());
  }

  void setSelected(int index) {
    requested_.store(index, std::memory_order_relaxed);
  }

  void setHostRate(double rate) {
    if (std::fabs(rate - hostRate_.load(std::memory_order_relaxed)) < 0.5) {
      return;
    }
    hostRate_.store(rate, std::memory_order_relaxed);
    reload_.store(true, std::memory_order_relaxed);
  }

  std::shared_ptr<NamDSP> model() const {
    return std::atomic_load(&model_);
  }

  std::string loadedName() const {
    auto m = model();
    return m ? m->name() : std::string();
  }

  std::string nameForIndex(int index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::vector<std::string> names = listNamFiles(dir_);
    if (index <= 0 || index > static_cast<int>(names.size())) {
      return {};
    }
    return names[static_cast<size_t>(index - 1)];
  }

  bool failed() const { return fail_.load(std::memory_order_relaxed); }

private:
  void run() {
    while (!quit_.load(std::memory_order_relaxed)) {
      for (int i = 0; i < 10 && !quit_.load(std::memory_order_relaxed); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      if (quit_.load(std::memory_order_relaxed)) {
        break;
      }
      loadSelected();
    }
  }

  void loadSelected() {
    std::string dir;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      dir = dir_;
    }
    const std::vector<std::string> names = listNamFiles(dir);
    const int index = requested_.load(std::memory_order_relaxed);
    if (index <= 0 || index > static_cast<int>(names.size())) {
      if (loadedIndex_ != 0) {
        loadedIndex_ = 0;
        loadedPath_.clear();
        fail_.store(false, std::memory_order_relaxed);
        std::atomic_store(&model_, std::shared_ptr<NamDSP>());
      }
      return;
    }
    const std::string path = dir + "/" + names[static_cast<size_t>(index - 1)];
    const double rate = hostRate_.load(std::memory_order_relaxed);
    const int block = maxBlock_.load(std::memory_order_relaxed);
    const bool force = reload_.exchange(false, std::memory_order_relaxed);
    if (!force && index == loadedIndex_ && path == loadedPath_ &&
        std::fabs(rate - loadedRate_) < 0.5 && block == loadedBlock_ &&
        std::atomic_load(&model_)) {
      return;
    }
    auto next = std::make_shared<NamDSP>();
    if (next->load(path, rate, block)) {
      std::atomic_store(&model_, std::shared_ptr<NamDSP>(next));
      fail_.store(false, std::memory_order_relaxed);
    } else {
      std::atomic_store(&model_, std::shared_ptr<NamDSP>());
      fail_.store(true, std::memory_order_relaxed);
    }
    loadedIndex_ = index;
    loadedPath_ = path;
    loadedRate_ = rate;
    loadedBlock_ = block;
  }

  mutable std::mutex mutex_;
  std::string dir_;
  std::atomic<double> hostRate_{48000.0};
  std::atomic<int> maxBlock_{4096};
  std::atomic<int> requested_{0};
  std::atomic<bool> quit_{true};
  std::atomic<bool> reload_{false};
  std::atomic<bool> fail_{false};
  std::thread thread_;
  std::shared_ptr<NamDSP> model_;
  int loadedIndex_ = -1;
  std::string loadedPath_;
  double loadedRate_ = 0.0;
  int loadedBlock_ = 0;
};

} // namespace bass
