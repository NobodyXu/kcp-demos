CC = clang
CXX = clang++

CFLAGS := -Ofast -fvisibility=hidden -Wall -flto
CFLAGS += -fno-asynchronous-unwind-tables -fno-unwind-tables -fmerge-all-constants

CXXFLAGS := -fno-exceptions -fno-rtti

LDFLAGS = -Wl,-icf=all,--gc-sections -flto -Wl,--plugin-opt=O3 -fuse-ld=lld

## Objects to build
C_SRCS := $(shell find . -name '*.c' -a ! -wholename './kcp/*')
CXX_SRCS := $(shell find . -name '*.cc' -a ! -wholename './kcp/*')
OUTS := $(C_SRCS:.c=.out) $(CXX_SRCS:.cc=.out)

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
