/*
 * Copyright (C) 2025 Sylas Hollander.
 * PURPOSE: General purpose video functions for linear framebuffers
 * SPDX-License-Identifier: MIT
*/

#pragma once

#define true 1
#define false 0

#define MAX_UINT64 ((UINT64)0xFFFFFFFFFFFFFFFFULL)
#define MAX_UINT32 ((UINT32)0xFFFFFFFF)
#define MAX_UINT16  ((UINT16)0xFFFF)
#define MAX_UINT8   ((UINT8)0xFF)


typedef char                int8_t;
typedef unsigned char       uint8_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef int                 int32_t;
typedef unsigned int        uint32_t;
typedef long long           __attribute__((aligned(8))) int64_t;
typedef unsigned long long  __attribute__((aligned(8))) uint64_t;
typedef _Bool               boolean_t;
typedef __SIZE_TYPE__       size_t;
typedef __UINTPTR_TYPE__    uintptr_t;
typedef long                intptr_t;

#define MIN(a, b)                       \
  (((a) < (b)) ? (a) : (b))

#define  BASE_1KB    0x00000400
#define  BASE_2KB    0x00000800
#define  BASE_4KB    0x00001000
#define  BASE_8KB    0x00002000
#define  BASE_16KB   0x00004000
#define  BASE_32KB   0x00008000
#define  BASE_64KB   0x00010000
#define  BASE_128KB  0x00020000
#define  BASE_256KB  0x00040000
#define  BASE_512KB  0x00080000
#define  BASE_1MB    0x00100000
#define  BASE_2MB    0x00200000
#define  BASE_4MB    0x00400000
#define  BASE_8MB    0x00800000
#define  BASE_16MB   0x01000000
#define  BASE_32MB   0x02000000
#define  BASE_64MB   0x04000000
#define  BASE_128MB  0x08000000
#define  BASE_256MB  0x10000000
#define  BASE_512MB  0x20000000

#define noreturn            _Noreturn
typedef _Bool               boolean_t;
