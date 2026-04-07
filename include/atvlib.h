/*
 * Copyright (C) 2025 Sylas Hollander.
 * PURPOSE: ATVLib main header file.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <efi.h>
#include <efilib.h>
#include <AppleScreen.h>
#include "baselibc_string.h"

#define DEBUG_CODE_BEGIN()
#define DEBUG_CODE_END()

extern EFI_HANDLE           gImageHandle;

extern VOID PatchSystemTable(EFI_SYSTEM_TABLE *SystemTable);

typedef EFI_STATUS RETURN_STATUS;
typedef __SIZE_TYPE__ size_t;