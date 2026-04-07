
#include <atv1sm.h>
#include <Library/PeCoffLib2.h>
#include <Library/UefiImageLib.h>

#define IMAGE_CERTIFICATE_ALIGN  8U

BOOLEAN
BaseOverflowAddU32 (
        UINT32  A,
        UINT32  B,
        UINT32  *Result
);
BOOLEAN
BaseOverflowSubU32 (
        UINT32  A,
        UINT32  B,
        UINT32  *Result
);

struct PE_COFF_LOADER_RUNTIME_CONTEXT_ {
    ///
    /// The RVA of the Relocation Directory.
    ///
    UINT32 RelocDirRva;
    ///
    /// The size, in Bytes, of the Relocation Directory.
    ///
    UINT32 RelocDirSize;
    ///
    /// Information bookkept during the initial Image relocation.
    ///
    UINT64 FixupData[];
};

STATIC
RETURN_STATUS
InternalVerifySections (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        IN     UINT32                        FileSize,
        OUT    UINT32                        *StartAddress
)
{
    BOOLEAN                        Overflow;
    UINT32                         NextSectRva = 0;
    UINT32                         SectRawEnd;
    UINT16                         SectionIndex;
    CONST EFI_IMAGE_SECTION_HEADER *Sections;

    ASSERT (Context != NULL);
    ASSERT (IS_POW2 (Context->SectionAlignment));
    ASSERT (StartAddress != NULL);
    //
    // Images without Sections have no usable data, disallow them.
    //
    if (Context->NumberOfSections == 0) {
        return RETURN_VOLUME_CORRUPTED;
    }

    Sections = (CONST EFI_IMAGE_SECTION_HEADER *) (CONST VOID *) (
            (CONST CHAR8 *) Context->FileBuffer + Context->SectionsOffset
    );
    //
    // The first Image section must begin the Image memory space, or it must be
    // adjacent to the Image Headers.
    //
    if (Sections[0].VirtualAddress == 0) {
        // FIXME: Add PCD to disallow.
        NextSectRva = 0;
    } else {
        //
        // Choose the raw or aligned Image Headers size depending on whether loading
        // unaligned Sections is allowed.
        //
        /*
        if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_CONTIGUOUS_SECTIONS) == 0) {
            Overflow = BaseOverflowAlignUpU32 (
                    Context->SizeOfHeaders,
                    Context->SectionAlignment,
                    &NextSectRva
            );
            if (Overflow) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        } else {
            NextSectRva = Context->SizeOfHeaders;
        }
         */
    }

    *StartAddress = NextSectRva;
    //
    // Verify all Image sections are valid.
    //
    for (SectionIndex = 0; SectionIndex < Context->NumberOfSections; ++SectionIndex) {
        //
        // Verify the Image section adheres to the W^X principle, if the policy
        // demands it.
        //
        /*
        if (PcdGetBool (PcdImageLoaderWXorX) && !PcdGetBool (PcdImageLoaderRemoveXForWX)) {
            if ((Sections[SectionIndex].Characteristics & (EFI_IMAGE_SCN_MEM_EXECUTE | EFI_IMAGE_SCN_MEM_WRITE)) == (EFI_IMAGE_SCN_MEM_EXECUTE | EFI_IMAGE_SCN_MEM_WRITE)) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        }
         */
        //
        // Verify the Image sections are disjoint (relaxed) or adjacent (strict)
        // depending on whether unaligned Image sections may be loaded or not.
        // Unaligned Image sections have been observed with iPXE Option ROMs and old
        // Apple Mac OS X bootloaders.
        //
        /*
        if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_CONTIGUOUS_SECTIONS) == 0) {
            if (Sections[SectionIndex].VirtualAddress != NextSectRva) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        } else {
            if (Sections[SectionIndex].VirtualAddress < NextSectRva) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // If the Image section address is not aligned by the Image section
            // alignment, fall back to important architecture-specific page sizes if
            // possible, to ensure the Image can have memory protection applied.
            // Otherwise, report no alignment for the Image.
            //
            if (!IS_ALIGNED (Sections[SectionIndex].VirtualAddress, Context->SectionAlignment)) {
                STATIC_ASSERT (
                        DEFAULT_PAGE_ALLOCATION_GRANULARITY <= RUNTIME_PAGE_ALLOCATION_GRANULARITY,
                        "This code must be adapted to consider the reversed order."
                );

                if (IS_ALIGNED (Sections[SectionIndex].VirtualAddress, RUNTIME_PAGE_ALLOCATION_GRANULARITY)) {
                    Context->SectionAlignment = RUNTIME_PAGE_ALLOCATION_GRANULARITY;
                } else if (DEFAULT_PAGE_ALLOCATION_GRANULARITY < RUNTIME_PAGE_ALLOCATION_GRANULARITY
                           && IS_ALIGNED (Sections[SectionIndex].VirtualAddress, DEFAULT_PAGE_ALLOCATION_GRANULARITY)) {
                    Context->SectionAlignment = DEFAULT_PAGE_ALLOCATION_GRANULARITY;
                } else {
                    Context->SectionAlignment = 1;
                }
            }
        }
         */
        //
        // Verify the Image sections with data are in bounds of the file buffer.
        //
        if (Sections[SectionIndex].SizeOfRawData > 0) {
            Overflow = BaseOverflowAddU32 (
                    Sections[SectionIndex].PointerToRawData,
                    Sections[SectionIndex].SizeOfRawData,
                    &SectRawEnd
            );
            if (Overflow) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }

            if (SectRawEnd > FileSize) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        }
        //
        // Determine the end of the current Image section.
        //
        Overflow = BaseOverflowAddU32 (
                Sections[SectionIndex].VirtualAddress,
                Sections[SectionIndex].VirtualSize,
                &NextSectRva
        );
        if (Overflow) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // VirtualSize does not need to be aligned, so align the result if needed.
        //
        /*
        if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_CONTIGUOUS_SECTIONS) == 0) {
            Overflow = BaseOverflowAlignUpU32 (
                    NextSectRva,
                    Context->SectionAlignment,
                    &NextSectRva
            );
            if (Overflow) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        }
         */
    }
    //
    // Set SizeOfImage to the aligned end address of the last ImageSection.
    //

    //if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_CONTIGUOUS_SECTIONS) == 0) {
        Context->SizeOfImage = NextSectRva;
    /*} else {
        //
        // Because VirtualAddress is aligned by SectionAlignment for all Image
        // sections, and they are disjoint and ordered by VirtualAddress,
        // VirtualAddress + VirtualSize must be safe to align by SectionAlignment for
        // all but the last Image section.
        // Determine the strictest common alignment that the last section's end is
        // safe to align to.
        //
        Overflow = BaseOverflowAlignUpU32 (
                NextSectRva,
                Context->SectionAlignment,
                &Context->SizeOfImage
        );
        if (Overflow) {
            Context->SectionAlignment = RUNTIME_PAGE_ALLOCATION_GRANULARITY;
            Overflow = BaseOverflowAlignUpU32 (
                    NextSectRva,
                    Context->SectionAlignment,
                    &Context->SizeOfImage
            );
            if (DEFAULT_PAGE_ALLOCATION_GRANULARITY < RUNTIME_PAGE_ALLOCATION_GRANULARITY
                && Overflow) {
                Context->SectionAlignment = DEFAULT_PAGE_ALLOCATION_GRANULARITY;
                Overflow = BaseOverflowAlignUpU32 (
                        NextSectRva,
                        Context->SectionAlignment,
                        &Context->SizeOfImage
                );
            }

            if (Overflow) {
                Context->SectionAlignment = 1;
            }
        }

    }
     */

    return RETURN_SUCCESS;
}

