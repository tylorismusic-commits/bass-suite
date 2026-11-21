// Bass Suite — engine.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// First-pass recipe engine.  The preamp uses Guitarix-derived tube stages and
// tone stacks; the power amp is a simple push-pull saturation for now.  M1
// replaces the power amp with the Plexi/sag/NFB model.

#pragma once

#include "dsp/atomic_shared.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

#include "biquad.hpp"
#include "cab_convolver.hpp"
#include "comp_pedal.hpp"
#include "octave.hpp"
#include "common.hpp"
#include "nam_model.hpp"
#include "params.hpp"
#include "oversample.hpp"
#include "power_amp.hpp"
#include "tuner.hpp"
#include "ts9.hpp"
#include "recipe.hpp"
#include "user_ir.hpp"

namespace bass {

class Engine {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate;
    prepareRecipe(static_cast<int>(Model::SVT));
    inputSmooth_.setTime(20.0f, sr_);
    outputSmooth_.setTime(20.0f, sr_);
    inputSmooth_.reset(0.5f);
    outputSmooth_.reset(0.5f);
    odL_.prepare(sr_);
    odR_.prepare(sr_);
    comp_.prepare(sr_);
    oct_.prepare(sr_);
    tuner_.prepare(sr_);
    cabConvolver_.prepare(4096);
    dcPreL_.set(20.0f, sr_);
    dcPreR_.set(20.0f, sr_);
    dcPostL_.set(20.0f, sr_);
    dcPostR_.set(20.0f, sr_);
    cabHpL_.set(70.0f, sr_);
    cabHpR_.set(70.0f, sr_);
    cabLpL_.lowPass(6500.0f, 0.707f, sr_);
    cabLpR_.lowPass(6500.0f, 0.707f, sr_);
    vuCoeff_ = timeCoeff(300.0f, sr_);
    vuPower_ = 0.0f;
    limiterAttack_ = timeCoeff(0.5f, sr_);
    limiterRelease_ = timeCoeff(80.0f, sr_);
    limiterGain_ = 1.0f;
    gateAttack_ = timeCoeff(1.0f, sr_);
    gateEnvRelease_ = timeCoeff(80.0f, sr_);
    gateRelease_ = timeCoeff(400.0f, sr_);
    gateThreshold_ = dbToGain(-47.0f);
    gateGain_ = 1.0f;
    gateEnv_ = 0.0f;
    compAttack_ = timeCoeff(5.0f, sr_);
    compRelease_ = timeCoeff(300.0f, sr_);
    compGain_ = 1.0f;
    compEnv_ = 0.0f;
    compOn_ = false;
    // prepare() clears the convolver, so force the next updateCabIR() to
    // install the current IR again.  Without this, activate() after the
    // constructor's first setControls() leaves the cab convolver empty
    // until a model switch changes the IR pointer.
    activeCab_ = nullptr;
    lastModel_ = -1;
    powerOs_ = 0;
    pendingModel_ = -1;
    pendingControls_ = Controls{};
    fadeDir_ = 0;
    switchReady_ = false;
    switchHold_ = 0;
    switchGain_ = 1.0f;
    switchFadeSamples_ = std::max(1.0f, sr_ * 0.015f);
    switchStep_ = 1.0f / switchFadeSamples_;
    switchInStep_ = 1.0f / std::max(1.0f, sr_ * 0.040f);
    switchHoldSamples_ = std::max(1, static_cast<int>(sr_ * 0.020f));
  }

  void setControls(const Controls& c) noexcept {
    const bool firstApply = lastModel_ < 0;
    if (lastModel_ < 0) {
      lastModel_ = c.model;
      prepareRecipe(c.model);
      fadeDir_ = 0;
    }
    if (!c.enabled) {
      // Power off is an immediate true bypass.  Do not wait for a pending
      // model fade to finish; cancel it and let the next powered-on call
      // request the model again.
      controls_.enabled = false;
      pendingModel_ = -1;
      switchReady_ = false;
      switchHold_ = 0;
      fadeDir_ = 0;
      switchGain_ = 1.0f;
      return;
    }
    // Make sure the audio path is live before any pending model fade runs;
    // otherwise the fade would stall behind the bypass check.
    controls_.enabled = true;
    if (c.namOn) {
      // The NAM path replaces the amp, so a model change cannot click.  Apply
      // it directly; otherwise a pending fade would never run and control
      // updates would stall.
      lastModel_ = c.model;
      prepareRecipe(c.model);
      controls_ = c;
      applyControls(c);
      pendingModel_ = -1;
      switchReady_ = false;
      switchHold_ = 0;
      fadeDir_ = 0;
      switchGain_ = 1.0f;
      return;
    }
    if (switchReady_) {
      lastModel_ = pendingModel_;
      prepareRecipe(pendingModel_);
      controls_ = pendingControls_;
      applyControls(controls_);
      pendingModel_ = -1;
      switchReady_ = false;
      fadeDir_ = 1;
      return;
    }
    const bool hardChanged =
        c.model != lastModel_ || c.ampBypass != controls_.ampBypass ||
        c.cab != controls_.cab || c.cabMic != controls_.cabMic ||
        c.cabBypass != controls_.cabBypass ||
        std::memcmp(c.userIr, controls_.userIr, sizeof(c.userIr)) != 0;
    if (!firstApply && hardChanged) {
      pendingModel_ = c.model;
      pendingControls_ = c;
      // Start a new mute only when idle.  During the fade-down, hold or
      // fade-in, just keep the latest requested set so the switch can finish;
      // otherwise setControls() would restart the fade every block and the
      // silent hold would never complete.
      if (fadeDir_ == 0 && switchHold_ == 0) {
        fadeDir_ = -1;
        switchGain_ = 1.0f;
      }
      // Keep the current recipe/IR while fading down; the pending set is
      // applied during the silent hold, then the output fades back in.
      return;
    }
    controls_ = c;
    applyControls(c);
  }

