 /*
 * Oona Räisänen 2017 */

#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>

#include "liquid_wrappers.h"

struct stereosample {
  int16_t l;
  int16_t r;
};

int main(int argc, char **argv) {

  const int buflen = 1024;
  float freq_pilot = 19000.0f * 2 * M_PI, srate = 171000.0f;

  int c;
  while ((c = getopt (argc, argv, "r:")) != -1)
    switch (c) {
      case 'r':
        srate = atof(optarg);
        break;
      case '?':
        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        fprintf(stderr, "usage: stereo -r <rate>\n");
        return EXIT_FAILURE;
      default:
        break;
    }

  if (srate < 106000.0f) {
    fprintf(stderr, "rate must be >= 106000\n");
    exit(EXIT_FAILURE);
  }

  freq_pilot /= srate;

  int16_t inbuf[buflen];
  stereosample outbuf[buflen];

  liquid::NCO nco_pilot_approx(19000.0f * 2 * M_PI / srate);
  liquid::NCO nco_pilot_exact(19000.0f * 2 * M_PI / srate);
  nco_pilot_exact.setPLLBandwidth(0.00001f);
  liquid::NCO nco_stereo(38000.0f * 2 * M_PI / srate);
  liquid::FIRFilter fir_pilot(127, 2000.0f / srate);

  liquid::WDelay delay(fir_pilot.getGroupDelayAt(4000.0f / srate));

  liquid::FIRFilter fir_l_plus_r(127, 15000.0f / srate);
  liquid::FIRFilter fir_l_minus_r(127, 15000.0f / srate);

  liquid::FIRFilter fir_deemph_l(16, 2121.0f / srate);
  liquid::FIRFilter fir_deemph_r(16, 2121.0f / srate);

  int16_t dc_cancel_buffer[buflen] = {0};
  int dc_cancel_sum = 0;

  while (fread(&inbuf, sizeof(int16_t), buflen, stdin)) {

    for (int n = 0; n < buflen; n++) {

      dc_cancel_sum -= dc_cancel_buffer[n];
      dc_cancel_buffer[n] = inbuf[n];
      dc_cancel_sum += dc_cancel_buffer[n];

      int16_t dc_cancel = dc_cancel_sum / buflen;

      std::complex<float> insample(1.0f*(inbuf[n] - dc_cancel), 0.0f);

      std::complex<float> pilot_bb =
        nco_pilot_approx.mixDown(insample);
      delay.push(insample);

      fir_pilot.push(pilot_bb);

      std::complex<float> pilot_bp =
        nco_pilot_approx.mixUp(fir_pilot.execute());

      float dph = std::arg(pilot_bp * std::conj(nco_pilot_exact.getComplex()));

      nco_stereo.setPhase(2 * nco_pilot_exact.getPhase());

      std::complex<float> stereo = nco_stereo.mixDown(delay.read());

      nco_pilot_approx.step();
      nco_pilot_exact.stepPLL(dph);
      nco_pilot_exact.step();

      ///

      fir_l_plus_r.push(delay.read());
      fir_l_minus_r.push(stereo);
      float l_plus_r = fir_l_plus_r.execute().real();
      float l_minus_r = fir_l_minus_r.execute().real();

      float left = (l_plus_r + l_minus_r);
      float right = (l_plus_r - l_minus_r);

      fir_deemph_l.push({left, 0.0f});
      fir_deemph_r.push({right, 0.0f});

      outbuf[n].l = fir_deemph_l.execute().real();
      outbuf[n].r = fir_deemph_r.execute().real();
    }

    if (!fwrite(&outbuf, sizeof(stereosample), buflen, stdout))
      return (EXIT_FAILURE);
    fflush(stdout);
  }
}
