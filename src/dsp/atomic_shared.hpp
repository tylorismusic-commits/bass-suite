// Bass Suite — portable shared_ptr slot
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace bass {

// std::atomic<std::shared_ptr<T>> is not provided by libc++ (the macOS
// toolchain rejects it: "std::atomic<T> requires that T be trivially
// copyable").  A mutex-protected slot gives the same load/store surface.
template <typename T>
class AtomicSharedPtr {
public:
  std::shared_ptr<T> load() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
  }

  void store(std::shared_ptr<T> value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = std::move(value);
  }

private:
  mutable std::mutex mutex_;
  std::shared_ptr<T> value_;
};

} // namespace bass
