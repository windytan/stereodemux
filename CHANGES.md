v1.0 (6/2024)
=============
- fix a bug that silenced the output if -g wasn't specified
- add the possibility to build using meson
- add tests and build check pipeline
- refactor the code a bit
- include cmath in options.cc

v0.3 (5/2022)
=============
- sample rate is a mandatory argument
- user can input sample rate as 192k instead of 192000
- user can specify de-emphasis time constant with -d
- output will be resampled if an output rate is given (-R)
- can set additional output gain with -g
- adjusted de-emphasis to be 50 μs by default
- adjusted audio low-pass from 15 kHz to 16.5 kHz

v0.2 (1/2022)
=============
- correct stereo polarity
- go mono if pilot not present
- more cpu-efficient de-emphasis with real-only filters
- remove the ineffective dc canceler
- tested the amount of separation to be enough or even a little extra
- nicer readme