STATIC
RETURN_STATUS
InternalValidateRelocInfo (
        IN CONST PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        IN UINT32                              StartAddress
)
{
    BOOLEAN Overflow;
    UINT32  SectRvaEnd;

    ASSERT (Context != NULL);
    ASSERT (!Context->RelocsStripped || Context->RelocDirSize == 0);
    //
    // If the Base Relocations have not been stripped, verify their Directory.
    //
    if (!Context->RelocsStripped && Context->RelocDirSize != 0) {
        //
        // Verify the Relocation Directory is not empty.
        //
        if (sizeof (EFI_IMAGE_BASE_RELOCATION_BLOCK) > Context->RelocDirSize) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Relocation Directory does not overlap with the Image Headers.
        //
        if (StartAddress > Context->RelocDirRva) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Relocation Directory is contained in the Image memory space.
        //
        Overflow = BaseOverflowAddU32 (
                Context->RelocDirRva,
                Context->RelocDirSize,
                &SectRvaEnd
        );
        if (Overflow || SectRvaEnd > Context->SizeOfImage) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Relocation Directory start is sufficiently aligned.
        //
        if (!IS_ALIGNED (Context->RelocDirRva, ALIGNOF (EFI_IMAGE_BASE_RELOCATION_BLOCK))) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
    }
    //
    // Verify the preferred Image load address is sufficiently aligned.
    //
    // FIXME: Only with force-aligned sections? What to do with XIP?
    if (!IS_ALIGNED (Context->ImageBase, (UINT64) Context->SectionAlignment)) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }

    return RETURN_SUCCESS;
}

