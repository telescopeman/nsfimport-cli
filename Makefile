# nsfimport - build with `make` (needs a C and C++ compiler: gcc/clang, or MinGW on Windows)

CXX      ?= c++
CC       ?= cc
CXXFLAGS ?= -O2
CFLAGS   ?= -O2
CXXFLAGS += -std=c++11 -fwrapv -Wall -Wno-multichar -Wno-comment -Wno-unused-result \
            -Wno-misleading-indentation -Wno-unknown-pragmas -Wno-unused-variable \
            -Wno-deprecated-register
CFLAGS   += -fwrapv -Wall -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-function

TARGET = nsfimport

CXX_SRCS = src/main.cpp \
           src/NSF_Import.cpp \
           src/M3U.cpp \
           src/ftm/FtmDocument.cpp \
           src/notsofatso/NSF_6502.cpp \
           src/notsofatso/NSF_Core.cpp \
           src/notsofatso/NSF_File.cpp \
           src/notsofatso/Wave_VRC7.cpp
C_SRCS   = src/notsofatso/fmopl.c

OBJS = $(CXX_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) -lm

%.o: %.cpp src/notsofatso/*.h src/ftm/*.h src/*.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c src/notsofatso/fmopl.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all clean
