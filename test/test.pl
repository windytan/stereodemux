use strict;
use warnings;
use IPC::Cmd qw(can_run);
use Carp;

# Windytan's stereodemux tests

##########################
## Limits & Preferences ##
##########################

# De-emphasis curve can't deviate more than this (dB) on any band
# when comparing decoded audio to original (before encoding via
# StereoTool)
my $max_deemphasis_error_db = 1.5;

# Overall RMS amplitude of the output audio should not differ from
# a lowpass-filtered version of the MPX signal by more than (dB)
my $max_levels_difference_rms_db = 1.0;

# Maximum resulting cross-talk (dB) from hard-panned source audio
my $max_stereo_crosstalk_db = -65;

# Maximum RMS difference between channels when source exactly centered (dB)
my $max_mono_difference_db = 0.01;

# Maximum amplitude difference between phase-aligned samples L/R
my $max_phase_aligned_difference = 1e-4;

# Print test information even if not failed
my $print_even_if_successful = 1;

# Different events in the test file (seconds)
my $testfile_panned_left_at_time  = 1.8;
my $testfile_centered_at_time     = 3.0;
my $testfile_in_antiphase_at_time = 6.0;
my $testfile_panned_right_at_time = 8.1;

##########################

use constant L => 0;
use constant R => 1;

my $has_failures = 0;

my @sides = qw( left right );

# These test files were generated by Otuxam3 in Thimeo Stereo Tool VST 8.00
# https://github.com/windytan/stereodemux/issues/2#issuecomment-1117933697
-e 'test/stereotool_50us.flac' or croak 'wav file not found!';
-e 'test/stereotool_75us.flac' or croak 'wav file not found!';
-e 'test/source.flac'          or croak 'wav file not found!';

my @samplerates_hz           = ( 128_000, 192_000 );
my @deemph_times_us          = ( 50,      75 );
my @deemphasis_test_bands_hz = ( 1500,    5000, 9000, 11000, 13_000, 15_000 );
my $deemphasis_halfbw_hz     = 1000;
my $tempfile                 = "test/test-decoded.wav";
my $binary                   = "./demux";

can_run('sox') or croak 'sox is not installed!';

main();

sub main {
  system("uname -rms");

  testPhase();
  testLevels();
  testCrosstalk();
  testDeemphasis();

  print $has_failures ? "Tests did not pass\n" : "All passed\n";

  exit $has_failures;
}

#########

# Test that hard-panned audio can't be heard on the wrong channel
sub testCrosstalk {
  for my $deemph (@deemph_times_us) {
    for my $fs (@samplerates_hz) {
      CheckSeparationLeftSide( $fs, $deemph );
      CheckSeparationRightSide( $fs, $deemph );
      CheckSeparationCenter( $fs, $deemph );
    }
  }
  return;
}

# Test that levels on both channels match up in time
sub testPhase {
  for my $deemph (@deemph_times_us) {
    for my $fs (@samplerates_hz) {
      my $diff   = getRightMinusLeftSampleExact( $deemph, $testfile_centered_at_time, 0.09, $fs );
      my $result = abs($diff) < $max_phase_aligned_difference;
      check( $result,
            "phase   params=[0°, $deemph μs, Fs=$fs Hz]  result[$diff]  "
          . "expected[< $max_phase_aligned_difference]" );

      $diff   = getRightPlusLeftSampleExact( $deemph, $testfile_in_antiphase_at_time, 0.09, $fs );
      $result = abs($diff) < $max_phase_aligned_difference;
      check( $result,
            "phase   params=[180°, $deemph μs, Fs=$fs Hz]  result[$diff]  "
          . "expected[< $max_phase_aligned_difference]" );
    }
  }
  return;
}

