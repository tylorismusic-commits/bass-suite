// Bass Suite — Neural Amp Modeler (.nam) loader
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wraps the MIT-licensed NeuralAmpModelerCore and adds real-time sample-rate
// conversion around it.  Models are loaded on a worker thread; the audio thread
// only sees an immutable shared_ptr and processes into preallocated buffers.

#pragma once

#include <samplerate.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// X11's X.h defines Success, which Eigen treats as a hard error.  The CLAP
// wrapper includes X11 before this header.
#ifdef Success
#undef Success
#endif

#include "get_dsp.h"

namespace bass {

class NamDSP {
public:
  NamDSP() = default;
  ~NamDSP() {
    if (inSrc_) src_delete(inSrc_);
    if (outSrc_) src_delete(outSrc_);
  }
  NamDSP(const NamDSP&) = delete;
  NamDSP& operator=(const NamDSP&) = delete;

  bool load(const std::string& path, double hostRate, int maxBlock) {
    try {
      nam::DspLoadOptions opts;
      opts.prewarm = false;
      auto dsp = nam::get_dsp(std::filesystem::path(path), opts);
      if (!dsp) {
        return false;
      }
      hostRate_ = hostRate > 1000.0 ? hostRate : 48000.0;
      maxBlock_ = std::max(32, maxBlock);
      modelRate_ = dsp->GetExpectedSampleRate();
      const bool arbitrary = dsp->SupportsArbitrarySampleRate();
      if (arbitrary || modelRate_ <= 0.0 ||
          std::fabs(modelRate_ - hostRate_) < 0.5) {
        modelRate_ = hostRate_;
        maxModelBlock_ = maxBlock_;
        resample_ = false;
        dsp->SetPrewarmOnReset(true);
        dsp->Reset(hostRate_, maxBlock_);
      } else {
        maxModelBlock_ =
            static_cast<int>(std::ceil(maxBlock_ * modelRate_ / hostRate_)) +
            256;
        resample_ = true;
        dsp->SetPrewarmOnReset(true);
        dsp->Reset(modelRate_, maxModelBlock_);
        int err = 0;
        inSrc_ = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &err);
        outSrc_ = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &err);
        if (!inSrc_ || !outSrc_) {
          if (inSrc_) src_delete(inSrc_);
          if (outSrc_) src_delete(outSrc_);
          inSrc_ = outSrc_ = nullptr;
          return false;
        }
      }
      dsp_ = std::move(dsp);
      name_ = std::filesystem::path(path).filename().string();
      outputGain_ = 1.0f;
      if (dsp_->HasLoudness()) {
        float db = -18.0f - static_cast<float>(dsp_->GetLoudness());
        db = std::min(std::max(db, -24.0f), 24.0f);
        outputGain_ = std::pow(10.0f, db / 20.0f);
      }
      allocate();
      reset();
      return true;
    } catch (...) {
      dsp_.reset();
      return false;
    }
  }

  bool valid() const { return static_cast<bool>(dsp_); }
  const std::string& name() const { return name_; }
  double modelRate() const { return modelRate_; }

  void reset() {
    if (inSrc_) src_reset(inSrc_);
    if (outSrc_) src_reset(outSrc_);
    fifo_.clear();
    std::fill(mono_.begin(), mono_.end(), 0.0f);
    std::fill(modelIn_.begin(), modelIn_.end(), 0.0f);
    std::fill(modelOut_.begin(), modelOut_.end(), 0.0f);
    std::fill(hostOut_.begin(), hostOut_.end(), 0.0f);
    std::fill(namIn_.begin(), namIn_.end(), NAM_SAMPLE(0));
    std::fill(namOut_.begin(), namOut_.end(), NAM_SAMPLE(0));
  }

  void process(float* left, float* right, int frames) {
    if (!dsp_ || frames <= 0) {
      return;
    }
    if (frames > maxBlock_) {
      for (int off = 0; off < frames; off += maxBlock_) {
        process(left + off, right + off,
                std::min(maxBlock_, frames - off));
      }
      return;
    }
    if (!resample_) {
      for (int i = 0; i < frames; ++i) {
        namIn_[static_cast<size_t>(i)] =
            0.5f * (left[i] + right[i]);
      }
      inPtrs_[0] = namIn_.data();
      outPtrs_[0] = namOut_.data();
      dsp_->process(inPtrs_.data(), outPtrs_.data(), frames);
      for (int i = 0; i < frames; ++i) {
        const float v = static_cast<float>(namOut_[static_cast<size_t>(i)]) *
                        outputGain_;
        left[i] = right[i] = v;
      }
      return;
    }

    for (int i = 0; i < frames; ++i) {
      mono_[static_cast<size_t>(i)] = 0.5f * (left[i] + right[i]);
    }
    SRC_DATA in{};
    in.data_in = mono_.data();
    in.input_frames = frames;
    in.data_out = modelIn_.data();
    in.output_frames = static_cast<long>(modelIn_.size());
    in.src_ratio = modelRate_ / hostRate_;
    in.end_of_input = 0;
    if (src_process(inSrc_, &in) != 0) {
      return;
    }
    const int modelFrames = static_cast<int>(in.output_frames_gen);
    if (modelFrames <= 0) {
      return;
    }
    int off = 0;
    while (off < modelFrames) {
      const int n = std::min(maxModelBlock_, modelFrames - off);
      for (int i = 0; i < n; ++i) {
        namIn_[static_cast<size_t>(i)] =
            modelIn_[static_cast<size_t>(off + i)];
      }
      inPtrs_[0] = namIn_.data();
      outPtrs_[0] = namOut_.data();
      dsp_->process(inPtrs_.data(), outPtrs_.data(), n);
      for (int i = 0; i < n; ++i) {
        modelOut_[static_cast<size_t>(off + i)] =
            static_cast<float>(namOut_[static_cast<size_t>(i)]);
      }
      off += n;
    }

    SRC_DATA out{};
    out.data_in = modelOut_.data();
    out.input_frames = modelFrames;
    out.data_out = hostOut_.data();
    out.output_frames = static_cast<long>(hostOut_.size());
    out.src_ratio = hostRate_ / modelRate_;
    out.end_of_input = 0;
    if (src_process(outSrc_, &out) != 0) {
      return;
    }
    const int hostFrames = static_cast<int>(out.output_frames_gen);
    fifo_.insert(fifo_.end(), hostOut_.begin(), hostOut_.begin() + hostFrames);
    if (fifo_.size() > static_cast<size_t>(frames) + 4096) {
      const size_t drop = fifo_.size() - static_cast<size_t>(frames) - 2048;
      fifo_.erase(fifo_.begin(), fifo_.begin() + static_cast<long>(drop));
    }
    const size_t n =
        std::min(static_cast<size_t>(frames), fifo_.size());
    for (size_t i = 0; i < n; ++i) {
      const float v = fifo_[i] * outputGain_;
      left[i] = right[i] = v;
    }
    for (size_t i = n; i < static_cast<size_t>(frames); ++i) {
      left[i] = right[i] = 0.0f;
    }
    fifo_.erase(fifo_.begin(), fifo_.begin() + static_cast<long>(n));
  }

