/*
 * Copyright (C) 2025 Sylas Hollander.
 * PURPOSE: ATVLib main header file.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <efi.h>
#include <efilib.h>
#include <AppleScreen.h>
#include "types.h"
#include "cons.h"
#include "baselibc_string.h"
#include "boot_args.h"
#include "tinyprintf.h"
#include "debug.h"

#define DEBUG_CODE_BEGIN()
#define DEBUG_CODE_END()

extern EFI_HANDLE           gImageHandle;

extern mach_boot_args_t     *gBA;
extern boolean_t            verbose;
extern noreturn void halt(void);

typedef EFI_STATUS RETURN_STATUS;