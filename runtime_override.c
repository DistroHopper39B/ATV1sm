/* EFI Runtime Overrides */

#include <atv1sm.h>
#include <Library/PeCoffLib2.h>
#include <Library/UefiImageLib.h>

EFI_IMAGE_LOAD      OriginalLoadImage = NULL;
EFI_IMAGE_START     OriginalStartImage = NULL;
EFI_IMAGE_UNLOAD    OriginalUnloadImage = NULL;
EFI_EXIT            OriginalExit = NULL;

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
    UINT32    ImageCaps;
} OC_IMAGE_LOADER_CAPS_PROTOCOL;

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

VOID
EFIAPI
FreeAlignedPages (
    IN VOID     *Buffer,
    IN UINTN    Pages
    );

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

        // FIXME: THIS BREAKS WHEN OPTIMIZATION LEVEL != 0. WHY?? I HAVE NO CLUE.
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

    if (FileSize == 0)
    {
        // File doesn't exist.
        return EFI_INVALID_PARAMETER;
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
    EFI_DEVICE_PATH *pFilePath = FilePath;

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
        Status = LoadImageFromDevicePath(&FilePath,
                                (VOID **) &ImageBuffer,
                                &ImageSize);
        if (Status != EFI_SUCCESS)
            return Status;
    } else {
        ImageBuffer = SourceBuffer;
        ImageSize = SourceSize;
    }

    if (ImageSize > MAX_UINT32) {
        return EFI_UNSUPPORTED;
    }

    // Initialize image context

    ImageStatus = UefiImageInitializeContext(&ImageContext,
                                             ImageBuffer,
                                             (UINT32) ImageSize);
    if (ImageStatus != EFI_SUCCESS)
    {
        if (ImageStatus == EFI_LOAD_ERROR)
        // Not a PE file, this can happen with a fat binary. We can use the original LoadImage for those.
        {
            Print(L"Trying stock LoadImage.");
            return OriginalLoadImage(BootPolicy,
                                     ParentImageHandle,
                                     pFilePath,
                                     SourceBuffer,
                                     SourceSize,
                                     ImageHandle);
        }
        return ImageStatus;

    }

    // Check the architecture
    /*
    if (ImageContext.Ctx.Pe.Machine != IMAGE_FILE_MACHINE_I386) {
        Print(L"OCB: PeCoff wrong machine - %x\n", ImageContext.Ctx.Pe.Machine);
        return EFI_UNSUPPORTED;
    }
     */

    //
    // Reject RT drivers for the moment.
    //
    if (UefiImageGetSubsystem(&ImageContext) == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER) {
        Print(L"OCB: PeCoff no support for RT drivers\n");
        return EFI_UNSUPPORTED;
    }

    ImageSize = UefiImageGetImageSize(&ImageContext);
    DestinationPages = EFI_SIZE_TO_PAGES(ImageSize);
    DestinationSize = EFI_PAGES_TO_SIZE(DestinationPages);
    DestinationAlignment = UefiImageGetSegmentAlignment(&ImageContext);

    if (DestinationSize >= BASE_16MB) {
        Print(L"OCB: PeCoff prohibits files over 16M (%u)\n", DestinationSize);
        return RETURN_UNSUPPORTED;
    }

    //
    // Allocate the image destination memory.
    // FIXME: RT drivers require EfiRuntimeServicesCode.
    //
    Status = AllocateAlignedPagesEx(
            AllocateAnyPages,
            UefiImageGetSubsystem(&ImageContext) == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION
            ? EfiLoaderCode : EfiBootServicesCode,
            DestinationPages,
            DestinationAlignment,
            &DestinationArea
    );

    if (EFI_ERROR (Status)) {
        Print(L"OCB: PeCoff could allocate image buffer\n");
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
        Print(L"OCB: PeCoff load image for execution error - %r\n", ImageStatus);
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
                                UefiImageGetEntryPointAddress(&ImageContext));
    OcLoadedImage->ImageArea = DestinationArea;
    OcLoadedImage->PageCount = DestinationPages;
    OcLoadedImage->Subsystem = UefiImageGetSubsystem(&ImageContext);

    LoadedImage = &OcLoadedImage->LoadedImage;

    LoadedImage->Revision = EFI_LOADED_IMAGE_INFORMATION_REVISION;
    LoadedImage->ParentHandle = ParentImageHandle;
    LoadedImage->SystemTable = gST;
    LoadedImage->ImageBase = DestinationBuffer;
    LoadedImage->ImageSize = DestinationSize;
    //
    // FIXME: Support RT drivers.
    //
    if (UefiImageGetSubsystem(&ImageContext) == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION) {
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

    Status = gBS->LocateDevicePath (&gEfiSimpleFileSystemProtocolGuid, &pFilePath, &LoadedImage->DeviceHandle);
    if (EFI_ERROR (Status)) {
        Print(L"ATV1sm [OCB]: %r encountered when trying to locate device path. This is expected behavior with Linux/GRUB2 and probably isn't fatal.\n", Status);
        //
        // TODO: Handle load protocol if necessary.
        //
        //return Status;
    }

    LoadedImage->FilePath     = DuplicateDevicePath (pFilePath);

    Status = gBS->InstallMultipleProtocolInterfaces(
            ImageHandle,
            &gEfiLoadedImageProtocolGuid,
            LoadedImage,
            &mOcLoadedImageProtocolGuid,
            OcLoadedImage,
            NULL
    );
    if (EFI_ERROR (Status)) {
        Print(L"OCB: PeCoff proto install error - %r\n", Status);
        FreePool(OcLoadedImage);
        gBS->FreePages((EFI_PHYSICAL_ADDRESS) DestinationBuffer, DestinationPages);
        return Status;


    }

    return EFI_SUCCESS;
}

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT  4

