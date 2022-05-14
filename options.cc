#include "options.h"

#include <getopt.h>
#include <cstdlib>
#include <iostream>
#include <string>

float decodeSIprefix (const char* arg) {
  std::string str(arg);
  double factor = 1.0;
  if (str.size() > 1) {
    switch (tolower(str.back())) {
      case 'k':
        factor = 1e3;
        break;
      case 'M':
        factor = 1e6;
        break;
      default:
        factor = 1.0;
        break;
    }
  }
  return float(std::atof(arg) * factor);
}

Options getOptions(int argc, char** argv) {
  Options options{};
  bool is_rate_set { false };

  int c;
  while ((c = getopt(argc, argv, "r:R:d:g:")) != -1) {
    switch (c) {
      case 'r':
        options.samplerate = decodeSIprefix(optarg);
        is_rate_set = true;
        break;
      case 'R':
        options.output_rate = decodeSIprefix(optarg);
        break;
      case 'd':
        options.time_constant_us = std::atof(optarg);
        break;
      case 'g':
        options.gain_db = std::atof(optarg);
        break;
      case '?':
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        options.print_usage  = true;
        options.exit_failure = true;
        break;
      default:
        break;
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