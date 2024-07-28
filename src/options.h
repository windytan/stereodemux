#pragma once

struct Options {
  float samplerate{};
  float output_rate{};
  float time_constant_us{50.f};
  float gain{1.f};
  bool  exit_failure{};
  bool  print_usage{};
  bool  swap{};
  bool  regenerate_pilot{};
};

Options getOptions(int argc, char** argv);