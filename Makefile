# Copyright (c) 2026 Paolo Oliveira. All rights reserved.
# Licensed under the MIT License. See LICENSE file in the project root for details.
#
# Makefile - Pulse hosted unit tests (GCC)
#
# Usage:
#   make
#   make run
#   make clean
#
# Override compile-time config, e.g.:
#   make clean && make CDEFS="-DPULSE_CFG_RUN_IMMEDIATELY=0 -DPULSE_MAX_TASKS=4"

CC      := gcc
CSTD    := -std=c11
CWARN   := -Wall -Wextra -Wpedantic
COPT    := -O0 -g0
CDEFS   ?=
CFLAGS  := $(CSTD) $(CWARN) $(COPT) $(CDEFS)

TARGET  := test_pulse
SRCS    := test/test_pulse.c

HEADERS := \
	src/pulse.h \
	src/pulse_version.h \
	src/pulse_port_host.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM

# ---- Notes for MCU builds (not executed) ----
# AVR example (you'll need avr-gcc toolchain):
#   avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL ... main.c -o main.elf
#
# MSP430 example (msp430-gcc toolchain):
#   msp430-elf-gcc -mmcu=msp430g2553 ... main.c -o main.elf
