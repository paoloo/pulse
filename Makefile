# Copyright (c) 2026 Paolo Oliveira. All rights reserved.
# Licensed under the MIT License. See LICENSE file in the project root for details.
#
# Makefile - Pulse hosted unit tests (GCC)
#
# Usage:
#   make				  # builds all tests
#   make run			  # runs all tests
#   make <target>		  # builds specific target: test_pulse or test_telemetry
#   make clean
#
# Override compile-time config, e.g.:
#   make clean && make CDEFS="-DPULSE_CFG_RUN_IMMEDIATELY=0 -DPULSE_MAX_TASKS=4"

CC      := gcc
CSTD    := -std=c11
CWARN   := -Wall -Wextra -Wpedantic
COPT    := -O0 -g0
CDEFS   ?=

INCLUDES := -Isrc

CFLAGS  := $(CSTD) $(CWARN) $(COPT) $(CDEFS) $(INCLUDES)

TEST_PULSE_TARGET     := test_pulse
TEST_TELEMETRY_TARGET := test_telemetry

TEST_PULSE_SRCS       := test/test_pulse.c
TEST_TELEMETRY_SRCS   := test/test_telemetry.c

HEADERS := \
	src/pulse.h \
	src/pulse_version.h \
	src/pulse_port_host.h

.PHONY: all run clean

all: $(TEST_PULSE_TARGET) $(TEST_TELEMETRY_TARGET)

$(TEST_PULSE_TARGET): $(TEST_PULSE_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(TEST_PULSE_SRCS) -o $(TEST_PULSE_TARGET)

$(TEST_TELEMETRY_TARGET): $(TEST_TELEMETRY_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(TEST_TELEMETRY_SRCS) -o $(TEST_TELEMETRY_TARGET)

run: all
	./$(TEST_PULSE_TARGET)
	./$(TEST_TELEMETRY_TARGET)

clean:
	rm -f $(TEST_PULSE_TARGET) $(TEST_TELEMETRY_TARGET)
	rm -rf $(TEST_PULSE_TARGET).dSYM $(TEST_TELEMETRY_TARGET).dSYM


# ---- Notes for MCU builds (not executed) ----
# AVR example (you'll need avr-gcc toolchain):
#   avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL ... main.c -o main.elf
#
# MSP430 example (msp430-gcc toolchain):
#   msp430-elf-gcc -mmcu=msp430g2553 ... main.c -o main.elf
