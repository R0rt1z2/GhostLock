API      ?= 28
VERSION  := $(shell cat VERSION 2>/dev/null || echo 0.0.0)
OUTDIR   ?= build
BIN      := $(OUTDIR)/ghostlock_root

DEFAULT_NDK := $(HOME)/.cache/android-ndk/android-ndk-r27d
NDK      ?= $(or $(ANDROID_NDK_HOME),$(ANDROID_NDK_ROOT),$(wildcard $(DEFAULT_NDK)))
NDK_CC   := $(if $(NDK),$(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi$(API)-clang)

ifneq ($(origin CC),default)
  TARGET_CC := $(CC)
else ifneq ($(wildcard $(NDK_CC)),)
  TARGET_CC := $(NDK_CC)
else
  $(error no NDK found; set NDK=/path/to/android-ndk-r27d or CC=<armv7a clang>)
endif

SRCS  := src/gl_root.c src/gl_profile.c
EMBED := src/gl_profiles_embedded.h
HDRS  := src/gl_profile.h src/gl_reclaim.h $(EMBED)
DEBUG ?= 0
DEBUG_CFLAGS := $(if $(filter 1,$(DEBUG)),-DDEBUG,)

CFLAGS := -O2 -g0 -Wall -Wextra -fPIE -pie -pthread \
          -Wno-unused-parameter -Wno-sign-compare $(DEBUG_CFLAGS)

.PHONY: all clean info
all: $(BIN)

$(BIN): $(SRCS) $(HDRS) | $(OUTDIR)
	$(TARGET_CC) $(CFLAGS) $(SRCS) -o $@
	@sha256sum $@

$(OUTDIR):
	mkdir -p $@

info:
	@echo "VERSION=$(VERSION)"
	@echo "NDK=$(NDK)"
	@echo "TARGET_CC=$(TARGET_CC)"
	@echo "BIN=$(BIN)"

clean:
	rm -rf $(OUTDIR) dist
