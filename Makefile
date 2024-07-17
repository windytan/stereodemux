CC=g++
CFLAGS=-std=gnu++11 -Wall -Wextra -Wstrict-overflow -Wshadow -Wdouble-promotion -Wundef -Wpointer-arith -Wcast-align -Wcast-qual -Wuninitialized -pedantic -Wno-return-type-c-linkage -O3

ifdef EXTRA_CFLAGS
CFLAGS += ${EXTRA_CFLAGS}
endif

demux: src/demux.cc src/demux.h src/liquid_wrappers.cc src/liquid_wrappers.h src/options.cc src/options.h
	$(CC) $(CFLAGS) -o $@ src/demux.cc src/liquid_wrappers.cc src/options.cc -lliquid

clean:
	rm -f demux