  void applyControls(const Controls& c) noexcept {
    const AmpRecipe& r = recipeFor(c.model);
    // The Gain knob is a preamp-drive range around a calibrated pivot.  The
    // high-gain models get a wider range and a lower pivot so they snarl
    // around 6 and are fully saturated by 10 with active pickups.
    float gainSpan = 36.0f;
    float gainPivot = 0.5f;
    if (c.model == 3) {
      gainSpan = 48.0f;
      gainPivot = 0.55f;
    } else if (c.model == 2) {
      gainSpan = 42.0f;
    } else if (c.model == 1) {
      gainSpan = 32.0f;
    }
    preGain_ = r.preGain * dbToGain((c.gain - gainPivot) * gainSpan);
    PowerAmpSpec ps;
    ps.drive = r.powerDrive;
    ps.feedback = r.powerFeedback;
    ps.sag = r.powerSag;
    ps.output = r.outputTrim;
    ps.useTable = r.powerTable;
    const int os = 4;
    if (os != powerOs_) {
      powerL_.prepare(sr_ * static_cast<float>(os), ps);
      powerR_.prepare(sr_ * static_cast<float>(os), ps);
      os1L_.prepare();
      os1R_.prepare();
      os2L_.prepare();
      os2R_.prepare();
      powerOs_ = os;
    } else {
      powerL_.setSpec(ps);
      powerR_.setSpec(ps);
    }
    toneSpec_ = toneStackSpec(r.tone);
    toneL_.set(toneSpec_, c.bass, c.mid, c.treble, sr_);
    toneR_.set(toneSpec_, c.bass, c.mid, c.treble, sr_);
    powerL_.setPresence(c.presence);
    powerR_.setPresence(c.presence);
    powerL_.setResonance(c.resonance);
    powerR_.setResonance(c.resonance);
    const float preBrightDb = c.model == 3 ? 2.0f : 0.0f;
    preBrightL_.highShelf(1600.0f, preBrightDb, 0.7f, sr_);
    preBrightR_.highShelf(1600.0f, preBrightDb, 0.7f, sr_);
    // Tighten the low end going into the high-gain cascade and add a little
    // 1.2 kHz focus.  This is the "tight" high-pass a 5150-style preamp has:
    // it keeps the low string from dominating the clipper and lets the upper
    // harmonics carry the aggression.
    // Only the three high-gain models get this voicing; the clean, Vox,
    // Orange and Marshall models keep their existing balance.
    const bool highGainModel = c.model == 3;
    const float tightHz = highGainModel ? 35.0f : 20.0f;
    const float preFocusDb = highGainModel ? 1.5f : 0.0f;
    float postBodyHz = 250.0f;
    float postBodyDb = 0.0f;
    float postFocusHz = 900.0f;
    float postFocusDb = 0.0f;
    float airHz = 5000.0f;
    float airDb = 0.0f;
    if (c.model == 0) {
      // Studio DI: clean and open.
      postBodyDb = 0.0f;
      postFocusDb = 0.0f;
      airDb = 0.0f;
    } else if (c.model == 1) {
      // Flip Top: warm low-mid, soft top.
      postBodyHz = 220.0f;
      postBodyDb = 1.5f;
      postFocusHz = 800.0f;
      postFocusDb = 1.0f;
      airHz = 4500.0f;
      airDb = 0.0f;
    } else if (c.model == 2) {
      // SVT: big low end, firm 900 Hz.
      postBodyHz = 120.0f;
      postBodyDb = 2.0f;
      postFocusHz = 900.0f;
      postFocusDb = 1.0f;
      airHz = 5000.0f;
      airDb = 1.0f;
    } else {
      // Modern: aggressive upper mids.
      postBodyHz = 200.0f;
      postBodyDb = 0.0f;
      postFocusHz = 1800.0f;
      postFocusDb = 2.5f;
      airHz = 5000.0f;
      airDb = 2.0f;
    }
    preTightL_.set(tightHz, sr_);
    preTightR_.set(tightHz, sr_);
    preFocusL_.peak(1200.0f, preFocusDb, 0.8f, sr_);
    preFocusR_.peak(1200.0f, preFocusDb, 0.8f, sr_);
    postBodyL_.peak(postBodyHz, postBodyDb, 0.9f, sr_);
    postBodyR_.peak(postBodyHz, postBodyDb, 0.9f, sr_);
    postFocusL_.peak(postFocusHz, postFocusDb, 0.8f, sr_);
    postFocusR_.peak(postFocusHz, postFocusDb, 0.8f, sr_);
    postAirL_.highShelf(airHz, airDb, 0.7f, sr_);
    postAirR_.highShelf(airHz, airDb, 0.7f, sr_);
    gateThreshold_ = dbToGain(-80.0f + 60.0f * c.gateThreshold);
    gateRelease_ = timeCoeff(20.0f + 980.0f * c.gateDecay, sr_);
    // Sustain compressor.  Rather than switching on at a gain threshold
    // (which made a level step around Gain 5), blend it in smoothly between
    // Gain 5 and 6.  The gain-reduction state keeps running so the blend
    // never jumps when it becomes audible.
    // The dedicated Comp pedal is the bass compressor; the guitar sustain
    // compressor stays off.
    compMix_ = 0.0f;
    compOn_ = false;
    compThreshold_ = dbToGain(-32.0f);
    compMakeup_ = dbToGain(16.0f);
    setPresenceResonance(c);
    inputSmooth_.setTarget(c.input);
    outputSmooth_.setTarget(c.output);
    odL_.setControls(c.odDrive, c.odTone, c.odLevel);
    odR_.setControls(c.odDrive, c.odTone, c.odLevel);
    comp_.setControls(c);
    oct_.setControls(c);
    syncUserIrBank();
    syncNamModel();
    updateCab(c);
    applyOutputEq(c);
    updateCabIR();
  }

