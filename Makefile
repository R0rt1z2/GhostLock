API      ?= 25
VERSION  := $(shell cat VERSION 2>/dev/null || echo 0.0.0)
OUTDIR   ?= build
SHA256SUM ?= $(if $(shell command -v sha256sum 2>/dev/null),sha256sum,shasum -a 256)
BIN      := $(OUTDIR)/ghostlock_root

DEFAULT_NDK := $(HOME)/.cache/android-ndk/android-ndk-r27d
NDK      ?= $(or $(ANDROID_NDK_HOME),$(ANDROID_NDK_ROOT),$(wildcard $(DEFAULT_NDK)))
HOST_OS  := $(shell uname -s)
NDK_HOST ?= $(if $(filter Darwin,$(HOST_OS)),darwin-x86_64,linux-x86_64)
NDK_CC   := $(if $(NDK),$(NDK)/toolchains/llvm/prebuilt/$(NDK_HOST)/bin/armv7a-linux-androideabi$(API)-clang)

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

define CLEAR_DF_1_PIE
f=$$1
u() { od -An -t u$$2 -j $$1 -N $$2 "$$f" | tr -d ' \n'; }
phoff=$$(u 28 4); phent=$$(u 42 2); phnum=$$(u 44 2); i=0
while [ $$i -lt $$phnum ]; do
  ph=$$((phoff + i * phent))
  if [ "$$(u $$ph 4)" = 2 ]; then
    dyn=$$(u $$((ph + 4)) 4); end=$$((dyn + $$(u $$((ph + 16)) 4)))
    while [ $$dyn -lt $$end ]; do
      if [ "$$(u $$dyn 4)" = 1879048187 ]; then
        b=$$(u $$((dyn + 7)) 1)
        printf "\\$$(printf %03o $$((b & 247)))" | dd of="$$f" bs=1 seek=$$((dyn + 7)) conv=notrunc 2>/dev/null
      fi
      dyn=$$((dyn + 8))
    done
  fi
  i=$$((i + 1))
done
endef
export CLEAR_DF_1_PIE

.PHONY: all clean info
all: $(BIN)

$(BIN): $(SRCS) $(HDRS) | $(OUTDIR)
	$(TARGET_CC) $(CFLAGS) $(SRCS) -o $@
	@sh -c "$$CLEAR_DF_1_PIE" sh $@
	@$(SHA256SUM) $@

$(OUTDIR):
	mkdir -p $@

info:
	@echo "VERSION=$(VERSION)"
	@echo "NDK=$(NDK)"
	@echo "TARGET_CC=$(TARGET_CC)"
	@echo "BIN=$(BIN)"

clean:
	rm -rf $(OUTDIR) dist
