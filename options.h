#pragma once

struct Options {
  float samplerate       { 0.f };
  float output_rate      { 0.f };
  float time_constant_us { 50.f };
  float gain             { 1.f };
  bool  exit_failure     { false };
  bool  print_usage      { false };
};

Options getOptions(int argc, char** argv);