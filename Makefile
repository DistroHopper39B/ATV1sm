#
# Copyright (C) 2025 Sylas Hollander.
# PURPOSE: Makefile for Apple TV bare-metal programs
# SPDX-License-Identifier: MIT
#

# Check what OS we're running. Should work on Linux and macOS.
OSTYPE = $(shell uname)

# Target defs for Linux cross compiler.
TARGET = i386-apple-darwin8

# SDK
SDK := $(shell pwd)/MacOSX10.4u.sdk

# Definitions for compiler
CC := /usr/bin/clang

INCLUDES := -Iinclude -Ignu-efi/inc #-Iudk/Include -Iudk/Include/Ia32

CFLAGS := -Wall -Werror -fno-stack-protector -nostdlib -fno-builtin -O0 -std=gnu11 --target=$(TARGET) $(INCLUDES) -fshort-wchar -mno-red-zone -isysroot $(SDK) -DEFI_DEBUG

OBJS_GNUEFI_LIB := boxdraw.o cmdline.o console.o crc.o data.o debug.o dpath.o error.o event.o exit.o guid.o hand.o hw.o init.o lock.o misc.o pause.o print.o smbios.o sread.o str.o runtime/efirtlib.o runtime/rtdata.o runtime/rtlock.o runtime/rtstr.o runtime/vm.o ia32/initplat.o ia32/math.o
OBJS_OPENCORE_LIB := ImageLoader.o fileload.o OpenFile.o FileProtocol.o GetFileInfo.o MachoFat.o
OBJS_UDK_LIB :=
OBJS := main.o baselibc_string.o cons.o tinyprintf.o $(addprefix gnu-efi/lib/, $(OBJS_GNUEFI_LIB)) #$(addprefix opencore/, $(OBJS_OPENCORE_LIB))

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
ASD: $(OBJS)
	$(CC) $(CFLAGS) -Wl,-bundle -Wl,-lSystem -e _main $^ -o $@
all: ASD

clean:
	rm -f $(OBJS) ASD