VOID
EFIAPI
InternalAssertJumpBuffer (
        IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
)
{
    ASSERT (JumpBuffer != NULL);

    ASSERT (((UINTN)JumpBuffer & (BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT - 1)) == 0);
}

RETURNS_TWICE
UINTN
EFIAPI
SetJump (
        OUT      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
)
{
    InternalAssertJumpBuffer (JumpBuffer);
    return 0;
}

STATIC
EFI_STATUS
InternalDirectStartImage (
        IN  OC_LOADED_IMAGE_PROTOCOL  *OcLoadedImage,
        IN  EFI_HANDLE                ImageHandle,
        OUT UINTN                     *ExitDataSize,
        OUT CHAR16                    **ExitData OPTIONAL
)
{
    EFI_STATUS  Status;
    EFI_HANDLE  LastImage;
    UINTN       SetJumpFlag;

    //
    // Push the current image.
    //
    LastImage           = gImageHandle;
    gImageHandle        = ImageHandle;

    //
    // Set long jump for Exit() support
    // JumpContext must be aligned on a CPU specific boundary.
    // Overallocate the buffer and force the required alignment
    //
    OcLoadedImage->JumpBuffer = AllocatePool (
            sizeof (BASE_LIBRARY_JUMP_BUFFER) + BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT
    );

    if (OcLoadedImage->JumpBuffer == NULL) {
        //
        // Pop the current start image context
        //
        gImageHandle = LastImage;
        return EFI_OUT_OF_RESOURCES;
    }

    OcLoadedImage->JumpContext = ALIGN_POINTER (
            OcLoadedImage->JumpBuffer,
            BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT
    );

    SetJumpFlag = SetJump (OcLoadedImage->JumpContext);
    //
    // The initial call to SetJump() must always return 0.
    // Subsequent calls to LongJump() cause a non-zero value to be returned by SetJump().
    //
    if (SetJumpFlag == 0) {
        //
        // Invoke the manually loaded image entry point.
        //
        //Print(L"OCB: Starting image %p\n", ImageHandle);
        OcLoadedImage->Started = TRUE;
        OcLoadedImage->Status  = OcLoadedImage->EntryPoint (
                gImageHandle,
                OcLoadedImage->LoadedImage.SystemTable
        );
        //
        // If the image returns, exit it through Exit()
        //
        gBS->Exit (OcLoadedImage, OcLoadedImage->Status, 0, NULL);
    }

    FreePool (OcLoadedImage->JumpBuffer);

    //
    // Pop the current image.
    //
    gImageHandle = LastImage;

    //
    // NOTE: EFI 1.10 is not supported, refer to
    // https://github.com/tianocore/edk2/blob/d8dd54f071cfd60a2dcf5426764a89cd91213420/MdeModulePkg/Core/Dxe/Image/Image.c#L1686-L1697
    //

    //
    //  Return the exit data to the caller
    //
    if ((ExitData != NULL) && (ExitDataSize != NULL)) {
        *ExitDataSize = OcLoadedImage->ExitDataSize;
        *ExitData     = OcLoadedImage->ExitData;
    } else if (OcLoadedImage->ExitData != NULL) {
        //
        // Caller doesn't want the exit data, free it
        //
        FreePool (OcLoadedImage->ExitData);
        OcLoadedImage->ExitData = NULL;
    }

    //
    // Save the Status because Image will get destroyed if it is unloaded.
    //
    Status = OcLoadedImage->Status;

    //
    // If the image returned an error, or if the image is an application
    // unload it
    //
    if (  EFI_ERROR (OcLoadedImage->Status)
          || (OcLoadedImage->Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION))
    {
        gBS->UnloadImage (ImageHandle);
    }

    return Status;
}

