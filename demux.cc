/* FM stereo demuxer
 * Copyright (c) 2017 OH2EIQ. MIT license. */
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
        fprintf(stderr, "usage: stereo -r <rate>\n");
        return EXIT_FAILURE;
      default:
        break;
    }

  if (srate < 106000.0f) {
    fprintf(stderr, "rate must be >= 106000\n");
    exit(EXIT_FAILURE);
  }

  int16_t inbuf[kBuflen];
  StereoSample outbuf[kBuflen];

  liquid::NCO nco_pilot_approx(kPilotHz * 2.f * float(M_PI) / srate);
  liquid::NCO nco_pilot_exact(kPilotHz * 2.f * float(M_PI) / srate);
  nco_pilot_exact.setPLLBandwidth(kPLLBandwidthHz / srate);
  liquid::NCO nco_stereo_subcarrier(2.f * kPilotHz * 2.f * float(M_PI) / srate);
  liquid::FIRFilter fir_pilot(srate / 1350.0f, kPilotFIRHalfbandHz / srate);

  liquid::WDelay audio_delay(fir_pilot.getGroupDelayAt(100.0f / srate));

  liquid::FIRFilterR fir_l_plus_r(srate / 1350.0f, kAudioFIRCutoffHz / srate);
  liquid::FIRFilterR fir_l_minus_r(srate / 1350.0f, kAudioFIRCutoffHz / srate);

  DeEmphasis deemphasis(srate);

  float dc_cancel_buffer[kBuflen] = {0};
  float dc_cancel_sum = 0.f;

  while (fread(&inbuf, sizeof(inbuf[0]), kBuflen, stdin)) {
    for (int n = 0; n < kBuflen; n++) {

      // Remove DC offset
      dc_cancel_sum -= dc_cancel_buffer[n];
      dc_cancel_buffer[n] = inbuf[n];
      dc_cancel_sum += dc_cancel_buffer[n];
      float dc_cancel = dc_cancel_sum / kBuflen;
      float insample = (inbuf[n] - dc_cancel);

      // Delay audio to match pilot filter delay
      audio_delay.push(insample);

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

      // Decode stereo
      fir_l_plus_r.push(audio_delay.read());
      fir_l_minus_r.push(nco_stereo_subcarrier.mixDown(audio_delay.read()).real());
      float l_plus_r  = fir_l_plus_r.execute();
      float l_minus_r = kStereoGain * fir_l_minus_r.execute();

      float left  = (l_plus_r + l_minus_r);
      float right = (l_plus_r - l_minus_r);

      auto d = deemphasis.run({left, right});

      outbuf[n].l = d.l;
      outbuf[n].r = d.r;
    }

    if (!fwrite(&outbuf, sizeof(outbuf[0]), kBuflen, stdout))
      return (EXIT_FAILURE);
  }
}
