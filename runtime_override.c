/* EFI Runtime Overrides */

#include <atvlib.h>
#include <Library/PeCoffLib2.h>
#include <Library/UefiImageLib.h>

///
/// The IA-32 architecture context buffer used by SetJump() and LongJump().
///
typedef struct {
    UINT32    Ebx;
    UINT32    Esi;
    UINT32    Edi;
    UINT32    Ebp;
    UINT32    Esp;
    UINT32    Eip;
    UINT32    Ssp;
} BASE_LIBRARY_JUMP_BUFFER;

STATIC EFI_GUID  mOcLoadedImageProtocolGuid = {
        0x1f3c963d, 0xf9dc, 0x4537, { 0xbb, 0x06, 0xd8, 0x08, 0x46, 0x4a, 0x85, 0x2e }
};

STATIC EFI_GUID  mOcImageLoaderCapsProtocolGuid = {
        0xf5bbca36, 0x0f99, 0x4e7b, { 0x86, 0x0f, 0x92, 0x06, 0xa9, 0x3b, 0x52, 0xd0 }
};

typedef struct {
    EFI_IMAGE_ENTRY_POINT        EntryPoint;
    EFI_PHYSICAL_ADDRESS         ImageArea;
    UINTN                        PageCount;
    EFI_STATUS                   Status;
    VOID                         *JumpBuffer;
    BASE_LIBRARY_JUMP_BUFFER     *JumpContext;
    CHAR16                       *ExitData;
    UINTN                        ExitDataSize;
    UINT16                       Subsystem;
    BOOLEAN                      Started;
    EFI_LOADED_IMAGE_PROTOCOL    LoadedImage;
} OC_LOADED_IMAGE_PROTOCOL;

EFI_STATUS
EFIAPI
AllocateAlignedPagesEx (
        IN     EFI_ALLOCATE_TYPE     Type,
        IN     EFI_MEMORY_TYPE       MemoryType,
        IN     UINTN                 Pages,
        IN     UINTN                 Alignment,
        IN OUT EFI_PHYSICAL_ADDRESS  *Memory
);

#define FreeAlignedPages FreePages;

