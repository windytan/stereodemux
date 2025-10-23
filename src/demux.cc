/* FM stereo demuxer
 * windytan */
#include "demux.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>

#include "liquid_wrappers.h"
#include "options.h"

namespace stereodemux {

// TODO: Only 1 'anti-alias' filter should be needed, because linear algebra

// Hertz to radians per sample
float angularFreq(float hertz, float samplerate) {
  return hertz * 2.f * static_cast<float>(M_PI) / samplerate;
}

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

int run(const Options& options) {
  if (options.print_usage) {
    fprintf(
        stderr,
        "usage: demux -r <samplerate> [-R samplerate_out] [-d time_constant_μs] [-g gain_db]\n");
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

  std::array<int16_t, kBuflen>         inbuf;
  std::array<StereoSampleS16, kBuflen> outbuf;
  std::array<StereoSampleS16, kBuflen> resampled_outbuf;

  const float gain = options.gain;

  liquid::NCO nco_pilot_approx(angularFreq(kPilotHz, options.samplerate));
  liquid::NCO nco_pilot_exact(angularFreq(kPilotHz, options.samplerate));
  nco_pilot_exact.setPLLBandwidth(kPLLBandwidthHz / options.samplerate);
  liquid::NCO nco_stereo_subcarrier(2.f * angularFreq(kPilotHz, options.samplerate));

  const int         pilotFirHalfLength = options.samplerate * 1e-6f * kPilotFIRUsec;
  liquid::FIRFilter fir_pilot(pilotFirHalfLength * 2 + 1, kPilotFIRHalfbandHz / options.samplerate);

  liquid::FIRFilterR fir_sum(kAudioFIRLengthUsec * 1e-6f * options.samplerate,
                             kAudioFIRCutoffHz / options.samplerate);
  liquid::FIRFilterR fir_diff(kAudioFIRLengthUsec * 1e-6f * options.samplerate,
                              kAudioFIRCutoffHz / options.samplerate);

  DeEmphasis     deemphasis(options.time_constant_us, options.samplerate);
  RunningAverage pilotnoise;

  liquid::Resampler resampler(resample_ratio, 13);

  for (int i = 0; i < kBuflen; i++) {
    pilotnoise.push(9.f);
  }

  while (fread(&inbuf, sizeof(inbuf[0]), inbuf.size(), stdin)) {
    unsigned int i_resampled = 0;

    for (std::size_t n = 0; n < inbuf.size(); n++) {
      const float insample = inbuf[n];

      // Pilot bandpass (mix-down + lowpass + mix-up)
      fir_pilot.push(nco_pilot_approx.mixDown(insample));
      const std::complex<float> pilot = nco_pilot_approx.mixUp(fir_pilot.execute());
      nco_pilot_approx.step();

      // Generate 38 kHz carrier
      nco_stereo_subcarrier.setPhase(2 * nco_pilot_exact.getPhase());

      // Pilot PLL
      const float phase_error = std::arg(pilot * std::conj(nco_pilot_exact.getComplex()));
      if (n % 4 == 0)
        nco_pilot_exact.stepPLL(phase_error);
      nco_pilot_exact.step();

      // Revert to mono if there is no pilot tone
      if (n % 4 == 0)
        pilotnoise.push(phase_error * phase_error);
      const float stereogain = std::min(std::max(kStereoSeparation - pilotnoise.get(), 0.f), 1.f);

      // Decode stereo & anti-alias
      fir_sum.push(insample);
      fir_diff.push(nco_stereo_subcarrier.mixDown(insample).imag());
      const float sum  = fir_sum.execute();
      const float diff = 2.f * fir_diff.execute() * stereogain;

      const float left  = (sum + diff) * gain;
      const float right = (sum - diff) * gain;

      // TODO: Combined FIR should be run here
      const StereoSampleF32 stereo = deemphasis.run({left, right});

      if (do_resample) {
        std::complex<float> out;

        if (resampler.execute(std::complex<float>(stereo.l, stereo.r), &out)) {
          resampled_outbuf[i_resampled].l = out.real();
          resampled_outbuf[i_resampled].r = out.imag();
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

}  // namespace stereodemux

int main(int argc, char** argv) {
  const stereodemux::Options options = stereodemux::getOptions(argc, argv);
  return stereodemux::run(options);
}