STATIC
RETURN_STATUS
InternalInitializePe (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        IN     UINT32                        FileSize
)
{
    BOOLEAN                               Overflow;
    CONST EFI_IMAGE_NT_HEADERS_COMMON_HDR *PeCommon;
    CONST EFI_IMAGE_NT_HEADERS32          *Pe32;
    CONST EFI_IMAGE_NT_HEADERS64          *Pe32Plus;
    CONST CHAR8                           *OptHdrPtr;
    UINT32                                HdrSizeWithoutDataDir;
    UINT32                                MinSizeOfOptionalHeader;
    UINT32                                MinSizeOfHeaders;
    CONST EFI_IMAGE_DATA_DIRECTORY        *RelocDir;
    CONST EFI_IMAGE_DATA_DIRECTORY        *SecDir;
    UINT32                                SecDirEnd;
    UINT32                                NumberOfRvaAndSizes;
    RETURN_STATUS                         Status;
    UINT32                                StartAddress;

    ASSERT (Context != NULL);
    ASSERT (sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR) + sizeof (UINT16) <= FileSize - Context->ExeHdrOffset);
    /*
    if (!PcdGetBool (PcdImageLoaderAllowMisalignedOffset)) {
        ASSERT (IS_ALIGNED (Context->ExeHdrOffset, ALIGNOF (EFI_IMAGE_NT_HEADERS_COMMON_HDR)));
    }
     */
    //
    // Locate the PE Optional Header.
    //
    OptHdrPtr = (CONST CHAR8 *) Context->FileBuffer + Context->ExeHdrOffset;
    OptHdrPtr += sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR);

    STATIC_ASSERT (
            IS_ALIGNED (ALIGNOF (EFI_IMAGE_NT_HEADERS_COMMON_HDR), ALIGNOF (UINT16))
            && IS_ALIGNED (sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR), ALIGNOF (UINT16)),
            "The following operation might be an unaligned access."
    );
    //
    // Determine the type of and retrieve data from the PE Optional Header.
    // Do not retrieve SizeOfImage as the value usually does not follow the
    // specification. Even if the value is large enough to hold the last Image
    // section, it may not be aligned, or it may be too large. No data can
    // possibly be loaded past the last Image section anyway.
    //
    switch (*(CONST UINT16 *) (CONST VOID *) OptHdrPtr) {
        case EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC:
            //
            // Verify the PE32 header is in bounds of the file buffer.
            //
            if (sizeof (*Pe32) > FileSize - Context->ExeHdrOffset) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // The PE32 header offset is always sufficiently aligned.
            //
            STATIC_ASSERT (
                    ALIGNOF (EFI_IMAGE_NT_HEADERS32) <= ALIGNOF (EFI_IMAGE_NT_HEADERS_COMMON_HDR),
                    "The following operations may be unaligned."
            );
            //
            // Populate the common data with information from the Optional Header.
            //
            Pe32 = (CONST EFI_IMAGE_NT_HEADERS32 *) (CONST VOID *) (
                    (CONST CHAR8 *) Context->FileBuffer + Context->ExeHdrOffset
            );

            Context->ImageType           = PeCoffLoaderTypePe32;
            Context->Subsystem           = Pe32->Subsystem;
            Context->SizeOfHeaders       = Pe32->SizeOfHeaders;
            Context->ImageBase           = Pe32->ImageBase;
            Context->AddressOfEntryPoint = Pe32->AddressOfEntryPoint;
            Context->SectionAlignment    = Pe32->SectionAlignment;

            RelocDir = Pe32->DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC;
            SecDir   = Pe32->DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_SECURITY;

            PeCommon              = &Pe32->CommonHeader;
            NumberOfRvaAndSizes   = Pe32->NumberOfRvaAndSizes;
            HdrSizeWithoutDataDir = sizeof (EFI_IMAGE_NT_HEADERS32) - sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR);

            break;

        case EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC:
            //
            // Verify the PE32+ header is in bounds of the file buffer.
            //
            if (sizeof (*Pe32Plus) > FileSize - Context->ExeHdrOffset) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the PE32+ header offset is sufficiently aligned.
            //
            /*
            if (!PcdGetBool (PcdImageLoaderAllowMisalignedOffset)
                && !IS_ALIGNED (Context->ExeHdrOffset, ALIGNOF (EFI_IMAGE_NT_HEADERS64))) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
             */
            //
            // Populate the common data with information from the Optional Header.
            //
            Pe32Plus = (CONST EFI_IMAGE_NT_HEADERS64 *) (CONST VOID *) (
                    (CONST CHAR8 *) Context->FileBuffer + Context->ExeHdrOffset
            );

            Context->ImageType           = PeCoffLoaderTypePe32Plus;
            Context->Subsystem           = Pe32Plus->Subsystem;
            Context->SizeOfHeaders       = Pe32Plus->SizeOfHeaders;
            Context->ImageBase           = Pe32Plus->ImageBase;
            Context->AddressOfEntryPoint = Pe32Plus->AddressOfEntryPoint;
            Context->SectionAlignment    = Pe32Plus->SectionAlignment;

            RelocDir = Pe32Plus->DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC;
            SecDir   = Pe32Plus->DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_SECURITY;

            PeCommon              = &Pe32Plus->CommonHeader;
            NumberOfRvaAndSizes   = Pe32Plus->NumberOfRvaAndSizes;
            HdrSizeWithoutDataDir = sizeof (EFI_IMAGE_NT_HEADERS64) - sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR);

            break;

        default:
            //
            // Disallow Images with unknown PE Optional Header signatures.
            //
            //DEBUG_RAISE ();
        {
            Print(L"Unknown PE Optional Header sig\n");
            return RETURN_UNSUPPORTED;
        }

    }
    //
    // Disallow Images with unknown directories.
    //
    if (NumberOfRvaAndSizes > EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Verify the Image alignment is a power of 2.
    //
    if (!IS_POW2 (Context->SectionAlignment)) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }

    STATIC_ASSERT (
            sizeof (EFI_IMAGE_DATA_DIRECTORY) <= MAX_UINT32 / EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES,
            "The following arithmetic may overflow."
    );
    //
    // Calculate the offset of the Image sections.
    //
    // Context->ExeHdrOffset + sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR) cannot overflow because
    //   * ExeFileSize > sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR) and
    //   * Context->ExeHdrOffset + ExeFileSize = FileSize
    //
    Overflow = BaseOverflowAddU32 (
            Context->ExeHdrOffset + sizeof (*PeCommon),
            PeCommon->FileHeader.SizeOfOptionalHeader,
            &Context->SectionsOffset
    );
    if (Overflow) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Verify the Section Headers offset is sufficiently aligned.
    //
    /*
    if (!PcdGetBool (PcdImageLoaderAllowMisalignedOffset)
        && !IS_ALIGNED (Context->SectionsOffset, ALIGNOF (EFI_IMAGE_SECTION_HEADER))) {
        DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
     */
    //
    // This arithmetic cannot overflow because all values are sufficiently
    // bounded.
    //
    MinSizeOfOptionalHeader = HdrSizeWithoutDataDir +
                              NumberOfRvaAndSizes * sizeof (EFI_IMAGE_DATA_DIRECTORY);

    ASSERT (MinSizeOfOptionalHeader >= HdrSizeWithoutDataDir);

    STATIC_ASSERT (
            sizeof (EFI_IMAGE_SECTION_HEADER) <= (MAX_UINT32 + 1ULL) / (MAX_UINT16 + 1ULL),
            "The following arithmetic may overflow."
    );
    //
    // Calculate the minimum size of the Image Headers.
    //
    Overflow = BaseOverflowAddU32 (
            Context->SectionsOffset,
            (UINT32) PeCommon->FileHeader.NumberOfSections * sizeof (EFI_IMAGE_SECTION_HEADER),
            &MinSizeOfHeaders
    );
    if (Overflow) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Verify the Image Header sizes are sane. SizeOfHeaders contains all header
    // components (DOS, PE Common and Optional Header).
    //
    if (MinSizeOfOptionalHeader > PeCommon->FileHeader.SizeOfOptionalHeader) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }

    if (MinSizeOfHeaders > Context->SizeOfHeaders) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Verify the Image Headers are in bounds of the file buffer.
    //
    if (Context->SizeOfHeaders > FileSize) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Populate the Image context with information from the Common Header.
    //
    Context->NumberOfSections = PeCommon->FileHeader.NumberOfSections;
    Context->Machine          = PeCommon->FileHeader.Machine;
    Context->RelocsStripped   =
            (
                    PeCommon->FileHeader.Characteristics & EFI_IMAGE_FILE_RELOCS_STRIPPED
            ) != 0;

    if (EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC < NumberOfRvaAndSizes) {
        Context->RelocDirRva  = RelocDir->VirtualAddress;
        Context->RelocDirSize = RelocDir->Size;

        if (Context->RelocsStripped && Context->RelocDirSize != 0) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
    } else {
        ASSERT (Context->RelocDirRva == 0);
        ASSERT (Context->RelocDirSize == 0);
    }

    if (EFI_IMAGE_DIRECTORY_ENTRY_SECURITY < NumberOfRvaAndSizes) {
        Context->SecDirOffset = SecDir->VirtualAddress;
        Context->SecDirSize   = SecDir->Size;
        //
        // Verify the Security Direction is in bounds of the Image buffer.
        //
        Overflow = BaseOverflowAddU32 (
                Context->SecDirOffset,
                Context->SecDirSize,
                &SecDirEnd
        );
        if (Overflow || SecDirEnd > FileSize) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Security Directory is sufficiently aligned.
        //
        if (!IS_ALIGNED (Context->SecDirOffset, IMAGE_CERTIFICATE_ALIGN)) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Security Directory size is sufficiently aligned, and that if
        // it is not empty, it can fit at least one certificate.
        //
        if (Context->SecDirSize != 0
            && (!IS_ALIGNED (Context->SecDirSize, IMAGE_CERTIFICATE_ALIGN)
                || Context->SecDirSize < sizeof (WIN_CERTIFICATE))) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
    } else {
        //
        // The Image context is zero'd on allocation.
        //
        ASSERT (Context->SecDirOffset == 0);
        ASSERT (Context->SecDirSize == 0);
    }
    //
    // Verify the Image sections are Well-formed.
    //
    Status = InternalVerifySections (
            Context,
            FileSize,
            &StartAddress
    );
    if (Status != RETURN_SUCCESS) {
        //DEBUG_RAISE ();
        return Status;
    }
    //
    // Verify the entry point is in bounds of the Image buffer.
    //
    if (Context->AddressOfEntryPoint >= Context->SizeOfImage) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Verify the basic Relocation information is well-formed.
    //
    Status = InternalValidateRelocInfo (Context, StartAddress);
    if (Status != RETURN_SUCCESS) {
        //DEBUG_RAISE ();
    }

    return Status;
}



