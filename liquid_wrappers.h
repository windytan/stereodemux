#pragma once

#include <complex>
#include <vector>

#include "liquid/liquid.h"

namespace liquid {

class FIRFilter {
 public:
  FIRFilter(int len, float fc, float As = 60.0f, float mu = 0.0f);
  ~FIRFilter();
  void push(std::complex<float> s);
  std::complex<float> execute();
  float getGroupDelayAt(float f);

 private:
  firfilt_crcf object_;
};

class FIRFilterR {
 public:
  FIRFilterR(int len, float fc, float As = 60.0f, float mu = 0.0f);
  ~FIRFilterR();
  void push(float s);
  float execute();
  float getGroupDelayAt(float f);

 private:
  firfilt_rrrf object_;
};


class NCO {
 public:
  NCO(float freq);
  ~NCO();
  std::complex<float> mixDown(std::complex<float> s);
  std::complex<float> mixUp(std::complex<float> s);
  void step();
  void setPLLBandwidth(float);
  void setFrequency(float);
  void setPhase(float);
  void stepPLL(float dphi);
  float getFrequency();
  float getPhase();
  std::complex<float> getComplex();

 private:
  nco_crcf object_;
};

class WDelay {
 public:
  WDelay(int k);
  ~WDelay();
  void push(float x);
  float read();

 private:
  wdelayf object_;
};

}  // namespace liquid
