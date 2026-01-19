/* Can I even distribute this */
/* Copyright 2026 Sylas Hollander */


#include <atvlib.h>

// Diagnostics core function
unsigned int gCoreFunction;

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

    Print(L"Apple TV Boot Services Bypass started.\n");

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

    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, EfiFilePath);
    if (DevicePath == NULL)
    {
        Print(L"Failed to create device path for file!\n");
        goto hang;
    }

    PatchLoadStartImage(SystemTable);

    Status = BS->LoadImage(FALSE, ImageHandle, DevicePath, NULL, 0, &LoadedImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load EFI file! (%r)\n", Status);
        goto hang;
    }

    Print(L"Loaded EFI file %s\n", EfiFilePath);

    Status = BS->StartImage(LoadedImageHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start EFI file! (%r)\n", Status);
        goto hang;
    }

    Print(L"EFI file exited with status %r\n", Status);

    return 0;

hang:
    while (1);
}
