/* FM stereo demuxer
 * windytan */
#include "demux.h"

#include <getopt.h>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>

#include "liquid_wrappers.h"
#include "options.h"

// TODO: Only 1 'anti-alias' filter should be needed, because linear algebra

DeEmphasis::DeEmphasis(float time_constant_us, float samplerate) {
  // https://lehrer.bulme.at/~tr/SDR/PRE_DE_EMPHASIS_web.html
  const float cutoff =
      (1.0f / (2.0f * static_cast<float>(M_PI) * time_constant_us * 1e-6f)) / samplerate;

  constexpr float kUnused{0.f};
  constexpr float kRipple{10.f};
  liquid_iirdes(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS, kDeEmphasisOrder,
                cutoff, kUnused, kRipple, kRipple, deemph_coeff_B.data(), deemph_coeff_A.data());
  iir_deemph_l = iirfilt_rrrf_create_sos(deemph_coeff_B.data(), deemph_coeff_A.data(), len + odd);
  iir_deemph_r = iirfilt_rrrf_create_sos(deemph_coeff_B.data(), deemph_coeff_A.data(), len + odd);
}

StereoSampleF32 DeEmphasis::run(StereoSampleF32 in) {
  StereoSampleF32 out;
  iirfilt_rrrf_execute(iir_deemph_l, in.l, &out.l);
  iirfilt_rrrf_execute(iir_deemph_r, in.r, &out.r);

  return out;
}

DeEmphasis::~DeEmphasis() {
  iirfilt_rrrf_destroy(iir_deemph_l);
  iirfilt_rrrf_destroy(iir_deemph_r);
}

void RunningAverage::push(float in) {
  sum -= buffer[idx];
  buffer[idx] = in;
  sum += buffer[idx];
  idx = (idx + 1) % buffer.size();
}

int main(int argc, char** argv) {
  const Options options = getOptions(argc, argv);

  if (options.print_usage) {
    const std::vector<std::string> usage{
        "usage: demux -r <samplerate> [OPTIONS]",
        "",
        "The program reads stdin and writes to stdout.",
        "Audio format is S16LE PCM, single channel (MPX) in, stereo out.",
        "",
        "-r, --samplerate-in <rate>  Input sample rate (Hz).",
        "-R, --samplerate-out <rate> Output sample rate (Hz).",
        "-g, --gain <number>         Output gain; use 6 to get +6dB or 2x for doubled amplitude.",
        "-d, --deemph <us>           De-emphasis time constant (microseconds), default is 50.",
        "--no-pilot                  Workaround for stations transmitting stereo without a pilot "
        "tone (rare).",
        "--swap                      Swap left and right channels."};

    for (const auto& line : usage) {
      fprintf(stderr, "%s\n", line.c_str());
    }
  }

  if (options.exit_failure) {
    return EXIT_FAILURE;
  }

  if (options.samplerate < kMinimumSampleRate) {
    fprintf(stderr, "input samplerate must be >= %.0f Hz\n", double(kMinimumSampleRate));
    return EXIT_FAILURE;
  }

  const float resample_ratio = options.output_rate / options.samplerate;
  const bool  do_resample    = resample_ratio != 1.f;

  if (resample_ratio > 1.f) {
    fprintf(stderr, "output samplerate must be <= input rate");
    return EXIT_FAILURE;
  };

  int16_t         inbuf[kBuflen];
  StereoSampleS16 outbuf[kBuflen];
  StereoSampleS16 resampled_outbuf[kBuflen];

  const float gain = options.gain;

  const float pilot_freq_hz = 19000.f;

  BPF bpf_pilot(pilot_freq_hz, options.samplerate);
  BPF bpf_subc(38000.f, options.samplerate);

  liquid::NCO nco_pilot_exact(angularFreq(pilot_freq_hz, options.samplerate));
  nco_pilot_exact.setPLLBandwidth(kPLLBandwidthHz / options.samplerate);
  liquid::NCO nco_stereo_subcarrier(38000.f / pilot_freq_hz *
                                    angularFreq(pilot_freq_hz, options.samplerate));

  liquid::FIRFilterR fir_l_plus_r(kAudioFIRLengthUsec * 1e-6f * options.samplerate,
                                  kAudioFIRCutoffHz / options.samplerate);
  liquid::FIRFilterR fir_l_minus_r(kAudioFIRLengthUsec * 1e-6f * options.samplerate,
                                   kAudioFIRCutoffHz / options.samplerate);

  DeEmphasis     deemphasis(options.time_constant_us, options.samplerate);
  RunningAverage pilotnoise;

  liquid::NCO nco_57(angularFreq(57000.f, options.samplerate));

  liquid::Resampler resampler(resample_ratio, 13);

  for (int i = 0; i < kBuflen; i++) {
    pilotnoise.push(9.f);
  }

  while (fread(&inbuf, sizeof(inbuf[0]), kBuflen, stdin)) {
    unsigned int i_resampled = 0;

    for (int n = 0; n < kBuflen; n++) {
      const float insample = inbuf[n];

      std::complex<float> pilot;
      if (options.regenerate_pilot) {
        pilot = bpf_subc.push(insample);

        pilot = pilot * pilot;

        pilot = nco_57.mixDown(pilot);
        nco_57.step();
      } else {
        pilot = bpf_pilot.push(insample);
      }

      // Generate 38 kHz carrier
      nco_stereo_subcarrier.setPhase(2.f * nco_pilot_exact.getPhase());

      // Pilot PLL
      const float phase_error = std::arg(pilot * std::conj(nco_pilot_exact.getComplex()));
      if (n % 4 == 0)
        nco_pilot_exact.stepPLL(phase_error);
      nco_pilot_exact.step();

      // Revert to mono if there is no pilot tone
      if (n % 4 == 0)
        pilotnoise.push(phase_error * phase_error);
      const float stereogain =
          options.regenerate_pilot
              ? 1.f
              : std::min(std::max(kStereoSeparation - pilotnoise.get(), 0.f), 1.f);

      // Decode stereo & anti-alias
      fir_l_plus_r.push(insample);
      fir_l_minus_r.push(nco_stereo_subcarrier.mixDown(insample).imag());
      const float l_plus_r  = fir_l_plus_r.execute();
      const float l_minus_r = 2 * fir_l_minus_r.execute() * stereogain;

      float left  = (l_plus_r + l_minus_r) * gain;
      float right = (l_plus_r - l_minus_r) * gain;

      if (options.swap)
        std::swap(left, right);

      // TODO: Combined FIR should be run here
      const StereoSampleF32 stereo = deemphasis.run({left, right});

      if (do_resample) {
        static std::complex<float> out[1];

        if (resampler.execute(std::complex<float>(stereo.l, stereo.r), out)) {
          resampled_outbuf[i_resampled].l = out[0].real();
          resampled_outbuf[i_resampled].r = out[0].imag();
          i_resampled++;
        }
      } else {
        outbuf[n] = stereo;
      }
    }

    if (do_resample) {
      if (!fwrite(&resampled_outbuf, sizeof(resampled_outbuf[0]), i_resampled, stdout))
        return EXIT_FAILURE;
    } else {
      if (!fwrite(&outbuf, sizeof(outbuf[0]), kBuflen, stdout))
        return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
