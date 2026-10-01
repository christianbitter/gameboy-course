# Makefile - for machines that have make (WSL, Linux, macOS, MSYS2 with make).
# On Windows without make, use build.ps1 instead; it needs only gcc.

CC      ?= gcc
CFLAGS  ?= -std=c17 -O0 -g -Wall -Wextra -Wshadow -Wvla \
           -Wno-unused-parameter -Wno-unused-function
INC     := -Iinclude
BUILD   := build

SRC         := $(wildcard src/*.c)
SRC_NO_MAIN := $(filter-out src/main.c,$(SRC))
TEST_SRC    := $(wildcard tests/*.c)

.PHONY: all test clean run

all: $(BUILD)/gbemu.exe

$(BUILD)/gbemu.exe: $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC)

$(BUILD)/gbemu_tests.exe: $(SRC_NO_MAIN) $(TEST_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC_NO_MAIN) $(TEST_SRC)

$(BUILD):
	mkdir -p $(BUILD)

# `make test` or `make test FILTER=m03`
test: $(BUILD)/gbemu_tests.exe
	./$(BUILD)/gbemu_tests.exe $(FILTER)

# `make run ROM=roms/tetris.gb FRAMES=600`
run: $(BUILD)/gbemu.exe
	./$(BUILD)/gbemu.exe --rom $(ROM) --frames $(FRAMES) --dump-frame $(BUILD)/frame.bmp

clean:
	rm -rf $(BUILD)