# Test that the output power in each band matches up with the original audio
sub testDeemphasis {
  for my $samplerate (@samplerates_hz) {
    for my $time_constant_us (@deemph_times_us) {
      for my $band_center (@deemphasis_test_bands_hz) {
        my $band_lo = $band_center - $deemphasis_halfbw_hz;
        my $band_hi = $band_center + $deemphasis_halfbw_hz;

        system("sox test/source.flac filtered.wav sinc $band_lo-$band_hi");
        my $rms_original = getRMS( "filtered.wav", $testfile_centered_at_time, 0.1 )->{'rms_mono'};

        demux( $time_constant_us, $samplerate );
        system("sox $tempfile filtered.wav sinc $band_lo-$band_hi");
        my $rms_decoded = getRMS( "filtered.wav", $testfile_centered_at_time, 0.1 )->{'rms_mono'};

        my $difference = $rms_decoded - $rms_original;
        my $result     = abs($difference) < $max_deemphasis_error_db;
        check( $result,
"de-emph params[$time_constant_us μs, Fs=$samplerate Hz, band=$band_center Hz]  result[error="
            . sprintf( "%+.1f dB", $difference )
            . "]  expected[|err| < $max_deemphasis_error_db dB]" );
      }
    }
  }
  return;
}

# Test that the overall power of the output audio matches the source audio
sub testLevels {

  # Make a comparison file by just low-pass filtering the pilot and subcarrier out
  system("sox test/source.flac -r 44100 source-filtered.wav sinc -16500");

  for my $time_constant_us (@deemph_times_us) {
    for my $samplerate (@samplerates_hz) {
      demux( $time_constant_us, $samplerate );

      # Hope the demuxed audio is of similar amplitude than the low-pass filtered MPX
      my $rms_orig = getRMS( "source-filtered.wav", $testfile_centered_at_time, 0.1 );
      my $rms_deco = getRMS( $tempfile,             $testfile_centered_at_time, 0.1 );

      my $diff   = abs( $rms_deco->{'rms_mono'} - $rms_orig->{'rms_mono'} );
      my $result = $diff < $max_levels_difference_rms_db;

      check( $result,
            "levels  params[Fs=$samplerate Hz, $time_constant_us μs]  result[orig="
          . sprintf( "%+.02f dB", $rms_orig->{'rms_mono'} )
          . " decoded="
          . sprintf( "%+.02f dB", $rms_deco->{'rms_mono'} )
          . "]  expected=[delta < $max_levels_difference_rms_db]" );
    }
  }

  return;
}

# Check that the stereo separation at time t is enough to the left
sub CheckSeparationLeftSide {
  my ( $fs, $deemph ) = @_;
  my $diff = getRightMinusLeftDecibels( $deemph, $testfile_panned_left_at_time, 0.09, $fs );

  my $result = $diff < 0;
  check( $result,
        "pan     params[left Fs=$fs Hz, $deemph μs]  result["
      . @sides[ $diff > 0 ]
      . "]  expected[left]" );

  $result = -abs($diff) < $max_stereo_crosstalk_db;
  check( $result,
        "cross   params[left Fs=$fs Hz, $deemph μs]  result[crosstalk=-|"
      . sprintf( "%+.1f dB", $diff )
      . "|]  expected[< $max_stereo_crosstalk_db dB]" );

  return;
}

# Check that the stereo separation at time t is enough to the right
sub CheckSeparationRightSide {
  my ( $fs, $deemph ) = @_;
  my $diff = getRightMinusLeftDecibels( $deemph, $testfile_panned_right_at_time, 0.09, $fs );

  my $result = $diff > 0;
  check( $result,
        "pan     params[right Fs=$fs Hz, $deemph μs]  result["
      . @sides[ $diff < 0 ]
      . "]  expected[right]" );

  $result = -abs($diff) < $max_stereo_crosstalk_db;
  check( $result,
        "cross   params[right Fs=$fs Hz, $deemph μs]  result[crosstalk=-|"
      . sprintf( "%+.1f dB", $diff )
      . "|]  expected[< $max_stereo_crosstalk_db dB]" );

  return;
}