  void setUserIrBank(std::shared_ptr<const UserIrBank> bank) noexcept {
    userBankPending_.store(std::move(bank));
  }

  void setNamModel(std::shared_ptr<NamDSP> model) noexcept {
    namPending_.store(std::move(model));
  }

  bool cabReady() const noexcept { return cabConvolver_.active(); }

  const Controls& currentControls() const noexcept { return controls_; }

  // Apply a complete control set immediately, bypassing the single-engine
  // model-switch fade.  The dual-engine wrapper uses this for its standby.
  void configure(const Controls& c) noexcept {
    lastModel_ = c.model;
    prepareRecipe(c.model);
    controls_ = c;
    applyControls(c);
    pendingModel_ = -1;
    switchReady_ = false;
    switchHold_ = 0;
    fadeDir_ = 0;
    switchGain_ = 1.0f;
  }

  bool loadFactoryCabs(const std::string& dir) noexcept {
    const bool ok = cabBank_.load(dir, sr_);
    // The bank may have replaced its IR data; force the convolver to take the
    // new pointers and tap counts even if the CabIR address is unchanged.
    activeCab_ = nullptr;
    updateCabIR();
    return ok;
  }



  void gateSample(float& l, float& r, bool on) noexcept {
    if (!on) {
      gateGain_ = 1.0f;
      gateEnv_ = 0.0f;
      return;
    }
    const float peak = std::max(std::fabs(l), std::fabs(r));
    gateEnv_ = peak > gateEnv_ ? peak : gateEnv_ * gateEnvRelease_;
    const float target = gateEnv_ > gateThreshold_ ? 1.0f : 0.0f;
    const float coeff = target > gateGain_ ? gateAttack_ : gateRelease_;
    gateGain_ = coeff * gateGain_ + (1.0f - coeff) * target;
    l *= gateGain_;
    r *= gateGain_;
  }

  // Power off is a true bypass: the dry input already sits in the output
  // buffers, so leave it untouched and only refresh the meters.  No cab, no
  // tails, no output trim, no host-side enable is consulted.
  void bypass(float* left, float* right, int frames,
              Meters& meters) noexcept {
    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float power = 0.0f;
    for (int i = 0; i < frames; ++i) {
      const float l = std::isfinite(left[i])
                          ? std::clamp(left[i], -32.0f, 32.0f)
                          : 0.0f;
      const float r = std::isfinite(right[i])
                          ? std::clamp(right[i], -32.0f, 32.0f)
                          : 0.0f;
      left[i] = l;
      right[i] = r;
      inPeakL = std::max(inPeakL, std::fabs(l));
      inPeakR = std::max(inPeakR, std::fabs(r));
      power += 0.5f * (l * l + r * r);
    }
    if (frames > 0) {
      power /= static_cast<float>(frames);
    }
    vuPower_ = vuCoeff_ * vuPower_ + (1.0f - vuCoeff_) * power;
    meters.inPeakLDb = gainToDb(inPeakL);
    meters.inPeakRDb = gainToDb(inPeakR);
    meters.inPeakDb = std::max(meters.inPeakLDb, meters.inPeakRDb);
    meters.outPeakLDb = meters.inPeakLDb;
    meters.outPeakRDb = meters.inPeakRDb;
    meters.outPeakDb = meters.inPeakDb;
    meters.outVuDb = gainToDb(std::sqrt(vuPower_)) + 18.0f;
  }

