# POPULOUS_LOG_LEVEL=trace|debug|info|warn|error|off  POPULOUS_LOG_TIMESTAMPS=1
# Portable defaults: `cc` is gcc on Linux and Apple clang on macOS.
# Override with e.g. `make CC=gcc` or `make CC=clang`.
ifeq ($(origin CC),default)
CC = cc
endif
UNAME_S := $(shell uname -s)
# SDL2 flags: prefer sdl2-config, fall back to pkg-config (Homebrew/MacPorts).
SDL_CONFIG ?= $(shell command -v sdl2-config 2>/dev/null)
ifneq ($(strip $(SDL_CONFIG)),)
SDL_CFLAGS ?= $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS ?= $(shell $(SDL_CONFIG) --libs)
else
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
endif
CPPFLAGS += $(SDL_CFLAGS)
ifeq ($(UNAME_S),Darwin)
# _POSIX_C_SOURCE in the sources hides BSD/Darwin extensions SDL headers use.
CPPFLAGS += -D_DARWIN_C_SOURCE
endif
CFLAGS ?= -Wall -Wextra -std=c11 -O2
LDLIBS += $(SDL_LIBS)
TEST_TIMEOUT ?= 20s
# `timeout` is GNU coreutils; macOS has none by default (brew coreutils: gtimeout).
# Without either, tests simply run without a time limit.
TIMEOUT_BIN ?= $(firstword $(shell command -v timeout 2>/dev/null) $(shell command -v gtimeout 2>/dev/null))
with_timeout = $(if $(TIMEOUT_BIN),$(TIMEOUT_BIN) $(1))
SANITIZER_FLAGS = -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

TARGET = populous_init_port
SRCS = populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c
# populous_prg.c is a unity build of these fragments (kept for test #include).
PRG_MODULES = modules/01_preamble.c modules/02_rules.c modules/03_ui_save.c modules/04_render_input.c modules/05_serial.c a4_state.h
CPPFLAGS += -DPOPULOUS_DEFAULT_ASSET_DIR='"$(abspath ../binaries/populous-extracted)"'
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)
RUNTIME_SOURCES = tests/test_runtime.c populous_init.c sdl_lowlevel.c graphics.c audio.c title_music.c intro_data.c pop_log.c $(HEADERS)
# The launcher's asset, mixer and title replayer units; the test includes the rest.
RUNTIME_LINK = graphics.c audio.c title_music.c intro_data.c pop_log.c

.PHONY: all clean run test-runtime test test-sanitize
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

populous_prg.o: populous_prg.c $(HEADERS) $(PRG_MODULES)

tests/test_runtime: $(RUNTIME_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(RUNTIME_LINK) -o $@ $(LDLIBS)

tests/test_runtime_sanitize: $(RUNTIME_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< $(RUNTIME_LINK) -o $@ $(LDLIBS)

define runtime_checks
	SDL_VIDEODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) screen
	SDL_VIDEODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) failures
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) launcher
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) intro
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) cli
	SDL_VIDEODRIVER=populous-invalid-driver $(call with_timeout,$(TEST_TIMEOUT)) $(1) invalid-driver
endef

test-runtime: tests/test_runtime
	$(call runtime_checks,./tests/test_runtime)

test: test-runtime

test-sanitize: tests/test_runtime_sanitize
	$(call runtime_checks,./tests/test_runtime_sanitize)

