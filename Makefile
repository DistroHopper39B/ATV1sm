#
# Copyright (C) 2025 Sylas Hollander.
# PURPOSE: Makefile for Apple TV bare-metal programs
# SPDX-License-Identifier: MIT
#

.NOTPARALLEL: libSystem.dylib ASD

# Check what OS we're running. Should work on Linux and macOS.
OSTYPE = $(shell uname)

# Target defs for Linux cross compiler.
TARGET = i386-apple-darwin8

# Definitions for compiler
CC := clang
LD := /opt/cross/bin/i386-apple-darwin8-ld
NASM := nasm

INCLUDES := -Iinclude -Ignu-efi/inc

CFLAGS := 	-Wall \
			-Wextra \
			-Werror \
			-pedantic \
			-std=c11 \
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
		longjmp.o \
		exit-asm.o \
		$(addprefix gnu-efi/lib/, $(OBJS_GNUEFI_LIB))


%.o: %.asm
	$(NASM) -fmacho32 $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ASD: $(OBJS)
	$(LD) -bundle -e _main $^ -o $@
all: ASD

clean:
	rm -f $(OBJS) ASD