  void processNam(float* left, float* right, int frames,
                  Meters& meters) noexcept {
    const Controls c = controls_;
    const float cabLevel =
        c.cabBypass ? 1.0f : dbToGain(cabLevelDb(c.cabLevel));
    const float namTrim = dbToGain(-12.0f + 24.0f * c.namLevel);
    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;

    // Input trim and 808, then the NAM block.
    for (int i = 0; i < frames; ++i) {
      float dryL = std::isfinite(left[i])
                       ? std::clamp(left[i], -32.0f, 32.0f)
                       : 0.0f;
      float dryR = std::isfinite(right[i])
                       ? std::clamp(right[i], -32.0f, 32.0f)
                       : 0.0f;
      inPeakL = std::max(inPeakL, std::fabs(dryL));
      inPeakR = std::max(inPeakR, std::fabs(dryR));
      // During the silent hold, drive the DSP with zero so the old model's
      // filter/power-amp state can settle before the new recipe is applied.
      if (switchHold_ > 0) {
        dryL = 0.0f;
        dryR = 0.0f;
      }
      gateSample(dryL, dryR, c.gateOn);
      const float in = inputSmooth_.next();
      const float trim = dbToGain((in - 0.5f) * 24.0f);
      float l = dryL * trim;
      float r = dryR * trim;
      if (c.compOn) {
        comp_.process(l, r);
      }
      oct_.process(l, r);
      if (c.odOn) {
        const float preL = l;
        const float preR = r;
        l = odL_.process(l);
        r = odR_.process(r);
        const float mix = std::clamp(c.odMix, 0.0f, 1.0f);
        l = preL + mix * (l - preL);
        r = preR + mix * (r - preR);
      }
      left[i] = l;
      right[i] = r;
    }
    if (activeNam_) {
      activeNam_->process(left, right, frames);
    }
    for (int i = 0; i < frames; ++i) {
      left[i] *= namTrim;
      right[i] *= namTrim;
    }
    for (int i = 0; i < frames; ++i) {
      left[i] = dcPreL_.process(left[i]);
      right[i] = dcPreR_.process(right[i]);
    }

    // Loop, cab, output and limiter remain available around the NAM amp.
    for (int i = 0; i < frames; ++i) {
      float l = left[i];
      float r = right[i];
      l = dcPostL_.process(l);
      r = dcPostR_.process(r);
      if (!c.cabBypass) {
        l = cabHpL_.process(l);
        r = cabHpR_.process(r);
        if (cabConvolver_.active()) {
          cabConvolver_.process(l, r);
          l = cabLpL_.process(l);
          r = cabLpR_.process(r);
        } else {
          l = cabLpL_.process(l);
          r = cabLpR_.process(r);
        }
        l *= cabLevel;
        r *= cabLevel;
      }
      l = outLowL_.process(l);
      r = outLowR_.process(r);
      l = outLowMidL_.process(l);
      r = outLowMidR_.process(r);
      l = outHighMidL_.process(l);
      r = outHighMidR_.process(r);
      l = outHighL_.process(l);
      r = outHighR_.process(r);
      l = outHpL_.process(l);
      r = outHpR_.process(r);
      l = outLpL_.process(l);
      r = outLpR_.process(r);

      const float out = outputSmooth_.next();
      const float outGain = dbToGain((out - 0.5f) * 24.0f);
      l *= outGain;
      r *= outGain;
      if (!std::isfinite(l) || !std::isfinite(r)) {
        l = 0.0f;
        r = 0.0f;
      }
      if (c.limiterOn) {
        const float thresh = dbToGain(-24.0f + 24.0f * c.limiterThreshold);
        const float peakNow = std::max(std::fabs(l), std::fabs(r));
        const float target = peakNow > thresh ? thresh / peakNow : 1.0f;
        const float coeff =
            target < limiterGain_ ? limiterAttack_ : limiterRelease_;
        limiterGain_ = coeff * limiterGain_ + (1.0f - coeff) * target;
        l = std::tanh(l * limiterGain_);
        r = std::tanh(r * limiterGain_);
      }
      left[i] = l;
      right[i] = r;
      outPeakL = std::max(outPeakL, std::fabs(l));
      outPeakR = std::max(outPeakR, std::fabs(r));
      const float power = 0.5f * (l * l + r * r);
      vuPower_ = vuCoeff_ * vuPower_ + (1.0f - vuCoeff_) * power;
    }

    meters.inPeakLDb = gainToDb(inPeakL);
    meters.inPeakRDb = gainToDb(inPeakR);
    meters.inPeakDb = std::max(meters.inPeakLDb, meters.inPeakRDb);
    meters.outPeakLDb = gainToDb(outPeakL);
    meters.outPeakRDb = gainToDb(outPeakR);
    meters.outPeakDb = std::max(meters.outPeakLDb, meters.outPeakRDb);
    meters.outVuDb = gainToDb(std::sqrt(vuPower_)) + 18.0f;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    const Controls c = controls_;
    tuner_.process(left, right, frames, c.tunerOn);
    if (!c.enabled) {
      bypass(left, right, frames, meters);
      publishTuner(meters);
      return;
    }
    if (c.namOn && activeNam_) {
      processNam(left, right, frames, meters);
      publishTuner(meters);
      return;
    }
    const int procModel =
        (fadeDir_ == -1 && lastModel_ >= 0) ? lastModel_ : c.model;
    const AmpRecipe& rec = recipeFor(procModel);
    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    const float master = dbToGain(-18.0f + c.master * 18.0f);
    const float makeup = dbToGain(rec.makeupDb);
    const float cabLevel = c.cabBypass ? 1.0f : dbToGain(cabLevelDb(c.cabLevel));


    for (int i = 0; i < frames; ++i) {
      float dryL = std::isfinite(left[i])
                       ? std::clamp(left[i], -32.0f, 32.0f)
                       : 0.0f;
      float dryR = std::isfinite(right[i])
                       ? std::clamp(right[i], -32.0f, 32.0f)
                       : 0.0f;
      inPeakL = std::max(inPeakL, std::fabs(dryL));
      inPeakR = std::max(inPeakR, std::fabs(dryR));
      // During the silent hold, drive the DSP with zero so the old model's
      // filter/power-amp state can settle before the new recipe is applied.
      if (switchHold_ > 0) {
        dryL = 0.0f;
        dryR = 0.0f;
      }
      gateSample(dryL, dryR, c.gateOn);

      const float in = inputSmooth_.next();
      const float trim = dbToGain((in - 0.5f) * 24.0f);
      float l = dryL * trim;
      float r = dryR * trim;

      if (c.compOn) {
        comp_.process(l, r);
      }
      oct_.process(l, r);

      // Drive: TS9 input network with a bass-safe parallel blend.
      if (c.odOn) {
        const float preL = l;
        const float preR = r;
        l = odL_.process(l);
        r = odR_.process(r);
        const float mix = std::clamp(c.odMix, 0.0f, 1.0f);
        l = preL + mix * (l - preL);
        r = preR + mix * (r - preR);
      }

      if (!c.ampBypass) {
        if (rec.solidState) {
          // JC-style clean path: high headroom, then the tone stack.
          l = std::tanh(l * (0.8f + c.gain * 0.6f) + 0.02f) - 0.02f;
          r = std::tanh(r * (0.8f + c.gain * 0.6f) + 0.02f) - 0.02f;
          l *= rec.preGain;
          r *= rec.preGain;
          l = toneL_.process(l);
          r = toneR_.process(r);
          l *= makeup;
          r *= makeup;
          l *= rec.outputTrim;
          r *= rec.outputTrim;
        } else {
          l = preTightL_.process(l);
          r = preTightR_.process(r);
          l = preFocusL_.process(l);
          r = preFocusR_.process(r);
          l = preBrightL_.process(l);
          r = preBrightR_.process(r);
          l *= preGain_;
          r *= preGain_;
          for (int s = 0; s < rec.stages; ++s) {
            l = stages_[s].process(l);
            r = stages_[s].process(r);
          }
          l = toneL_.process(l);
          r = toneR_.process(r);
          l *= makeup;
          r *= makeup;
          l = postBodyL_.process(l);
          r = postBodyR_.process(r);
          l = postFocusL_.process(l);
          r = postFocusR_.process(r);
        }
      }

      // Remove any DC offset generated by the nonlinear amp stages before
      // the loop and power amp.  The power stage can turn a small DC bias into
      // a large asymmetric offset otherwise.
      l = dcPreL_.process(l);
      r = dcPreR_.process(r);

      // FX loop: authentic default is between the preamp and the power amp.

      if (!c.ampBypass && !rec.solidState) {
        if (powerOs_ == 4) {
          float a0 = 0.0f, a1 = 0.0f;
          os1L_.up(l, a0, a1);
          float p0 = powerL_.process(a0);
          float p1 = powerL_.process(a1);
          float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
          os2L_.up(p0, q0, q1);
          os2L_.up(p1, q2, q3);
          q0 = powerL_.process(q0);
          q1 = powerL_.process(q1);
          q2 = powerL_.process(q2);
          q3 = powerL_.process(q3);
          l = os1L_.down(os2L_.down(q0, q1), os2L_.down(q2, q3));

          os1R_.up(r, a0, a1);
          p0 = powerR_.process(a0);
          p1 = powerR_.process(a1);
          os2R_.up(p0, q0, q1);
          os2R_.up(p1, q2, q3);
          q0 = powerR_.process(q0);
          q1 = powerR_.process(q1);
          q2 = powerR_.process(q2);
          q3 = powerR_.process(q3);
          r = os1R_.down(os2R_.down(q0, q1), os2R_.down(q2, q3));
        } else if (powerOs_ == 2) {
          float a0 = 0.0f, a1 = 0.0f;
          os1L_.up(l, a0, a1);
          a0 = powerL_.process(a0);
          a1 = powerL_.process(a1);
          l = os1L_.down(a0, a1);
          os1R_.up(r, a0, a1);
          a0 = powerR_.process(a0);
          a1 = powerR_.process(a1);
          r = os1R_.down(a0, a1);
        } else {
          l = powerL_.process(l);
          r = powerR_.process(r);
        }
      }

      if (!c.ampBypass) {
        l = presenceL_.process(l);
        r = presenceR_.process(r);
        l = presenceHiL_.process(l);
        r = presenceHiR_.process(r);
        l = resonanceL_.process(l);
        r = resonanceR_.process(r);
        l *= master;
        r *= master;
        l = postAirL_.process(l);
        r = postAirR_.process(r);
      }
      l = dcPostL_.process(l);
      r = dcPostR_.process(r);

      // High-gain sustain: a slow 4:1 compressor with makeup keeps the tail
      // of a decaying note up without gating it.  Clean/mid models bypass it.
      if (compOn_ && !c.ampBypass) {
        const float peak = std::max(std::fabs(l), std::fabs(r));
        const float envCoeff = peak > compEnv_ ? compAttack_ : compRelease_;
        compEnv_ = envCoeff * compEnv_ + (1.0f - envCoeff) * peak;
        float target = 1.0f;
        if (compEnv_ > compThreshold_) {
          target = std::pow(compEnv_ / compThreshold_, -0.75f);
        }
        const float gCoeff = target < compGain_ ? compAttack_ : compRelease_;
        compGain_ = gCoeff * compGain_ + (1.0f - gCoeff) * target;
        const float wet = compGain_ * compMakeup_;
        const float g = compMix_ * wet + (1.0f - compMix_);
        l *= g;
        r *= g;
      }

      // Cab: factory IR convolution when available, fixed speaker filter as
      // a fallback for builds without generated assets.
      if (!c.cabBypass) {
        l = cabHpL_.process(l);
        r = cabHpR_.process(r);
        if (cabConvolver_.active()) {
          cabConvolver_.process(l, r);
          l = cabLpL_.process(l);
          r = cabLpR_.process(r);
        } else {
          l = cabLpL_.process(l);
          r = cabLpR_.process(r);
        }
        l *= cabLevel;
        r *= cabLevel;
      }

      // Post-cab loop option.

      l = outLowL_.process(l);
      r = outLowR_.process(r);
      l = outLowMidL_.process(l);
      r = outLowMidR_.process(r);
      l = outHighMidL_.process(l);
      r = outHighMidR_.process(r);
      l = outHighL_.process(l);
      r = outHighR_.process(r);
      l = outHpL_.process(l);
      r = outHpR_.process(r);
      l = outLpL_.process(l);
      r = outLpR_.process(r);

      const float out = outputSmooth_.next();
      const float outGain = dbToGain((out - 0.5f) * 24.0f);
      l *= outGain;
      r *= outGain;

      // Brief mute when the model or cab/IR changes: 15 ms fade-out, 20 ms
      // zero-input hold for the recipe/IR swap, then a 40 ms fade-in so the
      // new model's level/timbre change does not arrive as a step.
      if (fadeDir_ < 0) {
        switchGain_ -= switchStep_;
        if (switchGain_ <= 0.0f) {
          switchGain_ = 0.0f;
          fadeDir_ = 0;
          switchHold_ = switchHoldSamples_;
        }
      } else if (switchHold_ > 0) {
        --switchHold_;
        if (switchHold_ == 0) {
          switchReady_ = true;
        }
      } else if (fadeDir_ > 0) {
        switchGain_ += switchInStep_;
        if (switchGain_ >= 1.0f) {
          switchGain_ = 1.0f;
          fadeDir_ = 0;
        }
      }
      l *= switchGain_;
      r *= switchGain_;

      if (!std::isfinite(l) || !std::isfinite(r)) {
        l = 0.0f;
        r = 0.0f;
      }
      if (c.limiterOn) {
        const float thresh = dbToGain(-24.0f + 24.0f * c.limiterThreshold);
        const float peakNow = std::max(std::fabs(l), std::fabs(r));
        const float target = peakNow > thresh ? thresh / peakNow : 1.0f;
        const float coeff =
            target < limiterGain_ ? limiterAttack_ : limiterRelease_;
        limiterGain_ = coeff * limiterGain_ + (1.0f - coeff) * target;
        l = std::tanh(l * limiterGain_);
        r = std::tanh(r * limiterGain_);
      }

      left[i] = l;
      right[i] = r;
      outPeakL = std::max(outPeakL, std::fabs(l));
      outPeakR = std::max(outPeakR, std::fabs(r));
      const float power = 0.5f * (l * l + r * r);
      vuPower_ = vuCoeff_ * vuPower_ + (1.0f - vuCoeff_) * power;
    }

    meters.inPeakLDb = gainToDb(inPeakL);
    meters.inPeakRDb = gainToDb(inPeakR);
    meters.inPeakDb = std::max(meters.inPeakLDb, meters.inPeakRDb);
    meters.outPeakLDb = gainToDb(outPeakL);
    meters.outPeakRDb = gainToDb(outPeakR);
    meters.outPeakDb = std::max(meters.outPeakLDb, meters.outPeakRDb);
    meters.outVuDb = gainToDb(std::sqrt(vuPower_)) + 18.0f;
    publishTuner(meters);
  }

