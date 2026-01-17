/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Uefi.h>

#include <Guid/FileInfo.h>

#include <Protocol/SimpleFileSystem.h>

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
//#include <Library/BaseOverflowLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
//#include <Library/OcDevicePathLib.h>
//#include <Library/OcFileLib.h>


//#include <atvlib.h>
#include <opencore.h>

BOOLEAN
BaseOverflowAddU32 (
        UINT32  A,
        UINT32  B,
        UINT32  *Result
)
{
#if defined (BASE_HAS_TYPE_GENERIC_BUILTINS)
    return __builtin_add_overflow (A, B, Result);
#elif defined (BASE_HAS_TYPE_SPECIFIC_BUILTINS_64)
    return __builtin_uadd_overflow (A, B, Result);
#else
    UINT32  Temp;

    //
    // I believe casting will be faster on X86 at least.
    //
    Temp    = A + B;
    *Result = Temp;
    if (Temp >= A) {
        return FALSE;
    }

    return TRUE;
#endif
}

BOOLEAN
BaseOverflowAddU64 (
        UINT64  A,
        UINT64  B,
        UINT64  *Result
)
{
#if defined (BASE_HAS_TYPE_GENERIC_BUILTINS)
    return __builtin_add_overflow (A, B, Result);
#elif defined (BASE_HAS_TYPE_SPECIFIC_BUILTINS)
    return __builtin_uaddll_overflow (A, B, Result);
#else
    UINT64  Temp;

    Temp    = A + B;
    *Result = Temp;
    if (Temp >= A) {
        return FALSE;
    }

    return TRUE;
#endif
}

BOOLEAN
BaseOverflowAddUN (
        UINTN  A,
        UINTN  B,
        UINTN  *Result
)
{
#if defined (BASE_HAS_TYPE_GENERIC_BUILTINS)
    return __builtin_add_overflow (A, B, Result);
#elif defined (BASE_HAS_TYPE_SPECIFIC_BUILTINS_64)
    return __builtin_uaddll_overflow (A, B, Result);
#elif defined (BASE_HAS_TYPE_SPECIFIC_BUILTINS_32)
    return __builtin_uadd_overflow (A, B, Result);
#else
    if (sizeof (UINTN) == sizeof (UINT64)) {
        return BaseOverflowAddU64 (A, B, (UINT64 *)Result);
    }

    return BaseOverflowAddU32 ((UINT32)A, (UINT32)B, (UINT32 *)Result);
#endif
}

VOID *
OcGetFileInfo (
  IN  EFI_FILE_PROTOCOL  *File,
  IN  EFI_GUID           *InformationType,
  IN  UINTN              MinFileInfoSize,
  OUT UINTN              *RealFileInfoSize  OPTIONAL
  )
{
  VOID  *FileInfoBuffer;

  UINTN       FileInfoSize;
  EFI_STATUS  Status;

  FileInfoSize   = 0;
  FileInfoBuffer = NULL;

  Status = File->GetInfo (
                   File,
                   InformationType,
                   &FileInfoSize,
                   NULL
                   );

  if ((Status == EFI_BUFFER_TOO_SMALL) && (FileInfoSize >= MinFileInfoSize)) {
    //
    // Some drivers (i.e. built-in 32-bit Apple HFS driver) may possibly omit null terminators from file info data.
    //
    if (CompareGuid (InformationType, &gEfiFileInfoGuid) && BaseOverflowAddUN (FileInfoSize, sizeof (CHAR16), &FileInfoSize)) {
      return NULL;
    }

    FileInfoBuffer = AllocateZeroPool (FileInfoSize);

    if (FileInfoBuffer != NULL) {
      Status = File->GetInfo (
                       File,
                       InformationType,
                       &FileInfoSize,
                       FileInfoBuffer
                       );

      if (!EFI_ERROR (Status)) {
        if (RealFileInfoSize != NULL) {
          *RealFileInfoSize = FileInfoSize;
        }
      } else {
        FreePool (FileInfoBuffer);

        FileInfoBuffer = NULL;
      }
    }
  }

  return FileInfoBuffer;
}
