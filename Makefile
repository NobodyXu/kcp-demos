CC = clang
CXX = clang++

CFLAGS := -Ofast -fvisibility=hidden -Wall -flto
CFLAGS += -fno-asynchronous-unwind-tables -fno-unwind-tables -fmerge-all-constants

CXXFLAGS := -fno-exceptions -fno-rtti

LDFLAGS = -Wl,-icf=all,--gc-sections -flto -Wl,--plugin-opt=O3 -fuse-ld=lld

## Objects to build
SRCS := $(wildcard */*.c)
OUTS := $(SRCS:.c=.out)

## Build rules
all: $(OUTS)

%.out: %.c kcp/ikcp.o kcp/ikcp.h Makefile
	$(CC) -std=c11 $(CFLAGS) -o $@ $< kcp/ikcp.o

%.out: %.cc kcp/ikcp.o kcp/ikcp.h Makefile
	$(CXX) -std=c++17 $(CXXFLAGS) $(CFLAGS) -o $@ $< kcp/ikcp.o

### Special rule for kcp/ikcp.o
kcp/ikcp.o: kcp/ikcp.c kcp/ikcp.h
	$(CC) -std=c11 -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OUTS) kcp/ikcp.o
.PHONY: clean