  void publishTuner(Meters& meters) const noexcept {
    meters.tunerHz = tuner_.hz();
    meters.tunerCents = tuner_.cents();
    meters.tunerConfidence = tuner_.confidence();
    meters.tunerNote = tuner_.note();
  }

  void reset() noexcept {
    for (auto& s : stages_) {
      s.reset();
    }
    toneL_.reset();
    toneR_.reset();
    odL_.reset();
    odR_.reset();
    comp_.reset();
    oct_.reset();
    tuner_.reset();
    cabConvolver_.reset();
    // Only reset the NAM model when this engine is actually using it.  Both
    // crossfade engines can hold the same shared NamDSP; resetting the idle
    // one would wipe the active engine's NAM state.
    if (controls_.namOn && activeNam_) {
      activeNam_->reset();
    }
    dcPreL_.reset();
    dcPreR_.reset();
    dcPostL_.reset();
    dcPostR_.reset();
    cabHpL_.reset();
    cabHpR_.reset();
    outLowL_.reset();
    outLowR_.reset();
    outLowMidL_.reset();
    outLowMidR_.reset();
    outHighMidL_.reset();
    outHighMidR_.reset();
    outHighL_.reset();
    outHighR_.reset();
    outHpL_.reset();
    outHpR_.reset();
    outLpL_.reset();
    outLpR_.reset();
    cabLpL_.reset();
    cabLpR_.reset();
    powerL_.reset();
    powerR_.reset();
    os1L_.reset();
    os1R_.reset();
    os2L_.reset();
    os2R_.reset();
    preBrightL_.reset();
    preBrightR_.reset();
    preTightL_.reset();
    preTightR_.reset();
    preFocusL_.reset();
    preFocusR_.reset();
    postBodyL_.reset();
    postBodyR_.reset();
    postFocusL_.reset();
    postFocusR_.reset();
    postAirL_.reset();
    postAirR_.reset();
    presenceL_.reset();
    presenceR_.reset();
    presenceHiL_.reset();
    presenceHiR_.reset();
    resonanceL_.reset();
    resonanceR_.reset();
    vuPower_ = 0.0f;
    limiterGain_ = 1.0f;
  }