# Check that the stereo separation at time t is centered
sub CheckSeparationCenter {
  my ( $fs, $deemph ) = @_;
  my $diff = getRightMinusLeftDecibels( $deemph, $testfile_centered_at_time, 0.09, $fs );

  my $result = abs( $diff < $max_mono_difference_db );
  check( $result,
        "mono    params[Fs=$fs Hz, $deemph μs]  result[|"
      . sprintf( "%+.3f dB", $diff )
      . "|]  expected[< $max_mono_difference_db dB]" );

  return;
}

# bool is expected to be true, otherwise fail with message
sub check {
  my ( $bool, $message ) = @_;
  if ( !$bool || $print_even_if_successful ) {
    print( ( $bool ? "[ OK ] " : "[FAIL] " ) . $message . "\n" );

    $has_failures = 1 if ( !$bool );
  }

  return;
}

# Run demuxer binary
sub demux {
  my ( $time_constant_us, $fs ) = @_;
  unlink($tempfile);
  system( "sox test/stereotool_"
      . $time_constant_us
      . "us.flac -t .s16 -r $fs -|"
      . "$binary -r $fs -d $time_constant_us|"
      . "sox -t .s16 -r $fs -c 2 - -r 44100 $tempfile" );

  return;
}

# Calculate the difference between RMS values averaged over time
sub getRightMinusLeftDecibels {
  ( my $deemph, my $t_start, my $t_dur, my $fs ) = @_;

  demux( $deemph, $fs );

  my $dbfs = getRMS( $tempfile, $t_start, $t_dur );
  return $dbfs->{'rms_r'} - $dbfs->{'rms_l'};
}

# Calculate the RMS of the difference between channels, phase-matched
sub getRightMinusLeftSampleExact {
  ( my $deemph, my $t_start, my $t_dur, my $fs ) = @_;

  demux( $deemph, $fs );

  return getRMS( $tempfile, $t_start, $t_dur )->{'diffs'};
}

# Calculate the RMS of the sum of channels, phase-matched
sub getRightPlusLeftSampleExact {
  ( my $deemph, my $t_start, my $t_dur, my $fs ) = @_;

  demux( $deemph, $fs );

  return getRMS( $tempfile, $t_start, $t_dur )->{'sums'};
}

# Calculate RMS amplitude for left, right, both, and the difference+sum
sub getRMS {
  my ( $file, $t_start, $t_dur ) = @_;

  my @squares;
  my @totalsquares;
  my @diffs;
  my @sums;

  open my $sox, '-|', "sox $file -t .f32 - trim $t_start $t_dur"
    or croak($!);
  while ( not eof $sox ) {
    my @ch;
    for ( L, R ) {
      read $sox, $ch[$_], 4;
      $ch[$_] = unpack "f", $ch[$_];

      push @{ $squares[$_] }, $ch[$_]**2;
      push @totalsquares,     $ch[$_]**2;
    }
    push @diffs, ( $ch[R] - $ch[L] )**2;
    push @sums,  ( $ch[R] + $ch[L] )**2;
  }
  close $sox;

  return {
    "rms_l"    => getdBFS( sqrt( mean( \@{ $squares[L] } ) ) ),
    "rms_r"    => getdBFS( sqrt( mean( \@{ $squares[R] } ) ) ),
    "rms_mono" => getdBFS( sqrt( mean( \@totalsquares ) ) ),
    "diffs"    => sqrt( mean( \@diffs ) ),
    "sums"     => sqrt( mean( \@sums ) )
  };
}

# Amplitude ratio to dBFS
sub getdBFS {
  ( my $amp ) = @_;
  return 20 * log10( abs($amp) );
}

# Array average value
sub mean {
  my $arr = $_[0];
  my $sum = 0;
  map { $sum += $_ } @{$arr};
  return $sum / @{$arr};
}

sub log10 {
  ( my $value ) = @_;
  return log($value) / log(10);
}