#
# Copyright (C) 2025 Sylas Hollander.
# PURPOSE: Makefile for Apple TV bare-metal programs
# SPDX-License-Identifier: MIT
#

# Check what OS we're running. Should work on Linux and macOS.
OSTYPE = $(shell uname)

# Target defs for Linux cross compiler.
TARGET = i386-apple-darwin8

# Definitions for compiler
CC := /usr/bin/clang

# Flags for mach-o linker

CFLAGS := -Wall -Werror -nostdlib -fno-stack-protector -fno-builtin -O0 -std=gnu11 --target=$(TARGET) -Iinclude -Iinclude/efi/Ia32 -Iinclude/efi -fshort-wchar

OBJS := main.o baselibc_string.o cons.o tinyprintf.o

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
ASD: $(OBJS)
	$(CC) $(CFLAGS) -e _main $^ -o $@
all: ASD

clean:
	rm -f *.o mach_kernel