  uint32_t latencySamples() const noexcept { return 0; }

private:
  void prepareRecipe(int model) noexcept {
    const AmpRecipe& r = recipeFor(model);
    for (int i = 0; i < 6; ++i) {
      stages_[i].prepare(r.stage[i], sr_);
    }
    toneSpec_ = toneStackSpec(r.tone);
    toneL_.set(toneSpec_, 0.5f, 0.45f, 0.55f, sr_);
    toneR_.set(toneSpec_, 0.5f, 0.45f, 0.55f, sr_);
    toneL_.reset();
    toneR_.reset();
  }

  void setPresenceResonance(const Controls& c) noexcept {
    const float presenceDb = (c.presence - 0.5f) * 30.0f;
    const float resonanceDb = (c.resonance - 0.5f) * 12.0f;
    // Presence is a high shelf plus a smaller air shelf, not a narrow peak:
    // the goal is high-gain bite without making the midrange nasal.
    presenceL_.highShelf(2600.0f, presenceDb, 0.7f, sr_);
    presenceR_.highShelf(2600.0f, presenceDb, 0.7f, sr_);
    presenceHiL_.highShelf(7000.0f, presenceDb * 0.5f, 0.7f, sr_);
    presenceHiR_.highShelf(7000.0f, presenceDb * 0.5f, 0.7f, sr_);
    resonanceL_.lowShelf(120.0f, resonanceDb, 0.7f, sr_);
    resonanceR_.lowShelf(120.0f, resonanceDb, 0.7f, sr_);
  }

