/* Can I even distribute this */
/* Copyright 2026 Sylas Hollander */


#include <atvlib.h>

// Diagnostics core function
unsigned int gCoreFunction;

EFI_STATUS
EFIAPI
UnsignedLoadImage(IN BOOLEAN            BootPolicy,
                  IN EFI_HANDLE         ParentImageHandle,
                  IN EFI_DEVICE_PATH    *FilePath,
                  IN VOID               *SourceBuffer   OPTIONAL,
                  IN UINTN              SourceSize,
                  OUT EFI_HANDLE        *ImageHandle);

EFI_STATUS
EFIAPI
UnsignedStartImage(IN EFI_HANDLE        ImageHandle,
                   OUT UINTN            *ExitDataSize,
                   OUT CHAR16           **ExitData);

CHAR16 *EfiFilePath = L"\\EFI\\BOOT\\bootia32.efi";

EFI_HANDLE gImageHandle;

int main(int param_1, char **param_2)
{
    EFI_STATUS                  Status;
    EFI_HANDLE                  LoadedImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
    EFI_DEVICE_PATH             *DevicePath;

    EFI_HANDLE ImageHandle;
    EFI_SYSTEM_TABLE *SystemTable;

    // These are the addresses for the handle and system table.
    // Not sure how Rairii got these, but they seem to work...
    ImageHandle = *(EFI_HANDLE *)((size_t)gCoreFunction - 0x11D8);
    SystemTable = *(EFI_SYSTEM_TABLE **)((size_t)gCoreFunction + 0x424);

    gImageHandle = ImageHandle;

    // Initialize GNU-EFI variables
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello from BootServices!\n");

    Status = BS->OpenProtocol(ImageHandle,
                              &LoadedImageProtocol,
                              (VOID **) &LoadedImage,
                              ImageHandle,
                              NULL,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status))
    {
        Print(L"Failed to get LoadedImageProtocol! (%r)\n", Status);
    }

    Print(L"Found image handle\n");

    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, EfiFilePath);
    if (DevicePath == NULL)
    {
        Print(L"Failed to create device path for file!\n");
        goto hang;
    }

    Print(L"Created device path %D\n", DevicePath);

    PatchLoadStartImage(SystemTable);

    Status = BS->LoadImage(FALSE, ImageHandle, DevicePath, NULL, 0, &LoadedImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load EFI file! (%r)\n", Status);
        goto hang;
    }

    Print(L"Loaded EFI file\n");

    Status = BS->StartImage(LoadedImageHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start EFI file! (%r)\n", Status);
        goto hang;
    }

    Print(L"EFI file exited\n");

    goto hang;

    hang:
    while (1);

    return 0;
}
