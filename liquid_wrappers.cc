#include "liquid_wrappers.h"

#include <cassert>
#include <complex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// https://github.com/jgaeddert/liquid-dsp/issues/229
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" {
#include "liquid/liquid.h"
}
#pragma clang diagnostic pop

namespace liquid {

FIRFilter::FIRFilter(int len, float fc, float As, float mu)
    : object_(firfilt_crcf_create_kaiser(len, fc, As, mu)) {
  firfilt_crcf_set_scale(object_, 2.0f * fc);
}

FIRFilter::~FIRFilter() {
  firfilt_crcf_destroy(object_);
}

void FIRFilter::push(std::complex<float> s) {
  firfilt_crcf_push(object_, s);
}

std::complex<float> FIRFilter::execute() {
  std::complex<float> result;
  firfilt_crcf_execute(object_, &result);
  return result;
}

FIRFilterR::FIRFilterR(int len, float fc, float As, float mu)
    : object_(firfilt_rrrf_create_kaiser(len, fc, As, mu)) {
  firfilt_rrrf_set_scale(object_, 2.0f * fc);
}

FIRFilterR::~FIRFilterR() {
  firfilt_rrrf_destroy(object_);
}

void FIRFilterR::push(float s) {
  firfilt_rrrf_push(object_, s);
}

float FIRFilterR::execute() {
  float result;
  firfilt_rrrf_execute(object_, &result);
  return result;
}

NCO::NCO(float freq) : object_(nco_crcf_create(LIQUID_VCO)) {
  nco_crcf_set_frequency(object_, freq);
}

NCO::~NCO() {
  nco_crcf_destroy(object_);
}

std::complex<float> NCO::mixDown(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_down(object_, s, &result);
  return result;
}

std::complex<float> NCO::mixUp(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_up(object_, s, &result);
  return result;
}

void NCO::step() {
  nco_crcf_step(object_);
}

void NCO::setPLLBandwidth(float bw) {
  nco_crcf_pll_set_bandwidth(object_, bw);
}

void NCO::setPhase(float ph) {
  nco_crcf_set_phase(object_, ph);
}

void NCO::stepPLL(float dphi) {
  nco_crcf_pll_step(object_, dphi);
}

float NCO::getPhase() {
  return nco_crcf_get_phase(object_);
}

std::complex<float> NCO::getComplex() {
  std::complex<float> y;
  nco_crcf_cexpf(object_, &y);
  return y;
}

Resampler::Resampler(float ratio, unsigned int length)
    : object_(resamp_crcf_create(ratio, length, 0.47f, 60.0f, 32)) {}

Resampler::~Resampler() {
  resamp_crcf_destroy(object_);
}

unsigned int Resampler::execute(std::complex<float> in, std::complex<float>* out) {
  unsigned int num_written;
  resamp_crcf_execute(object_, in, out, &num_written);

  return num_written;
}

}  // namespace liquid