  void syncUserIrBank() noexcept {
    auto bank = userBankPending_.load();
    if (bank != activeUserBank_) {
      activeUserBank_ = std::move(bank);
    }
  }

  void syncNamModel() noexcept {
    auto model = namPending_.load();
    if (model != activeNam_) {
      activeNam_ = std::move(model);
    }
  }

  void applyOutputEq(const Controls& c) noexcept {
    const float low = (c.outEqLow - 0.5f) * 24.0f;
    const float lowMid = (c.outEqLowMid - 0.5f) * 24.0f;
    const float highMid = (c.outEqHighMid - 0.5f) * 24.0f;
    const float high = (c.outEqHigh - 0.5f) * 24.0f;
    outLowL_.lowShelf(80.0f, low, 0.7f, sr_);
    outLowR_.lowShelf(80.0f, low, 0.7f, sr_);
    outLowMidL_.peak(250.0f, lowMid, 0.9f, sr_);
    outLowMidR_.peak(250.0f, lowMid, 0.9f, sr_);
    outHighMidL_.peak(1200.0f, highMid, 0.9f, sr_);
    outHighMidR_.peak(1200.0f, highMid, 0.9f, sr_);
    outHighL_.highShelf(6000.0f, high, 0.7f, sr_);
    outHighR_.highShelf(6000.0f, high, 0.7f, sr_);
    const float hp = outHpfHz(c.outHpf);
    const float lp = outLpfHz(c.outLpf);
    outHpL_.highPass(hp, 0.707f, sr_);
    outHpR_.highPass(hp, 0.707f, sr_);
    outLpL_.lowPass(lp, 0.707f, sr_);
    outLpR_.lowPass(lp, 0.707f, sr_);
  }

