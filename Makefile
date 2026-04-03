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
LD := /opt/cross/bin/i386-apple-darwin8-ld

INCLUDES := -Iinclude -Ignu-efi/inc #-Iudk/Include -Iudk/Include/Ia32

CFLAGS := -Wall \
			-Werror \
			-fno-stack-protector \
			-Wno-incompatible-library-redeclaration \
			-nostdlib \
			-fno-builtin \
			-O0 \
			-std=gnu11 \
			--target=$(TARGET) \
			$(INCLUDES) \
			-fshort-wchar \
			-mno-red-zone \
			-isysroot $(SDK) \
			-DEFI_DEBUG

OBJS_GNUEFI_LIB := boxdraw.o \
					cmdline.o \
					console.o \
					crc.o \
					data.o \
					debug.o \
					dpath.o \
					error.o \
					event.o \
					exit.o \
					guid.o \
					hand.o \
					hw.o \
					init.o \
					lock.o \
					misc.o \
					pause.o \
					print.o \
					smbios.o \
					sread.o \
					str.o \
					runtime/efirtlib.o \
					runtime/rtdata.o \
					runtime/rtlock.o \
					runtime/rtstr.o \
					runtime/vm.o \
					ia32/initplat.o \
					ia32/math.o
OBJS := main.o \
		baselibc_string.o \
		memory.o \
		runtime_override.o \
		pecoff.o \
		$(addprefix gnu-efi/lib/, $(OBJS_GNUEFI_LIB))

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
ASD: $(OBJS)
	$(LD) -syslibroot $(SDK) -bundle -lSystem -e _main $^ -o $@
all: ASD

clean:
	rm -f $(OBJS) ASD