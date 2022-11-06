#pragma once

#include <array>

#include "liquid_wrappers.h"

constexpr int   kBuflen             = 8192;       // I/O buffer length
constexpr float kMinimumSampleRate  = 106000.0f;
constexpr float kPilotHz            = 19000.0f;
constexpr float kPLLBandwidthHz     = 9.0f;
constexpr float kPilotFIRUsec       = 740.f;
constexpr float kPilotFIRHalfbandHz = 800.0f;
constexpr float kAudioFIRCutoffHz   = 16500.0f;
constexpr float kAudioFIRLengthUsec = 740.f;      // Longer filter = better antialiasing
constexpr int   kDeEmphasisOrder    = 1;
constexpr float kStereoSeparation   = 1.2f;

struct StereoSampleF32 {
  float l;
  float r;
};

struct StereoSampleS16 {
  StereoSampleS16() = default;
  StereoSampleS16(StereoSampleF32 samplef) {
    l = samplef.l;
    r = samplef.r;
  }
  int16_t l;
  int16_t r;
};

class DeEmphasis {
 public:
  DeEmphasis(float time_constant_us, float srate);
  ~DeEmphasis();
  StereoSampleF32 run(StereoSampleF32 in);

 private:
  static constexpr int odd = kDeEmphasisOrder % 2;         // odd/even order
  static constexpr int len = (kDeEmphasisOrder - odd) / 2; // filter semi-length

  float deemph_coeff_B[3*(len + odd)];
  float deemph_coeff_A[3*(len + odd)];

  iirfilt_rrrf iir_deemph_l;
  iirfilt_rrrf iir_deemph_r;
};

class RunningAverage {
 public:
  RunningAverage() = default;
  void push(float in);
  float get() const { return sum / buffer.size(); }

 private:
  std::array<float, kBuflen> buffer{};
  float sum {0.f};
  int   idx {0};
};
