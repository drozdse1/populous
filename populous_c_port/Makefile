CC = gcc
SDL_CONFIG ?= sdl2-config
CPPFLAGS += $(shell $(SDL_CONFIG) --cflags)
CFLAGS ?= -Wall -Wextra -std=c11 -O2
LDLIBS += $(shell $(SDL_CONFIG) --libs)
TEST_TIMEOUT ?= 20s
SANITIZER_FLAGS = -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

TARGET = populous_init_port
SRCS = populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c
CPPFLAGS += -DPOPULOUS_DEFAULT_ASSET_DIR='"$(abspath ../binaries/populous-extracted)"'
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)
RUNTIME_SOURCES = tests/test_runtime.c populous_init.c sdl_lowlevel.c $(HEADERS)

.PHONY: all clean run test-runtime test test-sanitize
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

tests/test_runtime: $(RUNTIME_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

tests/test_runtime_sanitize: $(RUNTIME_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

define runtime_checks
	SDL_VIDEODRIVER=dummy timeout $(TEST_TIMEOUT) $(1) screen
	SDL_VIDEODRIVER=dummy timeout $(TEST_TIMEOUT) $(1) failures
	SDL_VIDEODRIVER=dummy timeout $(TEST_TIMEOUT) $(1) launcher
	SDL_VIDEODRIVER=populous-invalid-driver timeout $(TEST_TIMEOUT) $(1) invalid-driver
endef

test-runtime: tests/test_runtime
	$(call runtime_checks,./tests/test_runtime)

test: test-runtime

test-sanitize: tests/test_runtime_sanitize
	$(call runtime_checks,./tests/test_runtime_sanitize)

# Only activate game checks once the core step supplies real test sources.
ifneq ($(wildcard tests/test_game.c),)
.PHONY: test-game
tests/test_game: tests/test_game.c populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< sdl_lowlevel.c land_data.c level_data.c graphics.c -o $@ $(LDLIBS)

tests/test_game_sanitize: tests/test_game.c populous_init.c populous_prg.c sdl_lowlevel.c land_data.c level_data.c graphics.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< sdl_lowlevel.c land_data.c level_data.c graphics.c -o $@ $(LDLIBS)

test-game: tests/test_game
	SDL_VIDEODRIVER=dummy timeout $(TEST_TIMEOUT) ./tests/test_game

test: test-game

test-sanitize: test-game-sanitize
.PHONY: test-game-sanitize
test-game-sanitize: tests/test_game_sanitize
	SDL_VIDEODRIVER=dummy timeout $(TEST_TIMEOUT) ./tests/test_game_sanitize
endif

PYTHON ?= python3
.PHONY: test-land test-land-sanitize test-land-local

tests/test_land: tests/test_land.c land_data.c land_data.h
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_land.c land_data.c -o $@

tests/test_land_sanitize: tests/test_land.c land_data.c land_data.h
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) tests/test_land.c land_data.c -o $@

test-land: tests/test_land
	timeout $(TEST_TIMEOUT) ./tests/test_land
	PYTHONDONTWRITEBYTECODE=1 timeout $(TEST_TIMEOUT) $(PYTHON) tests/test_extract_land.py

test-land-sanitize: tests/test_land_sanitize
	timeout $(TEST_TIMEOUT) ./tests/test_land_sanitize

# Explicit opt-in; never reads original data during the portable synthetic suite.
test-land-local: tests/test_land
	@test -n "$(LAND_DATA_DIR)" || { printf '%s\n' 'Set LAND_DATA_DIR to your external land0..3 directory'; exit 1; }
	timeout $(TEST_TIMEOUT) ./tests/test_land "$(LAND_DATA_DIR)"

.PHONY: test-assets
test-assets:
	PYTHONDONTWRITEBYTECODE=1 timeout $(TEST_TIMEOUT) $(PYTHON) tests/test_extract_assets.py

test: test-land test-assets test-graphics
test-sanitize: test-land-sanitize test-graphics-sanitize

.PHONY: test-graphics test-graphics-sanitize test-graphics-local
tests/test_graphics: tests/test_graphics.c graphics.c graphics.h
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_graphics.c graphics.c -o $@
tests/test_graphics_sanitize: tests/test_graphics.c graphics.c graphics.h
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) tests/test_graphics.c graphics.c -o $@
test-graphics: tests/test_graphics
	timeout $(TEST_TIMEOUT) ./tests/test_graphics
test-graphics-sanitize: tests/test_graphics_sanitize
	timeout $(TEST_TIMEOUT) ./tests/test_graphics_sanitize
test-graphics-local: tests/test_graphics
	@test -n "$(ASSET_DIR)" || { printf '%s\n' 'Set ASSET_DIR to an extract_assets.py export'; exit 1; }
	timeout $(TEST_TIMEOUT) ./tests/test_graphics "$(ASSET_DIR)"

clean:
	rm -f $(OBJS) $(TARGET) tests/test_runtime tests/test_runtime_sanitize tests/test_game tests/test_game_sanitize tests/test_land tests/test_land_sanitize tests/test_graphics tests/test_graphics_sanitize tests/test_level tests/test_level_sanitize

run: $(TARGET)
	./$(TARGET)


.PHONY: test-level test-level-sanitize test-level-local
tests/test_level: tests/test_level.c level_data.c level_data.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< level_data.c -o $@
tests/test_level_sanitize: tests/test_level.c level_data.c level_data.h
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(LDFLAGS) $< level_data.c -o $@
test-level: tests/test_level
	timeout $(TEST_TIMEOUT) ./tests/test_level
test-level-sanitize: tests/test_level_sanitize
	timeout $(TEST_TIMEOUT) ./tests/test_level_sanitize
test: test-level
test-sanitize: test-level-sanitize
# All 495 original starts load assets twice and simulate 100 ticks each.
LEVEL_TEST_TIMEOUT ?= 120s
test-level-local: tests/test_game
	@test -n "$(LEVEL_DATA_DIR)" || { printf '%s\n' 'Set LEVEL_DATA_DIR to an external raw data directory'; exit 1; }
	SDL_VIDEODRIVER=dummy timeout $(LEVEL_TEST_TIMEOUT) ./tests/test_game --levels "$(LEVEL_DATA_DIR)"
