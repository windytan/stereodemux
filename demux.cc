/* FM stereo demuxer
 * windytan */
#include "demux.h"

#include <getopt.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <complex>

#include "liquid_wrappers.h"

DeEmphasis::DeEmphasis(float srate) {
  liquid_iirdes(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_LOWPASS, LIQUID_IIRDES_SOS,
      kDeEmphasisOrder, kDeEmphasisCutoffHz / srate, 0.0f, 10.0f, 10.0f,
      deemph_coeff_B, deemph_coeff_A);
  iir_deemph_l = iirfilt_rrrf_create_sos(deemph_coeff_B,
      deemph_coeff_A, len + odd);
  iir_deemph_r = iirfilt_rrrf_create_sos(deemph_coeff_B,
      deemph_coeff_A, len + odd);
}

StereoSampleF DeEmphasis::run(StereoSampleF in) {
  StereoSampleF out;
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

int main(int argc, char **argv) {
  float srate = kDefaultRate;

  int c;
  while ((c = getopt(argc, argv, "r:")) != -1)
    switch (c) {
      case 'r':
        srate = atof(optarg);
        break;
      case '?':
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        fprintf(stderr, "usage: demux [-r <rate>]\n");
        return EXIT_FAILURE;
      default:
        break;
    }

  if (srate < kMinimumRate) {
    fprintf(stderr, "rate must be >= %.0f Hz\n", double(kMinimumRate));
    return EXIT_FAILURE;
  }

  int16_t inbuf[kBuflen];
  StereoSample outbuf[kBuflen];

  liquid::NCO nco_pilot_approx(kPilotHz * 2.f * float(M_PI) / srate);
  liquid::NCO nco_pilot_exact(kPilotHz * 2.f * float(M_PI) / srate);
  nco_pilot_exact.setPLLBandwidth(kPLLBandwidthHz / srate);
  liquid::NCO nco_stereo_subcarrier(2.f * kPilotHz * 2.f * float(M_PI) / srate);

  const int pilotFirHalfLength = srate * 1e-6f * 740.f;
  liquid::FIRFilter fir_pilot(pilotFirHalfLength * 2 + 1, kPilotFIRHalfbandHz / srate);

  liquid::FIRFilterR fir_l_plus_r (kAudioFIRLengthUsec * 1e-6f * srate, kAudioFIRCutoffHz / srate);
  liquid::FIRFilterR fir_l_minus_r(kAudioFIRLengthUsec * 1e-6f * srate, kAudioFIRCutoffHz / srate);

  DeEmphasis deemphasis(srate);
  RunningAverage pilotnoise;

  for (int i = 0; i < kBuflen; i++) {
    pilotnoise.push(9.f);
  }

  while (fread(&inbuf, sizeof(inbuf[0]), kBuflen, stdin)) {
    for (int n = 0; n < kBuflen; n++) {

      float insample = inbuf[n];

      // Pilot bandpass (mix-down + lowpass + mix-up)
      fir_pilot.push(nco_pilot_approx.mixDown(insample));
      std::complex<float> pilot =
        nco_pilot_approx.mixUp(fir_pilot.execute());
      nco_pilot_approx.step();

      // Generate 38 kHz carrier
      nco_stereo_subcarrier.setPhase(2 * nco_pilot_exact.getPhase());

      // Pilot PLL
      float phase_error =
          std::arg(pilot * std::conj(nco_pilot_exact.getComplex()));
      nco_pilot_exact.stepPLL(phase_error);
      nco_pilot_exact.step();

      // Revert to mono if there is no pilot tone
      if (n % 4 == 0)
        pilotnoise.push(phase_error * phase_error);
      float stereogain = 1.2f - pilotnoise.get();
      if (stereogain < 0.f) stereogain = 0.f;
      if (stereogain > 1.f) stereogain = 1.f;

      // Decode stereo
      fir_l_plus_r.push(insample);
      fir_l_minus_r.push(nco_stereo_subcarrier.mixDown(insample).imag());
      float l_plus_r  = fir_l_plus_r.execute();
      float l_minus_r = 2 * fir_l_minus_r.execute() * stereogain;

      float left  = (l_plus_r + l_minus_r);
      float right = (l_plus_r - l_minus_r);

      outbuf[n] = deemphasis.run({left, right});
    }

    if (!fwrite(&outbuf, sizeof(outbuf[0]), kBuflen, stdout))
      return EXIT_FAILURE;
  }
}