RETURN_STATUS
PeCoffInitializeContext (
        OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        IN  CONST VOID                    *FileBuffer,
        IN  UINT32                        FileSize
    )
{
    RETURN_STATUS               Status;
    CONST EFI_IMAGE_DOS_HEADER *DosHdr;

    ASSERT (Context != NULL);
    ASSERT (FileBuffer != NULL || FileSize == 0);
    //
    // Initialise the Image context with 0-values.
    //
    ZeroMem (Context, sizeof (*Context));

    Context->FileBuffer = FileBuffer;
    Context->FileSize   = FileSize;
    //
    // Check whether the DOS Image Header is present.
    //
    if (sizeof (*DosHdr) <= FileSize
        && *(CONST UINT16 *) (CONST VOID *) FileBuffer == EFI_IMAGE_DOS_SIGNATURE) {
        DosHdr = (CONST EFI_IMAGE_DOS_HEADER *) (CONST VOID *) (
                (CONST CHAR8 *) FileBuffer
        );
        //
        // Verify the DOS Image Header and the Executable Header are in bounds of
        // the file buffer, and that they are disjoint.
        //
        if (sizeof (EFI_IMAGE_DOS_HEADER) > DosHdr->e_lfanew
            || DosHdr->e_lfanew > FileSize) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }

        Context->ExeHdrOffset = DosHdr->e_lfanew;
        //
        // Verify the Execution Header offset is sufficiently aligned.
        //
        /*
        if (!PcdGetBool (PcdImageLoaderAllowMisalignedOffset)
            && !IS_ALIGNED (Context->ExeHdrOffset, ALIGNOF (EFI_IMAGE_NT_HEADERS_COMMON_HDR))) {
            return RETURN_UNSUPPORTED;
        }
         */
    }
    //
    // Verify the file buffer can hold a PE Common Header.
    //
    if (FileSize - Context->ExeHdrOffset < sizeof (EFI_IMAGE_NT_HEADERS_COMMON_HDR) + sizeof (UINT16)) {
        Print(L"File buffer cannot hold PE header\n");
        return RETURN_UNSUPPORTED;
    }

    STATIC_ASSERT (
            ALIGNOF (UINT32) <= ALIGNOF (EFI_IMAGE_NT_HEADERS_COMMON_HDR),
            "The following access may be performed unaligned"
    );
    //
    // Verify the Image Executable Header has a PE signature.
    //
    if (*(CONST UINT32 *) (CONST VOID *) ((CONST CHAR8 *) FileBuffer + Context->ExeHdrOffset) != EFI_IMAGE_NT_SIGNATURE) {
        Print(L"No Image Executable Header\n");
        return RETURN_LOAD_ERROR;
    }
    //
    // Verify the PE Image Header is well-formed.
    //
    Status = InternalInitializePe (Context, FileSize);
    if (Status != RETURN_SUCCESS) {
        return Status;
    }

    return RETURN_SUCCESS;
}

RETURN_STATUS
UefiImageInitializeContextPreHash (
        OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        IN  CONST VOID                       *FileBuffer,
        IN  UINT32                           FileSize
)
{
    return PeCoffInitializeContext ((PE_COFF_LOADER_IMAGE_CONTEXT *) Context, FileBuffer, FileSize);
}

RETURN_STATUS
UefiImageInitializeContextPostHash (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);

    return RETURN_SUCCESS;
}

RETURN_STATUS
UefiImageInitializeContext (
        OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        IN  CONST VOID                       *FileBuffer,
        IN  UINT32                           FileSize
)
{
    RETURN_STATUS Status;

    Status = UefiImageInitializeContextPreHash (Context, FileBuffer, FileSize);
    if (RETURN_ERROR (Status)) {
        return Status;
    }

    return UefiImageInitializeContextPostHash (Context);
}

STATIC
VOID
InternalLoadSections (
        IN  CONST PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        IN  UINT32                              LoadedHeaderSize,
        IN  UINT32                              DestinationSize
)
{
    CONST EFI_IMAGE_SECTION_HEADER *Sections;
    UINT16                         SectionIndex;
    UINT32                         EffectivePointerToRawData;
    UINT32                         DataSize;
    UINT32                         PreviousTopRva;

    Sections = (CONST EFI_IMAGE_SECTION_HEADER *) (CONST VOID *) (
            (CONST CHAR8 *) Context->FileBuffer + Context->SectionsOffset
    );
    //
    // As the loop zero's the data from the end of the previous section, start
    // with the size of the loaded Image Headers to zero their trailing data.
    //
    PreviousTopRva = LoadedHeaderSize;

    for (SectionIndex = 0; SectionIndex < Context->NumberOfSections; ++SectionIndex) {
        //
        // Zero from the end of the previous section to the start of this section.
        //
        ZeroMem (
                (CHAR8 *) Context->ImageBuffer + PreviousTopRva,
                Sections[SectionIndex].VirtualAddress - PreviousTopRva
        );
        //
        // Copy the maximum amount of data that fits both sizes.
        //
        if (Sections[SectionIndex].SizeOfRawData <= Sections[SectionIndex].VirtualSize) {
            DataSize = Sections[SectionIndex].SizeOfRawData;
        } else {
            DataSize = Sections[SectionIndex].VirtualSize;
        }
        //
        // Load the current Image section into the memory space.
        //
        EffectivePointerToRawData = Sections[SectionIndex].PointerToRawData;

        CopyMem (
                (CHAR8 *) Context->ImageBuffer + Sections[SectionIndex].VirtualAddress,
                (CHAR8 *) Context->FileBuffer + EffectivePointerToRawData,
                DataSize
        );

        PreviousTopRva = Sections[SectionIndex].VirtualAddress + DataSize;
    }
    //
    // Zero the trailing data after the last Image section.
    //
    ZeroMem (
            (CHAR8 *) Context->ImageBuffer + PreviousTopRva,
            DestinationSize - PreviousTopRva
    );
}

