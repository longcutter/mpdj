# MPC One+ build of Polyend Tracker CE
# Cross-compiles for armhf Linux (glibc 2.31+) with zig, from Mac or Linux:
#   pip3 install ziglang
#   make -C platforms/mpc -j4
# Output: platforms/mpc/build/tracker

ZIG        ?= python3 -m ziglang
TARGET_TRIPLE ?= arm-linux-gnueabihf.2.31
MCPU          ?= -mcpu=cortex_a17
CXX        := $(ZIG) c++ -target $(TARGET_TRIPLE)
CC         := $(ZIG) cc -target $(TARGET_TRIPLE)

ROOT       := ../..
CORE       := $(ROOT)/core
BUILD      ?= build
OBJ        := $(BUILD)/obj

CORE_CPP   := $(shell cd $(CORE) && find source libs interface effector FileManager Exporter AudioEngine synth -name '*.cpp')
CORE_C     := $(shell cd $(CORE) && find libs -name '*.c')
PLAT_CPP   := $(shell find source -name '*.cpp')
PLAT_C     := $(shell find source -name '*.c')

OBJS := $(addprefix $(OBJ)/core/,$(CORE_CPP:.cpp=.o) $(CORE_C:.c=.o)) \
        $(addprefix $(OBJ)/plat/,$(PLAT_CPP:.cpp=.o) $(PLAT_C:.c=.o))

INCLUDES := -Isource -Isource/compat \
  -I$(CORE)/include -I$(CORE)/config -I$(CORE)/source -I$(CORE)/synth -I$(CORE)/libs \
  -I$(CORE)/libs/Audio -I$(CORE)/libs/ElapsedMillis -I$(CORE)/libs/Audio/utility \
  -I$(CORE)/libs/FT812 -I$(CORE)/libs/fonts -I$(CORE)/libs/NewDisplay -I$(CORE)/interface \
  -I$(CORE)/effector -I$(CORE)/FileManager -I$(CORE)/Exporter -I$(CORE)/AudioEngine \
  -I$(CORE)/libs/Audio/Polyverb -I$(CORE)/AudioEngine/ConstantValues \
  -I$(CORE)/AudioEngine/MasterEnhancer -I$(ROOT)/util/include -I$(ROOT)/util/3rdparty \
  -I$(ROOT)/synth/include

-include $(ROOT)/version.mk

# PLATFORM_IMX = Tracker+ feature set (stereo, 16 tracks). FEATURE_SYNTH left out.
DEFINES := -DPLATFORM_IMX -DPLATFORM_MPC -DUSB_MIDI -DLAYOUT_US_ENGLISH \
  -D__MK66FX1M0__ -DTEENSYDUINO=142 -DARDUINO=10805 -DF_CPU=180000000 \
  -DNDEBUG $(FW_VERSION_DEFINES) $(CUSTOM_DEFINES)

COMMON   := $(MCPU) -O2 -g -fno-common -ffunction-sections -fdata-sections \
            -Wall -Wno-unused-parameter -Wno-unused-variable -MMD -MP
CXXFLAGS := $(COMMON) -std=gnu++20 -include cstdint -include mpc_compat.h -fno-rtti -fno-exceptions
CFLAGS   := $(COMMON) -std=gnu11 -include mpc_compat.h
LDFLAGS  := -Wl,--gc-sections -lpthread -ldl -lm

all: $(BUILD)/tracker

strip: $(BUILD)/tracker
	$(CXX) -s -o $(BUILD)/tracker-mpc $(OBJS) $(LDFLAGS)

$(BUILD)/tracker: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

$(OBJ)/core/%.o: $(CORE)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(DEFINES) $(INCLUDES) $(CXXFLAGS) -c -o $@ $<

$(OBJ)/core/%.o: $(CORE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c -o $@ $<

$(OBJ)/plat/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(DEFINES) $(INCLUDES) $(CXXFLAGS) -c -o $@ $<

$(OBJ)/plat/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c -o $@ $<

objs: $(OBJS)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
.PHONY: all objs clean strip