/*
STATIC
EFI_HANDLE
ImageHandleToDeviceHandle(IN EFI_HANDLE ImageHandle)
{
    EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage = NULL;
    EFI_STATUS                  Status = EFI_SUCCESS;

    Status = gBS->HandleProtocol(ImageHandle,
                                 &gEfiLoadedImageProtocolGuid,
                                 (VOID **) &LoadedImage);

    if (Status != EFI_SUCCESS)
    {
        return NULL;
    }

    return LoadedImage->DeviceHandle;
}
*/

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
BaseOverflowSubU32 (
        UINT32  A,
        UINT32  B,
        UINT32  *Result
)
{
#if defined (BASE_HAS_TYPE_GENERIC_BUILTINS)
    return __builtin_sub_overflow (A, B, Result);
#elif defined (BASE_HAS_TYPE_SPECIFIC_BUILTINS)
    return __builtin_usub_overflow (A, B, Result);
#else
    *Result = A - B;
    if (B <= A) {
        return FALSE;
    }

    return TRUE;
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
        if (CompareGuid (InformationType, &gEfiFileInfoGuid) && BaseOverflowAddU32 (FileInfoSize, sizeof (CHAR16), &FileInfoSize)) {
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

STATIC
EFI_STATUS
OcGetFileSize(IN EFI_FILE_PROTOCOL   *File,
            OUT UINT32              *Size)
{
    EFI_STATUS      Status = EFI_SUCCESS;
    UINT64          Position = 0;
    EFI_FILE_INFO   *FileInfo;

    Status = File->SetPosition(File, 0xFFFFFFFFFFFFFFFFULL);
    if (EFI_ERROR(Status))
    {
        //
        // Some drivers, like EfiFs, return EFI_UNSUPPORTED when trying to seek
        // past the file size. Use slow method via attributes for them.
        //
        FileInfo = OcGetFileInfo (File, &gEfiFileInfoGuid, sizeof (*FileInfo), NULL);
        Print(L"FileInfo->FileSize: %d\n", FileInfo->FileSize);
        if (FileInfo != NULL) {
            if ((UINT32)FileInfo->FileSize == FileInfo->FileSize) {
                *Size  = (UINT32)FileInfo->FileSize;
                Status = EFI_SUCCESS;
            }

            FreePool (FileInfo);
        }
        return Status;
    }

    Status = File->GetPosition (File, &Position);
    File->SetPosition (File, 0);
    if (EFI_ERROR (Status)) {
        return Status;
    }

    Print(L"Position: %d\n", Position);

    if ((UINT32)Position != Position) {
        return EFI_OUT_OF_RESOURCES;
    }

    *Size = (UINT32)Position;

    return EFI_SUCCESS;
}

EFI_STATUS
OcGetFileData (
        IN  EFI_FILE_PROTOCOL  *File,
        IN  UINT32             Position,
        IN  UINT32             Size,
        OUT UINT8              *Buffer
)
{
    EFI_STATUS  Status;
    UINTN       ReadSize;
    UINTN       RequestedSize;

    while (Size > 0) {
        Status = File->SetPosition (File, Position);
        if (EFI_ERROR (Status)) {
            return Status;
        }

        //
        // We are required to read in 1 MB portions, because otherwise some
        // systems namely MacBook7,1 will not read file data from APFS volumes
        // but will pretend they did. Reproduced with BootKernelExtensions.kc.
        //
        ReadSize = RequestedSize = MIN (Size, BASE_1MB);
        Status   = File->Read (File, &ReadSize, Buffer);
        if (EFI_ERROR (Status)) {
            File->SetPosition (File, 0);
            return Status;
        }

        if (ReadSize != RequestedSize) {
            File->SetPosition (File, 0);
            return EFI_BAD_BUFFER_SIZE;
        }

        Position += (UINT32)ReadSize;
        Buffer   += ReadSize;
        Size     -= (UINT32)ReadSize;
    }

    File->SetPosition (File, 0);

    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
LoadImageFromDevicePath(IN EFI_DEVICE_PATH    **FullFilePath,
                        IN OUT VOID           **Buffer,
                        OUT UINTN             *Size)
{
    EFI_STATUS                          Status;
    EFI_HANDLE                          FSHandle;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *FileSystem;
    EFI_FILE_HANDLE                     File;
    UINT32                              FileSize;
    EFI_FILE_HANDLE                     LastFile;
    EFI_FILE_HANDLE                     NextFile;
    EFI_DEVICE_PATH                     *FilePath;
    CHAR16                              *AlignedPathName;
    CHAR16                              *PathName;
    UINTN                               PathLength;
    VOID                                *FileBuffer = NULL;

    *Size = 0;

    // Find the Simple File System Protocol
    Status = gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid,
                                   FullFilePath,
                                   &FSHandle);
    if (Status != EFI_SUCCESS)
    {
        DEBUG((D_ERROR, "Cannot locate Simple File System Protocol within device path! (%r)\n", Status));
        return Status;
    }

    // Open the Simple File System Protocol
    Status = gBS->OpenProtocol(FSHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID **) &FileSystem,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (Status != EFI_SUCCESS)
    {
        DEBUG((D_ERROR, "Cannot open Simple File System Protocol! (%r)\n", Status));
        return Status;
    }

    // Open the root file system
    Status = FileSystem->OpenVolume(FileSystem, &LastFile);
    if (Status != EFI_SUCCESS)
    {
        DEBUG((D_ERROR, "Cannot open filesystem! (%r)\n", Status));
        return Status;
    }

    // Get to the end of the device path (the file name)
    FilePath = *FullFilePath;
    while (!IsDevicePathEnd(FilePath))
    {
        if ((DevicePathType(FilePath) != MEDIA_DEVICE_PATH) ||
            (DevicePathSubType(FilePath) != MEDIA_FILEPATH_DP))
        {
            Status = EFI_INVALID_PARAMETER;
            goto CloseLastFile;
        }

        // Align FilePathNode->PathName
        // yeah i'm just gonna copy this from opencore lol

        AlignedPathName = AllocateCopyPool(DevicePathNodeLength(FilePath) - SIZE_OF_FILEPATH_DEVICE_PATH,
                         ((FILEPATH_DEVICE_PATH *) FilePath)->PathName);
        if (AlignedPathName == NULL)
        {
            Status = EFI_OUT_OF_RESOURCES;
            goto CloseLastFile;
        }

        //
        // This is a compatibility hack for firmware types that do not support
        // opening filepaths (directories) with a trailing slash.
        // More details in a852f85986c1fe23fc3a429605e3c560ea800c54 OpenCorePkg commit.
        //
        PathLength = StrLen(AlignedPathName);
        if ((PathLength > 0) && (AlignedPathName[PathLength - 1] == '\\')) {
            AlignedPathName[PathLength - 1] = '\0';
        }

        PathName = AlignedPathName;

        Status = LastFile->Open(LastFile,
                                &NextFile,
                                PathName,
                                EFI_FILE_MODE_READ,
                                0);
        if (Status != EFI_SUCCESS)
        {
            goto CloseLastFile;
        }

        LastFile->Close(LastFile);
        LastFile = NextFile;
        FilePath = NextDevicePathNode(FilePath);
    }

    // We found the file
    File = LastFile;
    Status = OcGetFileSize(File, &FileSize);
    if (Status != EFI_SUCCESS)
    {
        File->Close(File);
        return EFI_UNSUPPORTED;
    }

    FileBuffer = AllocatePool(FileSize);
    if (FileBuffer == NULL)
    {
        File->Close(File);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = OcGetFileData(File, 0, FileSize, FileBuffer);
    if (Status != EFI_SUCCESS)
    {
        DEBUG((D_ERROR, "Reading file data failed! (%r)\n", Status));
        FreePool(FileBuffer);
        File->Close(File);
        return EFI_DEVICE_ERROR;
    }

    *Buffer = FileBuffer;
    Print(L"FileSize = %d\n", FileSize);
    *Size = FileSize;
    return EFI_SUCCESS;


CloseLastFile:
    LastFile->Close(LastFile);
    DEBUG((D_ERROR, "File system closed prematurely. Status = %r\n", Status));
    return Status;
}

#define EFI_LOADED_IMAGE_PROTOCOL_REVISION  0x1000

///
/// Revision defined in EFI1.1.
///
#define EFI_LOADED_IMAGE_INFORMATION_REVISION  EFI_LOADED_IMAGE_PROTOCOL_REVISION

EFI_STATUS
EFIAPI
UnsignedLoadImage(IN BOOLEAN            BootPolicy,
                  IN EFI_HANDLE         ParentImageHandle,
                  IN EFI_DEVICE_PATH    *FilePath,
                  IN VOID               *SourceBuffer   OPTIONAL,
                  IN UINTN              SourceSize,
                  OUT EFI_HANDLE        *ImageHandle) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_STATUS ImageStatus;
    VOID *ImageBuffer = NULL;
    UINTN ImageSize;
    UINT32 DestinationSize;
    UINT32 DestinationPages;
    UINT32 DestinationAlignment;
    UEFI_IMAGE_LOADER_IMAGE_CONTEXT ImageContext;
    EFI_PHYSICAL_ADDRESS DestinationArea;
    VOID *DestinationBuffer;
    OC_LOADED_IMAGE_PROTOCOL *OcLoadedImage;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    DEBUG((D_ERROR, "UnsignedLoadImage starting up\n"));

    if ((ParentImageHandle == NULL) || (ImageHandle == NULL)) {
        // FIXME: Also return EFI_INVALID_PARAMETER for incorrect image handle.
        return EFI_INVALID_PARAMETER;
    }

    if ((SourceBuffer == NULL) && (FilePath == NULL)) {
        return EFI_NOT_FOUND;
    }

    if ((SourceBuffer != NULL) && (SourceSize == 0)) {
        return EFI_UNSUPPORTED;
    }

    if (SourceBuffer == NULL) // Loading from file path.
    {
        LoadImageFromDevicePath(&FilePath,
                                (VOID **) ImageBuffer,
                                &ImageSize);
    } else {
        ImageBuffer = SourceBuffer;
        ImageSize = SourceSize;
    }

    if (ImageSize > MAX_UINT32) {
        return EFI_UNSUPPORTED;
    }

    // Initialize image context

    ImageStatus = UefiImageInitializeContext(&ImageContext,
                                             SourceBuffer,
                                             (UINT32) ImageSize);
    if (ImageStatus != EFI_SUCCESS) {
        DEBUG((D_ERROR, "Initializing EFI image failed: %r\n", ImageStatus));
    }

    // Check the architecture
    if (ImageContext.Ctx.Pe.Machine != IMAGE_FILE_MACHINE_I386) {
        DEBUG ((D_ERROR, "OCB: PeCoff wrong machine - %x\n", ImageContext.Ctx.Pe.Machine));
        return EFI_UNSUPPORTED;
    }

    //
    // Reject RT drivers for the moment.
    //
    if (ImageContext.Ctx.Pe.Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER) {
        DEBUG ((DEBUG_INFO, "OCB: PeCoff no support for RT drivers\n"));
        return EFI_UNSUPPORTED;
    }

    ImageSize = ImageContext.Ctx.Pe.SizeOfImage;
    DestinationPages = EFI_SIZE_TO_PAGES(ImageSize);
    DestinationSize = EFI_PAGES_TO_SIZE(DestinationPages);
    DestinationAlignment = ImageContext.Ctx.Pe.SectionAlignment;

    if (DestinationSize >= BASE_16MB) {
        DEBUG ((D_ERROR, "OCB: PeCoff prohibits files over 16M (%u)\n", DestinationSize));
        return RETURN_UNSUPPORTED;
    }

    //
    // Allocate the image destination memory.
    // FIXME: RT drivers require EfiRuntimeServicesCode.
    //
    Status = AllocateAlignedPagesEx(
            AllocateAnyPages,
            ImageContext.Ctx.Pe.Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION
            ? EfiLoaderCode : EfiBootServicesCode,
            DestinationPages,
            DestinationAlignment,
            &DestinationArea
    );

    if (EFI_ERROR (Status)) {
        DEBUG ((D_ERROR, "OCB: PeCoff could allocate image buffer\n"));
        return Status;
    }

    DestinationBuffer = (VOID *) (UINTN) DestinationArea;

    ImageStatus = UefiImageLoadImageForExecution(
            &ImageContext,
            DestinationBuffer,
            DestinationSize,
            NULL,
            0
    );

    if (EFI_ERROR (ImageStatus)) {
        DEBUG ((D_ERROR, "OCB: PeCoff load image for execution error - %r\n", ImageStatus));
        gBS->FreePages((EFI_PHYSICAL_ADDRESS) DestinationBuffer, DestinationPages);
        return EFI_UNSUPPORTED;
    }

    //
    // Construct a LoadedImage protocol for the image.
    //
    OcLoadedImage = AllocateZeroPool(sizeof(*OcLoadedImage));
    if (OcLoadedImage == NULL) {
        gBS->FreePages((EFI_PHYSICAL_ADDRESS) DestinationBuffer, DestinationPages);
        return EFI_OUT_OF_RESOURCES;
    }

    OcLoadedImage->EntryPoint = (EFI_IMAGE_ENTRY_POINT) ((UINTN) DestinationBuffer +
                                                         ImageContext.Ctx.Pe.AddressOfEntryPoint);
    OcLoadedImage->ImageArea = DestinationArea;
    OcLoadedImage->PageCount = DestinationPages;
    OcLoadedImage->Subsystem = ImageContext.Ctx.Pe.Subsystem;

    LoadedImage = &OcLoadedImage->LoadedImage;

    LoadedImage->Revision = EFI_LOADED_IMAGE_INFORMATION_REVISION;
    LoadedImage->ParentHandle = ParentImageHandle;
    LoadedImage->SystemTable = gST;
    LoadedImage->ImageBase = DestinationBuffer;
    LoadedImage->ImageSize = DestinationSize;
    //
    // FIXME: Support RT drivers.
    //
    if (ImageContext.Ctx.Pe.Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION) {
        LoadedImage->ImageCodeType = EfiLoaderCode;
        LoadedImage->ImageDataType = EfiLoaderData;
    } else {
        LoadedImage->ImageCodeType = EfiBootServicesCode;
        LoadedImage->ImageDataType = EfiBootServicesData;
    }

    //
    // Install LoadedImage and the image's entry point.
    //
    *ImageHandle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces(
            ImageHandle,
            &gEfiLoadedImageProtocolGuid,
            LoadedImage,
            &mOcLoadedImageProtocolGuid,
            OcLoadedImage,
            NULL
    );
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "OCB: PeCoff proto install error - %r\n", Status));
        FreePool(OcLoadedImage);
        gBS->FreePages((EFI_PHYSICAL_ADDRESS) DestinationBuffer, DestinationPages);
        return Status;


    }

    gBS->FreePages ((EFI_PHYSICAL_ADDRESS) (VOID *) (UINTN) OcLoadedImage->ImageArea, OcLoadedImage->PageCount);
    FreePool (OcLoadedImage);
    //
    // NOTE: Avoid EFI 1.10 extension of closing opened protocols.
    //
    return EFI_SUCCESS;
}
EFI_STATUS
EFIAPI
UnsignedStartImage(IN EFI_HANDLE        ImageHandle,
                   OUT UINTN            *ExitDataSize,
                   OUT CHAR16           **ExitData)

{
    return EFI_UNSUPPORTED;
}