RETURN_STATUS
PeCoffLoadImage (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context,
        OUT    VOID                          *Destination,
        IN     UINT32                        DestinationSize
)
{
    UINT32                         LoadedHeaderSize;
    //CONST EFI_IMAGE_SECTION_HEADER *Sections;

    ASSERT (Context != NULL);
    ASSERT (Destination != NULL);
    ASSERT (ADDRESS_IS_ALIGNED (Destination, Context->SectionAlignment));
    ASSERT (Context->SizeOfImage <= DestinationSize);

    Context->ImageBuffer = Destination;
    //
    // Load the Image Headers into the memory space, if the policy demands it.
    //
    //Sections = (CONST EFI_IMAGE_SECTION_HEADER *) (CONST VOID *) (
    //        (CONST CHAR8 *) Context->FileBuffer + Context->SectionsOffset
    //);

    //if (PcdGetBool (PcdImageLoaderLoadHeader) && Sections[0].VirtualAddress != 0) {
        LoadedHeaderSize = Context->SizeOfHeaders;
        CopyMem (Context->ImageBuffer, (VOID *) Context->FileBuffer, LoadedHeaderSize);
    //} else {
    //    LoadedHeaderSize = 0;
    //}
    //
    // Load all Image sections into the memory space.
    //
    InternalLoadSections (Context, LoadedHeaderSize, DestinationSize);

    return RETURN_SUCCESS;
}

UINTN
PeCoffLoaderGetImageAddress (
        IN CONST PE_COFF_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);
    ASSERT (Context->ImageBuffer != NULL);

    return (UINTN) Context->ImageBuffer;
}

UINT32
PeCoffGetSizeOfImage (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);

    return Context->SizeOfImage;
}

BOOLEAN
BaseOverflowAlignUpU32 (
        UINT32  Value,
        UINT32  Alignment,
        UINT32  *Result
)
{
    BOOLEAN  Status;

    Status   = BaseOverflowAddU32 (Value, Alignment - 1U, Result);
    *Result &= ~(Alignment - 1U);

    return Status;
}

/**
  Reads a 32-bit value from memory that may be unaligned.

  This function returns the 32-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 32-bit value that may be unaligned.

  @return The 32-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned32 (
        IN CONST UINT32  *Buffer
)
{
    ASSERT (Buffer != NULL);

    return *Buffer;
}

/**
  Writes a 32-bit value to memory that may be unaligned.

  This function writes the 32-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 32-bit value that may be unaligned.
  @param  Value   The 32-bit value to write to Buffer.

  @return The 32-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned32 (
        OUT UINT32  *Buffer,
        IN  UINT32  Value
)
{
    ASSERT (Buffer != NULL);

    return *Buffer = Value;
}

/**
  Reads a 64-bit value from memory that may be unaligned.

  This function returns the 64-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 64-bit value that may be unaligned.

  @return The 64-bit value read from Buffer.

**/
UINT64
EFIAPI
ReadUnaligned64 (
        IN CONST UINT64  *Buffer
)
{
    ASSERT (Buffer != NULL);

    return *Buffer;
}

/**
  Writes a 64-bit value to memory that may be unaligned.

  This function writes the 64-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  A pointer to a 64-bit value that may be unaligned.
  @param  Value   The 64-bit value to write to Buffer.

  @return The 64-bit value to write to Buffer.

**/
UINT64
EFIAPI
WriteUnaligned64 (
        OUT UINT64  *Buffer,
        IN  UINT64  Value
)
{
    ASSERT (Buffer != NULL);

    return *Buffer = Value;
}

