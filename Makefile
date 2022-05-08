demux: demux.cc demux.h liquid_wrappers.cc liquid_wrappers.h options.cc options.h
	g++ -std=gnu++11 -Wall -Wextra -Wstrict-overflow -Wshadow -Wdouble-promotion -Wundef -Wpointer-arith -Wcast-align -Wcast-qual -Wuninitialized -pedantic -Wno-return-type-c-linkage -O3 -o demux demux.cc liquid_wrappers.cc options.cc -lliquid

clean:
	rm -f demux
