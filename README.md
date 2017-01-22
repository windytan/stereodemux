# stereodemux

Decode the stereo separation from an FM multiplex carrier. Requires
[liquid-dsp](https://github.com/jgaeddert/liquid-dsp).

Input: 16-bit FM demodulated PCM signal sampled at 171 kHz (or another rate
specified with `-r`)

Output: 16-bit stereo PCM at above rate

## Compiling

    make

## Usage

Listening to stereo broadcasts with `rtl_fm` and SoX:

    rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 -f 90.0M | \
      ./demux | \
      play -q -t .s16 -r 171k -c 2 -

## Copyright

(c) OH2EIQ. MIT license.