EFI_STATUS
EFIAPI
UnsignedStartImage(IN EFI_HANDLE        ImageHandle,
                   OUT UINTN            *ExitDataSize,
                   OUT CHAR16           **ExitData)

{
    EFI_STATUS                     Status;
    OC_LOADED_IMAGE_PROTOCOL       *OcLoadedImage;

    Status = gBS->HandleProtocol (
            ImageHandle,
            &mOcLoadedImageProtocolGuid,
            (VOID **)&OcLoadedImage
    );
    if (EFI_ERROR (Status)) {
        // We didn't load this file. Try using the original StartImage.

        return OriginalStartImage(ImageHandle,
                                  ExitDataSize,
                                  ExitData);
    }

    return InternalDirectStartImage (
            OcLoadedImage,
            ImageHandle,
            ExitDataSize,
            ExitData
    );

}


/**
  Unload image routine for OcImageLoaderLoad.

  @param[in]  OcLoadedImage     Our loaded image instance.
  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
**/
STATIC
EFI_STATUS
InternalDirectUnloadImage (
  IN  OC_LOADED_IMAGE_PROTOCOL  *OcLoadedImage,
  IN  EFI_HANDLE                ImageHandle
  )
{
    EFI_STATUS                 Status;
    EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;

    LoadedImage = &OcLoadedImage->LoadedImage;
    if (LoadedImage->Unload != NULL) {
        Status = LoadedImage->Unload (ImageHandle);
        if (EFI_ERROR (Status)) {
            return Status;
        }

        //
        // Do not allow to execute Unload multiple times.
        //
        LoadedImage->Unload = NULL;
    } else if (OcLoadedImage->Started) {
        return EFI_UNSUPPORTED;
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ImageHandle,
                    &gEfiLoadedImageProtocolGuid,
                    LoadedImage,
                    &mOcLoadedImageProtocolGuid,
                    OcLoadedImage,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
        return Status;
    }

    FreeAlignedPages ((VOID *)(UINTN)OcLoadedImage->ImageArea, OcLoadedImage->PageCount);
    FreePool (OcLoadedImage);
    //
    // NOTE: Avoid EFI 1.10 extension of closing opened protocols.
    //
    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UnsignedUnloadImage(
    IN EFI_HANDLE ImageHandle
    )
{
    EFI_STATUS                     Status;
    OC_LOADED_IMAGE_PROTOCOL       *OcLoadedImage;
    OC_IMAGE_LOADER_CAPS_PROTOCOL  *OcImageLoaderCaps;

    //
    // If we loaded the image, do the unloading manually.
    //
    Status = gBS->HandleProtocol (
                    ImageHandle,
                    &mOcLoadedImageProtocolGuid,
                    (VOID **)&OcLoadedImage
                    );
    if (!EFI_ERROR (Status)) {
        return InternalDirectUnloadImage (
                 OcLoadedImage,
                 ImageHandle
                 );
    }

    //
    // If we saved image caps during load, free them now.
    //
    Status = gBS->HandleProtocol (
                    ImageHandle,
                    &mOcImageLoaderCapsProtocolGuid,
                    (VOID **)&OcImageLoaderCaps
                    );
    if (!EFI_ERROR (Status)) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ImageHandle,
                        &mOcImageLoaderCapsProtocolGuid,
                        OcImageLoaderCaps,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
            return Status;
        }

        FreePool (OcImageLoaderCaps);
    }

    return OriginalUnloadImage (ImageHandle);
}

#define CpuDeadLoop() while (1) {}

VOID InternalLongJump(VOID *, UINTN);

STATIC
VOID
LongJump(
    IN BASE_LIBRARY_JUMP_BUFFER *JumpBuffer,
    IN UINTN                    Value
    )
{
    InternalLongJump(JumpBuffer, Value);
}