# Only activate game checks once the core step supplies real test sources.
ifneq ($(wildcard tests/test_game.c),)
.PHONY: test-game
tests/test_game: tests/test_game.c populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c $(HEADERS) $(PRG_MODULES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c -o $@ $(LDLIBS)

tests/test_game_sanitize: tests/test_game.c populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c $(HEADERS) $(PRG_MODULES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c -o $@ $(LDLIBS)

test-game: tests/test_game
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_game

test: test-game

test-sanitize: test-game-sanitize
.PHONY: test-game-sanitize
test-game-sanitize: tests/test_game_sanitize
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_game_sanitize
endif

PYTHON ?= python3
.PHONY: test-land test-land-sanitize test-land-local

tests/test_land: tests/test_land.c land_data.c land_data.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_land.c land_data.c -o $@

tests/test_land_sanitize: tests/test_land.c land_data.c land_data.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) tests/test_land.c land_data.c -o $@

test-land: tests/test_land
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_land
	PYTHONDONTWRITEBYTECODE=1 $(call with_timeout,$(TEST_TIMEOUT)) $(PYTHON) tests/test_extract_land.py

test-land-sanitize: tests/test_land_sanitize
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_land_sanitize

# Explicit opt-in; never reads original data during the portable synthetic suite.
test-land-local: tests/test_land
	@test -n "$(LAND_DATA_DIR)" || { printf '%s\n' 'Set LAND_DATA_DIR to your external land0..3 directory'; exit 1; }
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_land "$(LAND_DATA_DIR)"

.PHONY: test-assets
test-assets:
	PYTHONDONTWRITEBYTECODE=1 $(call with_timeout,$(TEST_TIMEOUT)) $(PYTHON) tests/test_extract_assets.py

test: test-land test-assets test-graphics
test-sanitize: test-land-sanitize test-graphics-sanitize

.PHONY: test-graphics test-graphics-sanitize test-graphics-local
tests/test_graphics: tests/test_graphics.c graphics.c graphics.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_graphics.c graphics.c -o $@
tests/test_graphics_sanitize: tests/test_graphics.c graphics.c graphics.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) tests/test_graphics.c graphics.c -o $@
test-graphics: tests/test_graphics
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_graphics
test-graphics-sanitize: tests/test_graphics_sanitize
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_graphics_sanitize
test-graphics-local: tests/test_graphics
	@test -n "$(ASSET_DIR)" || { printf '%s\n' 'Set ASSET_DIR to an extract_assets.py export'; exit 1; }
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_graphics "$(ASSET_DIR)"

clean:
	rm -f $(OBJS) $(TARGET) tests/test_runtime tests/test_runtime_sanitize tests/test_game tests/test_game_sanitize tests/test_land tests/test_land_sanitize tests/test_graphics tests/test_graphics_sanitize tests/test_level tests/test_level_sanitize tests/test_audio tests/test_audio_sanitize tests/test_net tests/test_net_sanitize

run: $(TARGET)
	./$(TARGET)


.PHONY: test-level test-level-sanitize test-level-local
tests/test_level: tests/test_level.c level_data.c level_data.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< level_data.c -o $@
tests/test_level_sanitize: tests/test_level.c level_data.c level_data.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< level_data.c -o $@
test-level: tests/test_level
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_level
test-level-sanitize: tests/test_level_sanitize
	$(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_level_sanitize
test: test-level
test-sanitize: test-level-sanitize
# All 495 original starts load assets twice and simulate 100 ticks each.
LEVEL_TEST_TIMEOUT ?= 120s
test-level-local: tests/test_game
	@test -n "$(LEVEL_DATA_DIR)" || { printf '%s\n' 'Set LEVEL_DATA_DIR to an external raw data directory'; exit 1; }
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(LEVEL_TEST_TIMEOUT)) ./tests/test_game --levels "$(LEVEL_DATA_DIR)"

# Mixer, _load_sound and popsam parsers, title replayer; the device check uses SDL's dummy driver.
.PHONY: test-audio test-audio-sanitize
tests/test_audio: tests/test_audio.c audio.c audio.h title_music.c title_music.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< audio.c title_music.c pop_log.c -o $@ $(LDLIBS)
tests/test_audio_sanitize: tests/test_audio.c audio.c audio.h title_music.c title_music.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< audio.c title_music.c pop_log.c -o $@ $(LDLIBS)
test-audio: tests/test_audio
	SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_audio
test-audio-sanitize: tests/test_audio_sanitize
	SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) ./tests/test_audio_sanitize
test: test-audio
test-sanitize: test-audio-sanitize

# Two-player link: channel, original serial protocol, two processes over 127.0.0.1.
.PHONY: test-net test-net-sanitize
NET_SOURCES = tests/test_net.c populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c serial_link.h $(HEADERS) $(PRG_MODULES)
NET_LINK = sdl_lowlevel.c land_data.c level_data.c graphics.c audio.c title_music.c intro_data.c pop_log.c serial_link.c
tests/test_net: $(NET_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(NET_LINK) -o $@ $(LDLIBS)
tests/test_net_sanitize: $(NET_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< $(NET_LINK) -o $@ $(LDLIBS)
define net_checks
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) channel
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) protocol
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) tcp-full
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(call with_timeout,$(TEST_TIMEOUT)) $(1) tcp-quick
endef
test-net: tests/test_net
	$(call net_checks,./tests/test_net)
test-net-sanitize: tests/test_net_sanitize
	$(call net_checks,./tests/test_net_sanitize)
test: test-net
test-sanitize: test-net-sanitize
