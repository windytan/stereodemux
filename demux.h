#pragma once

#include <array>

#include "liquid_wrappers.h"

constexpr int   kBuflen             = 8192;
constexpr float kDefaultRate        = 171000.0f;
constexpr float kMinimumRate        = 106000.0f;
constexpr float kPilotHz            = 19000.0f;
constexpr float kPLLBandwidthHz     = 9.0f;
constexpr float kPilotFIRHalfbandHz = 800.0f;
constexpr float kAudioFIRCutoffHz   = 15000.0f;
constexpr float kAudioFIRLengthUsec = 740.f;
constexpr int   kDeEmphasisOrder    = 2;
constexpr float kDeEmphasisCutoffHz = 5000.0f;

struct StereoSampleF {
  float l;
  float r;
};

struct StereoSample {
  StereoSample() = default;
  StereoSample(StereoSampleF samplef) {
    l = samplef.l;
    r = samplef.r;
  }
  int16_t l;
  int16_t r;
};

class DeEmphasis {
 public:
  DeEmphasis(float srate);
  ~DeEmphasis();
  StereoSampleF run(StereoSampleF in);

 private:
  static const int odd = kDeEmphasisOrder % 2;         // odd/even order
  static const int len = (kDeEmphasisOrder - odd) / 2; // filter semi-length

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