  void updateCabIR() noexcept {
    // Factory cab selection is direct: 0 = 1x15, 1 = 4x10, 2 = 2x12,
    // 3 = 8x10.  A user IR replaces the factory cab for the current amp.
    const int cabModel =
        (fadeDir_ == -1 && lastModel_ >= 0) ? lastModel_ : controls_.model;
    const int set = std::clamp(controls_.cabSet, 0,
                               static_cast<int>(CabBank::Count) - 1);
    const CabIR* ir = nullptr;
    if (controls_.cab == 1 && activeUserBank_) {
      const int model = std::min(std::max(cabModel, 0), kAmpCount - 1);
      const int index = controls_.userIr[model];
      if (index > 0 &&
          index <= static_cast<int>(activeUserBank_->entries.size())) {
        const CabIR* candidate =
            &activeUserBank_->entries[static_cast<size_t>(index - 1)].ir;
        if (!candidate->h.empty()) {
          ir = candidate;
        }
      }
    }
    const int mic = controls_.cabMic;
    if (!ir) {
      ir = cabBank_.get(set, mic);
    }
    if (!ir) {
      ir = cabBank_.get(set, 0);
    }
    if (ir != activeCab_) {
      activeCab_ = ir;
      cabConvolver_.setIR(ir);
    }
  }

  void updateCab(const Controls& c) noexcept {
    cabHpL_.set(cabLowCutHz(c.cabLowCut), sr_);
    cabHpR_.set(cabLowCutHz(c.cabLowCut), sr_);
    cabLpL_.lowPass(cabHighCutHz(c.cabHighCut), 0.707f, sr_);
    cabLpR_.lowPass(cabHighCutHz(c.cabHighCut), 0.707f, sr_);
  }

  float sr_ = 48000.0f;
  float preGain_ = 0.0f;
  Controls controls_;
  int lastModel_ = -1;
  int pendingModel_ = -1;
  Controls pendingControls_;
  bool switchReady_ = false;
  int fadeDir_ = 0;
  int switchHold_ = 0;
  int switchHoldSamples_ = 1;
  float switchFadeSamples_ = 1.0f;
  float switchGain_ = 1.0f;
  float switchStep_ = 1.0f;
  float switchInStep_ = 0.001f;
  float vuCoeff_ = 0.0f;
  float vuPower_ = 0.0f;
  ToneStackSpec toneSpec_;
  TubeStage stages_[6];
  ToneStack toneL_;
  ToneStack toneR_;
  Smoothed inputSmooth_;
  Smoothed outputSmooth_;
  Ts9 odL_;
  Ts9 odR_;
  CompPedal comp_;
  OctavePedal oct_;
  Tuner tuner_;
  CabBank cabBank_;
  CabConvolver cabConvolver_;
  Biquad outLowL_, outLowR_, outLowMidL_, outLowMidR_;
  Biquad outHighMidL_, outHighMidR_, outHighL_, outHighR_;
  Biquad outHpL_, outHpR_, outLpL_, outLpR_;
  const CabIR* activeCab_ = nullptr;
  AtomicSharedPtr<const UserIrBank> userBankPending_;
  std::shared_ptr<const UserIrBank> activeUserBank_;
  AtomicSharedPtr<NamDSP> namPending_;
  std::shared_ptr<NamDSP> activeNam_;
  HighPass2 dcPreL_;
  HighPass2 dcPreR_;
  HighPass2 dcPostL_;
  HighPass2 dcPostR_;
  HighPass2 cabHpL_;
  HighPass2 cabHpR_;
  Biquad cabLpL_;
  Biquad cabLpR_;
  PowerAmp powerL_;
  PowerAmp powerR_;
  Oversampler2x os1L_;
  Oversampler2x os1R_;
  Oversampler2x os2L_;
  Oversampler2x os2R_;
  int powerOs_ = 0;
  Biquad preBrightL_;
  Biquad preBrightR_;
  HighPass2 preTightL_;
  HighPass2 preTightR_;
  Biquad preFocusL_;
  Biquad preFocusR_;
  Biquad postBodyL_;
  Biquad postBodyR_;
  Biquad postFocusL_;
  Biquad postFocusR_;
  Biquad postAirL_;
  Biquad postAirR_;
  Biquad presenceL_;
  Biquad presenceR_;
  Biquad presenceHiL_;
  Biquad presenceHiR_;
  Biquad resonanceL_;
  Biquad resonanceR_;
  float limiterGain_ = 1.0f;
  float limiterAttack_ = 0.0f;
  float limiterRelease_ = 0.0f;
  float gateAttack_ = 0.0f;
  float gateRelease_ = 0.0f;
  float gateEnvRelease_ = 0.0f;
  float gateThreshold_ = 1.0f;
  float gateGain_ = 1.0f;
  float gateEnv_ = 0.0f;
  float compAttack_ = 0.0f;
  float compRelease_ = 0.0f;
  float compThreshold_ = 1.0f;
  float compMakeup_ = 1.0f;
  float compMix_ = 0.0f;
  float compGain_ = 1.0f;
  float compEnv_ = 0.0f;
  bool compOn_ = false;
};

} // namespace bass
