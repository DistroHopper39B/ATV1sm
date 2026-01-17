/* Glue for OpenCore compatibility layer */

#include <atvlib.h>
#include <opencore.h>
#include <pecoff.h>
#include <UefiImageFormat.h>

#define FORMAT_SUPPORTED(Format, Source)                                                                     \
  ((UEFI_IMAGE_FORMAT_SUPPORT & (1U << (Format))) != 0 &&                                                    \
   (((Source) == UEFI_IMAGE_SOURCE_NON_FV && (UEFI_IMAGE_FORMAT_SUPPORT_NON_FV & (1U << (Format))) != 0) ||  \
    ((Source) == UEFI_IMAGE_SOURCE_FV && (UEFI_IMAGE_FORMAT_SUPPORT_FV & (1U << (Format))) != 0)))

RETURN_STATUS
UefiImageInitializeContextPreHash (
        OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        IN  CONST VOID                       *FileBuffer,
        IN  UINT32                           FileSize,
        IN  UEFI_IMAGE_SOURCE                Source,
        IN  UINT8                            ImageOrigin
)
{
    RETURN_STATUS  Status;

#if (UEFI_IMAGE_FORMAT_SUPPORT_SOURCES & (1U << UEFI_IMAGE_SOURCE_NON_FV)) != 0
    ASSERT ((PcdGet8 (PcdUefiImageFormatSupportNonFv) & ~((1ULL << UefiImageFormatMax) - 1ULL)) == 0);
  ASSERT (PcdGet8 (PcdUefiImageFormatSupportNonFv) != 0);
#else
    ASSERT (Source != UEFI_IMAGE_SOURCE_NON_FV);
#endif

#if (UEFI_IMAGE_FORMAT_SUPPORT_SOURCES & (1U << UEFI_IMAGE_SOURCE_FV)) != 0
        ASSERT ((PcdGet8 (PcdUefiImageFormatSupportFv) & ~((1ULL << UefiImageFormatMax) - 1ULL)) == 0);
  ASSERT (PcdGet8 (PcdUefiImageFormatSupportFv) != 0);
#else
    ASSERT (Source != UEFI_IMAGE_SOURCE_FV);
#endif

    Status = EFI_UNSUPPORTED;
/*
    STATIC_ASSERT (
            UefiImageFormatUe == UefiImageFormatMax - 1,
            "Support for more formats needs to be added above."
    );
    */

    if (FORMAT_SUPPORTED (UefiImageFormatUe, Source)) {
        Status = InternalInitializeContextPreHash (
                Context,
                FileBuffer,
                FileSize,
                UefiImageFormatUe,
                ImageOrigin
        );
        if (!RETURN_ERROR (Status)) {
            Context->FormatIndex = UefiImageFormatUe;
        }
    }

    if (RETURN_ERROR (Status) && FORMAT_SUPPORTED (UefiImageFormatPe, Source)) {
        Status = InternalInitializeContextPreHash (
                Context,
                FileBuffer,
                FileSize,
                UefiImageFormatPe,
                ImageOrigin
        );
        if (!RETURN_ERROR (Status)) {
            Context->FormatIndex = UefiImageFormatPe;
        }
    }

    return Status;
}

RETURN_STATUS
UefiImageInitializeContext (
        OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        IN  CONST VOID                       *FileBuffer,
        IN  UINT32                           FileSize,
        IN  UEFI_IMAGE_SOURCE                Source,
        IN  UINT8                            ImageOrigin
)
{
    RETURN_STATUS Status;

    Status = UefiImageInitializeContextPreHash (
            Context,
            FileBuffer,
            FileSize,
            Source,
            ImageOrigin
    );
    if (RETURN_ERROR (Status)) {
        return Status;
    }

    return UefiImageInitializeContextPostHash (Context);
}