private:
  void allocate() {
    mono_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    modelIn_.assign(static_cast<size_t>(maxModelBlock_), 0.0f);
    modelOut_.assign(static_cast<size_t>(maxModelBlock_), 0.0f);
    hostOut_.assign(static_cast<size_t>(maxBlock_) + 512, 0.0f);
    namIn_.assign(static_cast<size_t>(maxModelBlock_), NAM_SAMPLE(0));
    namOut_.assign(static_cast<size_t>(maxModelBlock_), NAM_SAMPLE(0));
    fifo_.clear();
    fifo_.reserve(static_cast<size_t>(maxBlock_) * 3 + 16384);
    inPtrs_.assign(1, nullptr);
    outPtrs_.assign(1, nullptr);
  }

  std::unique_ptr<nam::DSP> dsp_;
  std::string name_;
  double hostRate_ = 48000.0;
  double modelRate_ = 48000.0;
  int maxBlock_ = 4096;
  int maxModelBlock_ = 4096;
  bool resample_ = false;
  float outputGain_ = 1.0f;
  SRC_STATE* inSrc_ = nullptr;
  SRC_STATE* outSrc_ = nullptr;
  std::vector<float> mono_;
  std::vector<float> modelIn_;
  std::vector<float> modelOut_;
  std::vector<float> hostOut_;
  std::vector<float> fifo_;
  std::vector<NAM_SAMPLE> namIn_;
  std::vector<NAM_SAMPLE> namOut_;
  std::vector<NAM_SAMPLE*> inPtrs_;
  std::vector<NAM_SAMPLE*> outPtrs_;
};

} // namespace bass
