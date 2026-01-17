/*
 * Copyright (C) 2025 Sylas Hollander.
 * PURPOSE: ATVLib main header file.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "types.h"
#include "cons.h"
#include "baselibc_string.h"
#include "boot_args.h"
#include "tinyprintf.h"
#include "debug.h"
#include <Uefi.h>
#include <Uefi/UefiSpec.h>
#include <AppleScreenInfo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

extern mach_boot_args_t     *gBA;
extern boolean_t            verbose;
extern noreturn void halt(void);