STATIC
RETURN_STATUS
InternalApplyRelocation (
        IN  CONST PE_COFF_LOADER_IMAGE_CONTEXT     *Context,
        IN  CONST EFI_IMAGE_BASE_RELOCATION_BLOCK  *RelocBlock,
        IN  UINT32                                 RelocIndex,
        IN  UINT64                                 Adjust,
        OUT UINT64                                 *FixupData OPTIONAL
)
{
    BOOLEAN Overflow;

    UINT16  RelocType;
    UINT16  RelocOffset;
    UINT32  RelocTargetRva;
    UINT32  RemRelocTargetSize;

    CHAR8   *Fixup;
    UINT32  Fixup32;
    UINT64  Fixup64;

    RelocType   = IMAGE_RELOC_TYPE (RelocBlock->Relocations[RelocIndex]);
    RelocOffset = IMAGE_RELOC_OFFSET (RelocBlock->Relocations[RelocIndex]);
    //
    // Absolute Base Relocations are used for padding any must be skipped.
    //
    if (RelocType == EFI_IMAGE_REL_BASED_ABSOLUTE) {
        if (FixupData != NULL) {
            FixupData[RelocIndex] = 0;
        }

        return RETURN_SUCCESS;
    }
    //
    // Verify the Base Relocation target address is in bounds of the Image buffer.
    //
    Overflow = BaseOverflowAddU32 (
            RelocBlock->VirtualAddress,
            RelocOffset,
            &RelocTargetRva
    );
    if (Overflow) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }

    Overflow = BaseOverflowSubU32 (
            Context->SizeOfImage,
            RelocTargetRva,
            &RemRelocTargetSize
    );
    if (Overflow) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }

    Fixup = (CHAR8 *) Context->ImageBuffer + RelocTargetRva;
    //
    // Apply the Base Relocation fixup per type.
    // If RuntimeContext is not NULL, store the current value of the fixup
    // target to determine whether it has been changed during runtime
    // execution.
    //
    // It is not clear how EFI_IMAGE_REL_BASED_HIGH and
    // EFI_IMAGE_REL_BASED_LOW are supposed to be handled. While the PE
    // specification suggests to just add the high or low part of the
    // displacement, there are concerns about how it's supposed to deal with
    // wraparounds. As they are virtually non-existent, they are unsupported for
    // the time being.
    //
    switch (RelocType) {
        case EFI_IMAGE_REL_BASED_HIGHLOW:
            //
            // Verify the Base Relocation target is in bounds of the Image buffer.
            //
            if (sizeof (UINT32) > RemRelocTargetSize) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the Image Base Relocation does not target the Image Relocation
            // Directory.
            //
            if (RelocTargetRva + sizeof (UINT32) > Context->RelocDirRva
                && Context->RelocDirRva + Context->RelocDirSize > RelocTargetRva) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Relocate the target instruction.
            //
            Fixup32  = ReadUnaligned32 ((CONST VOID *) Fixup);
            Fixup32 += (UINT32) Adjust;
            WriteUnaligned32 ((VOID *) Fixup, Fixup32);
            //
            // Record the relocated value for Image runtime relocation.
            //
            if (FixupData != NULL) {
                FixupData[RelocIndex] = Fixup32;
            }

            break;

        case EFI_IMAGE_REL_BASED_DIR64:
            //
            // Verify the Image Base Relocation target is in bounds of the Image
            // buffer.
            //
            if (sizeof (UINT64) > RemRelocTargetSize) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the Image Base Relocation does not target the Image Relocation
            // Directory.
            //
            if (RelocTargetRva + sizeof (UINT64) > Context->RelocDirRva
                && Context->RelocDirRva + Context->RelocDirSize > RelocTargetRva) {
                //DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Relocate target the instruction.
            //
            Fixup64  = ReadUnaligned64 ((CONST VOID *) Fixup);
            Fixup64 += Adjust;
            WriteUnaligned64 ((VOID *) Fixup, Fixup64);
            //
            // Record the relocated value for Image runtime relocation.
            //
            if (FixupData != NULL) {
                FixupData[RelocIndex] = Fixup64;
            }

            break;
/*
        case EFI_IMAGE_REL_BASED_ARM_MOV32T:

            //
            // Verify ARM Thumb mode Base Relocations are supported.
            //
            if ((PcdGet32 (PcdImageLoaderRelocTypePolicy) & PCD_RELOC_TYPE_POLICY_ARM) == 0) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the Base Relocation target is in bounds of the Image buffer.
            //
            if (sizeof (UINT64) > RemRelocTargetSize) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the Base Relocation target is sufficiently aligned.
            // The ARM Thumb instruction pair must start on a 16-bit boundary.
            //
            if (!IS_ALIGNED (RelocTargetRva, ALIGNOF (UINT16))) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Verify the Base Relocation does not target the Relocation Directory.
            //
            if (RelocTargetRva + sizeof (UINT64) > Context->RelocDirRva
                && Context->RelocDirRva + Context->RelocDirSize > RelocTargetRva) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
            //
            // Relocate the target instruction.
            //
            PeCoffThumbMovwMovtImmediateFixup (Fixup, Adjust);
            //
            // Record the relocated value for Image runtime relocation.
            //
            if (FixupData != NULL) {
                FixupData[RelocIndex] = ReadUnaligned64 ((CONST VOID *) Fixup);
            }


            break;
*/
        default:
            //
            // The Image Base Relocation type is unknown, disallow the Image.
            //
            //DEBUG_RAISE ();
            Print(L"Relocation failed\n");
            return RETURN_UNSUPPORTED;
    }

    return RETURN_SUCCESS;
}

