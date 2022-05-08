v0.3 (5/2022)
=============
- sample rate is a mandatory argument
- user can input sample rate as 192k instead of 192000
- user can specify de-emphasis time constant with -d
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