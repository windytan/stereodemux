#include "options.h"

#include <getopt.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace stereodemux {

float decodeSIprefix(const char* arg) {
  std::string str(arg);
  float       factor = 1.f;
  if (str.size() > 1) {
    switch (tolower(str.back())) {
      case 'k': factor = 1e3f; break;
      case 'M': factor = 1e6f; break;
      default:  factor = 1.0f; break;
    }
  }
  return static_cast<float>(std::atof(arg)) * factor;
}

float dB_to_ratio(float dB) {
  return std::pow(10.f, dB / 20.f);
}

float decodeGain(const char* arg) {
  std::string str(arg);
  float       factor;
  if (str.size() > 1 && tolower(str.back()) == 'x') {
    factor = std::atof(arg);
  } else {
    factor = dB_to_ratio(std::atof(arg));
  }
  return factor;
}

Options getOptions(int argc, char** argv) {
  Options options{};
  bool    is_rate_set{};
  int     option_char{};
  int     option_index{};

  // clang-format off
  const std::array<struct option, 7> long_options{{
       {"samplerate-in",  required_argument, nullptr,       'r'},
       {"samplerate-out", required_argument, nullptr,       'R'},
       {"deemph",         required_argument, nullptr,       'd'},
       {"gain",           required_argument, nullptr,       'g'},
       {nullptr,          0,                 nullptr,       0},
  }};
  // clang-format on

  while ((option_char = getopt_long(argc, argv, "r:R:d:g:", long_options.data(), &option_index)) >=
         0) {
    switch (option_char) {
      case 0: break;  // long-only option
      case 'r':
        options.samplerate = decodeSIprefix(optarg);
        is_rate_set        = true;
        break;
      case 'R': options.output_rate = decodeSIprefix(optarg); break;
      case 'd': options.time_constant_us = std::atof(optarg); break;
      case 'g': options.gain = decodeGain(optarg); break;
      case '?':
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        options.print_usage  = true;
        options.exit_failure = true;
        break;
      default: break;
    }
  }

  if (not is_rate_set) {
    options.print_usage  = true;
    options.exit_failure = true;
  }

  if (options.output_rate == 0.f) {
    options.output_rate = options.samplerate;
  }

  return options;
}

}  // namespace stereodemux