RETURN_STATUS
PeCoffRelocateImage (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT    *Context,
        IN     UINT64                          BaseAddress,
        OUT    PE_COFF_LOADER_RUNTIME_CONTEXT  *RuntimeContext OPTIONAL,
        IN     UINT32                          RuntimeContextSize
)
{
    RETURN_STATUS                         Status;
    BOOLEAN                               Overflow;

    UINT64                                Adjust;

    UINT32                                RelocBlockOffsetMax;
    UINT32                                TopOfRelocDir;

    UINT32                                RelocBlockOffset;
    CONST EFI_IMAGE_BASE_RELOCATION_BLOCK *RelocBlock;
    UINT32                                RelocBlockSize;
    UINT32                                SizeOfRelocs;
    UINT32                                NumRelocs;
    UINT32                                RelocIndex;
    UINT32                                FixupDataIndex;
    UINT64                                *CurrentFixupData;

    //ASSERT (Context != NULL);
    //ASSERT (!Context->RelocsStripped || BaseAddress == Context->ImageBase);
    //ASSERT (RuntimeContext != NULL || RuntimeContextSize == 0);
    //ASSERT (RuntimeContext == NULL || RuntimeContextSize >= sizeof (PE_COFF_LOADER_RUNTIME_CONTEXT) + Context->RelocDirSize * (sizeof (UINT64) / sizeof (UINT16)));
    //
    // Initialise the Image runtime context header.
    //
    if (RuntimeContext != NULL) {
        RuntimeContext->RelocDirRva  = Context->RelocDirRva;
        RuntimeContext->RelocDirSize = Context->RelocDirSize;
    }
    //
    // Verify the Relocation Directory is not empty.
    //
    if (Context->RelocDirSize == 0) {
        return RETURN_SUCCESS;
    }
    //
    // Calculate the Image displacement from its prefered load address.
    //
    Adjust = BaseAddress - Context->ImageBase;
    //
    // Runtime drivers should unconditionally go through the full Relocation
    // procedure early to eliminate the possibility of errors later at runtime.
    // Runtime drivers don't have their Base Relocations stripped, this is
    // verified during context creation.
    // Skip explicit Relocation when the Image is already loaded at its
    // prefered location.
    //
    if (RuntimeContext == NULL && Adjust == 0) {
        return RETURN_SUCCESS;
    }

    RelocBlockOffset    = Context->RelocDirRva;
    TopOfRelocDir       = Context->RelocDirRva + Context->RelocDirSize;
    RelocBlockOffsetMax = TopOfRelocDir - sizeof (EFI_IMAGE_BASE_RELOCATION_BLOCK);
    FixupDataIndex      = 0;
    //
    // Align TopOfRelocDir because, if the policy does not demand Relocation Block
    // sizes to be aligned, the code below will manually align them. Thus, the
    // end offset of the last Relocation Block must be compared to a manually
    // aligned Relocation Directoriy end offset.
    //

    //if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_RELOCATION_BLOCK_SIZES) != 0) {
        Overflow = BaseOverflowAlignUpU32 (
                TopOfRelocDir,
                ALIGNOF (EFI_IMAGE_BASE_RELOCATION_BLOCK),
                &TopOfRelocDir
        );
        if (Overflow) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
    //}
    //
    // Apply all Base Relocations of the Image.
    //
    while (RelocBlockOffset <= RelocBlockOffsetMax) {
        RelocBlock = (CONST EFI_IMAGE_BASE_RELOCATION_BLOCK *) (CONST VOID *) (
                (CONST CHAR8 *) Context->ImageBuffer + RelocBlockOffset
        );
        //
        // Verify the Base Relocation Block size is well-formed.
        //
        Overflow = BaseOverflowSubU32 (
                RelocBlock->SizeOfBlock,
                sizeof (EFI_IMAGE_BASE_RELOCATION_BLOCK),
                &SizeOfRelocs
        );
        if (Overflow) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Verify the Base Relocation Block is in bounds of the Relocation
        // Directory.
        //
        if (SizeOfRelocs > RelocBlockOffsetMax - RelocBlockOffset) {
            //DEBUG_RAISE ();
            return RETURN_VOLUME_CORRUPTED;
        }
        //
        // Advance to the next Base Relocation Block offset based on the alignment
        // policy.
        //
        /*
        if ((PcdGet32 (PcdImageLoaderAlignmentPolicy) & PCD_ALIGNMENT_POLICY_RELOCATION_BLOCK_SIZES) == 0) {
            RelocBlockSize = RelocBlock->SizeOfBlock;
            //
            // Verify the next Base Relocation Block offset is sufficiently aligned.
            //
            if (!IS_ALIGNED (RelocBlockSize, ALIGNOF (EFI_IMAGE_BASE_RELOCATION_BLOCK))) {
                DEBUG_RAISE ();
                return RETURN_VOLUME_CORRUPTED;
            }
        } else {
         */
            //
            // This arithmetic cannot overflow because we know
            //   1) RelocBlock->SizeOfBlock <= RelocMax <= TopOfRelocDir
            //   2) IS_ALIGNED (TopOfRelocDir, ALIGNOF (EFI_IMAGE_BASE_RELOCATION_BLOCK)).
            //
            RelocBlockSize = ALIGN_VALUE (
                    RelocBlock->SizeOfBlock,
                    ALIGNOF (EFI_IMAGE_BASE_RELOCATION_BLOCK)
            );

        //
        // This division is safe due to the guarantee made above.
        //
        NumRelocs = SizeOfRelocs / sizeof (*RelocBlock->Relocations);

        if (RuntimeContext != NULL) {
            CurrentFixupData = &RuntimeContext->FixupData[FixupDataIndex];
            //
            // This arithmetic cannot overflow because The number of Image Base
            // Relocations cannot exceed the size of their Image Relocation Block, and
            // latter has been verified to be in bounds of the Image buffer. The Image
            // buffer size and RelocDataIndex are both bound by MAX_UINT32.
            //
            FixupDataIndex += NumRelocs;
        } else {
            CurrentFixupData = NULL;
        }
        //
        // Process all Base Relocations of the current Block.
        //
        for (RelocIndex = 0; RelocIndex < NumRelocs; ++RelocIndex) {
            //
            // Apply the Image Base Relocation fixup.
            // If RuntimeContext is not NULL, store the current value of the fixup
            // target to determine whether it has been changed during runtime
            // execution.
            //
            // It is not clear how EFI_IMAGE_REL_BASED_HIGH and
            // EFI_IMAGE_REL_BASED_LOW are supposed to be handled. While PE reference
            // suggests to just add the high or low part of the displacement, there
            // are concerns about how it's supposed to deal with wraparounds.
            // As no known linker emits them, omit support.
            //
            Status = InternalApplyRelocation (
                    Context,
                    RelocBlock,
                    RelocIndex,
                    Adjust,
                    CurrentFixupData
            );
            if (Status != RETURN_SUCCESS) {
                //DEBUG_RAISE ();
                return Status;
            }
        }
        //
        // This arithmetic cannot overflow because it has been checked that the
        // Image Base Relocation Block is in bounds of the Image buffer.
        //
        RelocBlockOffset += RelocBlockSize;
    }
    //
    // Verify the Relocation Directory size matches the contained data.
    //
    if (RelocBlockOffset != TopOfRelocDir) {
        //DEBUG_RAISE ();
        return RETURN_VOLUME_CORRUPTED;
    }
    //
    // Initialise the remaining uninitialised portion of the Image runtime
    // context.
    //
    if (RuntimeContext != NULL) {
        //
        // This arithmetic cannot overflow due to the guarantee given by
        // PeCoffLoaderGetRuntimeContextSize().
        //
        ZeroMem (
                &RuntimeContext->FixupData[FixupDataIndex],
                RuntimeContextSize - sizeof (PE_COFF_LOADER_RUNTIME_CONTEXT) - FixupDataIndex * sizeof (UINT64)
        );
    }

    return RETURN_SUCCESS;
}

UINT32
PeCoffGetSectionAlignment (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);

    return Context->SectionAlignment;
}

UINT32
PeCoffGetAddressOfEntryPoint (
        IN CONST PE_COFF_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);

    return Context->AddressOfEntryPoint;
}

RETURN_STATUS
UefiImageLoadImage (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        OUT    VOID                             *Destination,
        IN     UINT32                           DestinationSize
)
{
    return PeCoffLoadImage ((PE_COFF_LOADER_IMAGE_CONTEXT *) Context, Destination, DestinationSize);
}

UINTN
UefiImageLoaderGetImageAddress (
        IN CONST UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    return PeCoffLoaderGetImageAddress ((const PE_COFF_LOADER_IMAGE_CONTEXT *) Context);
}

UINT32
UefiImageGetImageSize (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    return PeCoffGetSizeOfImage ((PE_COFF_LOADER_IMAGE_CONTEXT *) Context);
}

RETURN_STATUS
UefiImageGetSymbolsPath (
        IN  CONST UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context,
        OUT CONST CHAR8                            **SymbolsPath,
        OUT UINT32                                 *SymbolsPathSize
)
{
    // Unreferenced arguments
    (VOID)Context; (VOID)SymbolsPath; (VOID)SymbolsPathSize;
    return EFI_NOT_FOUND;
}

VOID *
EFIAPI
InvalidateInstructionCacheRange (
        IN      VOID   *Address,
        IN      UINTN  Length
)
{
    if (Length == 0) {
        return Address;
    }

    ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINTN)Address));
    return Address;
}

