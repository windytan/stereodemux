demux: demux.cc
	g++ -std=gnu++11 -Wall -Wextra -O3 -o demux demux.cc liquid_wrappers.cc -lliquid
