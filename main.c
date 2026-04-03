/* Can I even distribute this */
/* Copyright 2026 Sylas Hollander */


#include <atvlib.h>

// Diagnostics core function
unsigned int gCoreFunction;

// GOPShim path
CHAR16 *GopShimFilePath = L"\\System\\Library\\CoreServices\\Runtime_Files\\EFI\\Drivers\\GopShimDxe.efi";

// bootia32 EFI file path
CHAR16 *BootEfiFilePath = L"\\EFI\\BOOT\\bootia32.efi";

EFI_HANDLE gImageHandle;

int main(void)
{
    EFI_STATUS                  Status;
    EFI_HANDLE                  BootEfiLoadedImageHandle;
    EFI_HANDLE                  GopShimLoadedImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
    EFI_DEVICE_PATH             *BootEfiDevicePath;
    EFI_DEVICE_PATH             *GopShimDevicePath;

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

    PatchLoadStartImage(SystemTable);

    BootEfiDevicePath = FileDevicePath(LoadedImage->DeviceHandle, BootEfiFilePath);
    if (BootEfiDevicePath == NULL)
    {
        Print(L"Failed to create device path for file!\n");
        goto hang;
    }

    GopShimDevicePath = FileDevicePath(LoadedImage->DeviceHandle, GopShimFilePath);
    if (GopShimDevicePath == NULL)
    {
        Print(L"Failed to create device path for GOP shim!\n");
        goto hang;
    }

    // Load GopShim
    Status = BS->LoadImage(FALSE, ImageHandle, GopShimDevicePath, NULL, 0, &GopShimLoadedImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"GOPShim not found!\n");
        // continue
    }
    else
    {
        // Start GopShim
        Status = BS->StartImage(GopShimLoadedImageHandle, NULL, NULL);
        if (EFI_ERROR(Status))
        {
            Print(L"Failed to start GOPShim EFI file! (%r)\n", Status);
            goto hang;
        }

        Print(L"GOP shim loaded\n");
    }

    // Load bootia32
    Status = BS->LoadImage(FALSE, ImageHandle, BootEfiDevicePath, NULL, 0, &BootEfiLoadedImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load \\EFI\\BOOT\\BOOTIA32.EFI (%r)\n", Status);
        goto hang;
    }

    Print(L"Loaded EFI file %s\n", BootEfiFilePath);
    // Start bootia32
    Status = BS->StartImage(BootEfiLoadedImageHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start \\EFI\\BOOT\\BOOTIA32.EFI (%r)\n", Status);
        goto hang;
    }

    Print(L"EFI file exited with status %r\n", Status);

    return 0;

hang:
    while (1);
}