STATIC
EFI_STATUS
InternalDirectExit (
  IN  OC_LOADED_IMAGE_PROTOCOL  *OcLoadedImage,
  IN  EFI_HANDLE                ImageHandle,
  IN  EFI_STATUS                ExitStatus,
  IN  UINTN                     ExitDataSize,
  IN  CHAR16                    *ExitData     OPTIONAL
  )
{
    EFI_TPL  OldTpl;

    /*
    DEBUG ((
      DEBUG_VERBOSE,
      "OCB: Exit %p %p (%d) - %r\n",
      ImageHandle,
      mCurrentImageHandle,
      OcLoadedImage->Started,
      ExitStatus
      ));
      */

    //
    // Prevent possible reentrance to this function for the same ImageHandle.
    //
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

    //
    // If the image has not been started just free its resources.
    // Should not happen normally.
    //
    if (!OcLoadedImage->Started) {
        InternalDirectUnloadImage (OcLoadedImage, ImageHandle);
        gBS->RestoreTPL (OldTpl);
        return EFI_SUCCESS;
    }

    //
    // If the image has been started, verify this image can exit.
    //
    /*
    if (ImageHandle != mCurrentImageHandle) {
        DEBUG ((DEBUG_LOAD|DEBUG_ERROR, "OCB: Image is not exitable image\n"));
        gBS->RestoreTPL (OldTpl);
        return EFI_INVALID_PARAMETER;
    }
    */

    //
    // Set the return status.
    //
    OcLoadedImage->Status = ExitStatus;

    //
    // If there's ExitData info provide it.
    //
    if (ExitData != NULL) {
        OcLoadedImage->ExitDataSize = ExitDataSize;
        OcLoadedImage->ExitData     = AllocatePool (OcLoadedImage->ExitDataSize);
        if (OcLoadedImage->ExitData != NULL) {
            CopyMem (OcLoadedImage->ExitData, ExitData, OcLoadedImage->ExitDataSize);
        } else {
            OcLoadedImage->ExitDataSize = 0;
        }
    }

    //
    // return to StartImage
    //
    gBS->RestoreTPL (OldTpl);
    LongJump (OcLoadedImage->JumpContext, (UINTN)-1);

    //
    // If we return from LongJump, then it is an error
    //
    ASSERT (FALSE);
    CpuDeadLoop ();
    return EFI_ACCESS_DENIED;
}

STATIC
EFI_STATUS
EFIAPI
UnsignedExit (
    IN EFI_HANDLE   ImageHandle,
    IN EFI_STATUS   ExitStatus,
    IN UINTN        ExitDataSize,
    IN CHAR16       *ExitData       OPTIONAL
    )
{
    EFI_STATUS                  Status;
    OC_LOADED_IMAGE_PROTOCOL    *OcLoadedImage;

    Status = gBS->HandleProtocol (
                  ImageHandle,
                  &mOcLoadedImageProtocolGuid,
                  (VOID **)&OcLoadedImage
                  );

    //DEBUG ((DEBUG_VERBOSE, "OCB: InternalEfiExit %p - %r / %r\n", ImageHandle, ExitStatus, Status));

    if (!EFI_ERROR (Status)) {
        return InternalDirectExit (
                 OcLoadedImage,
                 ImageHandle,
                 ExitStatus,
                 ExitDataSize,
                 ExitData
                 );
    }

    return OriginalExit(ImageHandle, ExitStatus, ExitDataSize, ExitData);
}

VOID PatchSystemTable(EFI_SYSTEM_TABLE *SystemTable)
{
    Print(L"Patching system table...");
    EFI_BOOT_SERVICES   *BootServices = SystemTable->BootServices;

    // Preserve original LoadImage and StartImage in case we need them for some reason.
    if (!OriginalLoadImage)
        OriginalLoadImage       = BootServices->LoadImage;

    if (!OriginalStartImage)
        OriginalStartImage      = BootServices->StartImage;

    if (!OriginalUnloadImage)
        OriginalUnloadImage     = BootServices->UnloadImage;

    if (!OriginalExit)
        OriginalExit            = BootServices->Exit;

    BootServices->LoadImage     = UnsignedLoadImage;
    Print(L" LoadImage ");

    BootServices->StartImage    = UnsignedStartImage;
    Print(L" StartImage ");

    BootServices->UnloadImage   = UnsignedUnloadImage;
    Print(L" UnloadImage ");

    BootServices->Exit          = UnsignedExit;
    Print(L" Exit\n");
}