RETURN_STATUS
UefiImageLoadImageForExecution (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT    *Context,
        OUT    VOID                               *Destination,
        IN     UINT32                             DestinationSize,
        OUT    UEFI_IMAGE_LOADER_RUNTIME_CONTEXT  *RuntimeContext OPTIONAL,
        IN     UINT32                             RuntimeContextSize
)
{
    RETURN_STATUS Status;
    UINTN         BaseAddress;
    UINTN         SizeOfImage;
    //
    // Load the Image into the memory space.
    //
    Status = UefiImageLoadImage (Context, Destination, DestinationSize);
    if (RETURN_ERROR (Status)) {
        return Status;
    }
    //
    // Relocate the Image to the address it has been loaded to.
    //
    BaseAddress = UefiImageLoaderGetImageAddress (Context);
    Status = UefiImageRelocateImage (
            Context,
            BaseAddress,
            RuntimeContext,
            RuntimeContextSize
    );
    if (RETURN_ERROR (Status)) {
        return Status;
    }

    SizeOfImage = UefiImageGetImageSize (Context);
    //
    // Flush the instruction cache so the image data is written before execution.
    //
    InvalidateInstructionCacheRange ((VOID *) BaseAddress, SizeOfImage);

    return RETURN_SUCCESS;
}

CONST CHAR8 *
DeCygwinPathIfNeeded (
        IN  CONST CHAR8   *Name,
        IN  CHAR8         *Temp,
        IN  UINTN         Size
)
{
    CHAR8  *Ptr;
    UINTN  Index;
    UINTN  Index2;

    Ptr = strstr (Name, "/cygdrive/");
    if (Ptr == NULL) {
        return Name;
    }

    for (Index = 9, Index2 = 0; (Index < (Size + 9)) && (Ptr[Index] != '\0'); Index++, Index2++) {
        Temp[Index2] = Ptr[Index];
        if (Temp[Index2] == '/') {
            Temp[Index2] = '\\';
        }

        if (Index2 == 1) {
            Temp[Index2 - 1] = Ptr[Index];
            Temp[Index2]     = ':';
        }
    }

    return Temp;
}

UINT32
UefiImageGetEntryPointAddress (
        IN CONST UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    return PeCoffGetAddressOfEntryPoint ((const PE_COFF_LOADER_IMAGE_CONTEXT *) Context);
}

UINTN
UefiImageLoaderGetImageEntryPoint (
        IN CONST UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    UINTN  ImageAddress;
    UINT32 EntryPointAddress;

    ASSERT (Context != NULL);

    ImageAddress      = UefiImageLoaderGetImageAddress (Context);
    EntryPointAddress = UefiImageGetEntryPointAddress (Context);

    return ImageAddress + EntryPointAddress;
}

UINT32
UefiImageGetSegmentAlignment (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    return PeCoffGetSectionAlignment ((PE_COFF_LOADER_IMAGE_CONTEXT *) Context);
}

#define FUNCTION_ENTRY_POINT(FunctionPointer)  (VOID *)(UINTN)(FunctionPointer)

VOID
EFIAPI
UefiImageLoaderRelocateImageExtraAction (
        IN CONST UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *ImageContext
)
{
    RETURN_STATUS Status;
    CONST CHAR8   *PdbPath;
    UINT32        PdbPathSize;
#if defined (__CC_ARM) || defined (__GNUC__)
    CHAR8         Temp[512];
#endif

    Status = UefiImageGetSymbolsPath (ImageContext, &PdbPath, &PdbPathSize);

    if (!RETURN_ERROR (Status)) {
#ifdef __CC_ARM
        #if (__ARMCC_VERSION < 500000)
    // Print out the command for the RVD debugger to load symbols for this image
    DEBUG ((DEBUG_LOAD | DEBUG_INFO, "load /a /ni /np %a &0x%p\n", DeCygwinPathIfNeeded (PdbPath, Temp, sizeof (Temp)), UefiImageLoaderGetImageAddress (ImageContext)));
 #else
    // Print out the command for the DS-5 to load symbols for this image
    DEBUG ((DEBUG_LOAD | DEBUG_INFO, "add-symbol-file %a -o 0x%p\n", DeCygwinPathIfNeeded (PdbPath, Temp, sizeof (Temp)), UefiImageLoaderGetImageAddress (ImageContext)));
 #endif
#elif __GNUC__
        // This may not work correctly if you generate PE/COFF directly as then the Offset would not be required
        DEBUG ((D_LOAD | DEBUG_INFO, "add-symbol-file %a -o 0x%p\n", DeCygwinPathIfNeeded (PdbPath, Temp, sizeof (Temp)), UefiImageLoaderGetImageAddress (ImageContext)));
#else
        DEBUG ((DEBUG_LOAD | DEBUG_INFO, "Loading driver at 0x%11p EntryPoint=0x%11p\n", (VOID *)(UINTN)UefiImageLoaderGetImageAddress (ImageContext), FUNCTION_ENTRY_POINT (UefiImageLoaderGetImageEntryPoint (ImageContext))));
#endif
    } else {
        DEBUG ((D_LOAD | DEBUG_INFO, "Loading driver at 0x%11p EntryPoint=0x%11p\n", (VOID *)(UINTN)UefiImageLoaderGetImageAddress (ImageContext), FUNCTION_ENTRY_POINT (UefiImageLoaderGetImageEntryPoint (ImageContext))));
    }
}

RETURN_STATUS
UefiImageRelocateImage (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT    *Context,
        IN     UINT64                             BaseAddress,
        OUT    UEFI_IMAGE_LOADER_RUNTIME_CONTEXT  *RuntimeContext OPTIONAL,
        IN     UINT32                             RuntimeContextSize
)
{
    RETURN_STATUS Status;

    Status = PeCoffRelocateImage (
            (PE_COFF_LOADER_IMAGE_CONTEXT *) Context,
            BaseAddress,
            (PE_COFF_LOADER_RUNTIME_CONTEXT *) RuntimeContext,
            RuntimeContextSize
    );
    if (!RETURN_ERROR (Status)) {
        UefiImageLoaderRelocateImageExtraAction (Context);
    }

    return Status;
}

UINT16
PeCoffGetSubsystem (
        IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *Context
)
{
    ASSERT (Context != NULL);

    return Context->Subsystem;
}

UINT16
UefiImageGetSubsystem (
        IN OUT UEFI_IMAGE_LOADER_IMAGE_CONTEXT  *Context
)
{
    return PeCoffGetSubsystem((PE_COFF_LOADER_IMAGE_CONTEXT *) Context);
}
