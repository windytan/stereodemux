#pragma once

namespace stereodemux {

struct Options {
  float samplerate{};
  float output_rate{};
  float time_constant_us{50.f};
  float gain{1.f};
  bool  exit_failure{};
  bool  print_usage{};
};

Options getOptions(int argc, char** argv);

}  // namespace stereodemux
