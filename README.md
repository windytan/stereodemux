# stereodemux

This command-line tool converts an FM broadcast signal into stereo sound with de-emphasis applied.

It expects 16-bit signed-integer MPX (FM demodulated mono PCM) via stdin and outputs stereo PCM to stdout. The sample rate can be specified with `-r`; otherwise it will be 171000 Hz.

## Requires

* [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)

## Compiling

    make

## Usage examples

Listen to stereo broadcasts with `rtl_fm` and `sox`:

    rtl_fm -M fm -l 0 -A std -p 0 -s 192k -g 40 -F 9 -f 90.0M | \
      ./demux -r 192000 | \
      play -q -t .s16 -r 192k -c 2 -

Listen with `aplay`:

    rtl_fm -M fm -l 0 -A std -p 0 -s 192k -g 40 -F 9 -f 90.0M | \
      ./demux -r 192000 | \
      aplay -f S16_LE -c 2 -r 192000

Convert a 192 kHz MPX WAV into a 44.1 kHz stereo WAV:

    sox mpx.wav -t .s16 - | \
      ./demux -r 192000 | \
      sox -t .s16 -r 192000 -c 2 - -r 44100 stereo.wav