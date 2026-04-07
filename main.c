/*
 * ATV1sm - Apple TV EFI bypass
 * SPDX-License-Identifier: MIT
 */

#include <atv1sm.h>

/*
 * Diagnostics core function.
 * This gets set to an address within TestSupport.efi.
 */
unsigned int gCoreFunction;

// GOPShim path
CHAR16 *GopShimFilePath = L"\\System\\Library\\CoreServices\\Runtime_Files\\EFI\\Drivers\\GopShimDxe.efi";

// bootia32 EFI file path
CHAR16 *BootEfiFilePath = L"\\EFI\\BOOT\\bootia32.efi";

EFI_HANDLE gImageHandle;

#define VERSION_STRING  "0.0.1"
#define COPYRIGHT_YEAR  "2026"
#define AUTHOR          "Sylas Hollander"
#define AUTHOR_SITE     "<www.distrohopper39b.com>"

VOID Header(VOID)
{
    Print(L"ATV1sm version " VERSION_STRING "\n");
    Print(L"Copyright " COPYRIGHT_YEAR " " AUTHOR " " AUTHOR_SITE "\n");
}

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

    /*
     * Calculate the addresses of the EFI system table and image handle
     * This is based on the offset of gST and gImageHandle as compared to gCoreFunction in TestSupport.efi.
     * Thanks to Rairii (https://github.com/wack0) for finding these!
     */
    ImageHandle = *(EFI_HANDLE *)((size_t)gCoreFunction - 0x11D8);
    SystemTable = *(EFI_SYSTEM_TABLE **)((size_t)gCoreFunction + 0x424);

    gImageHandle = ImageHandle;

    // Initialize GNU-EFI variables
    InitializeLib(ImageHandle, SystemTable);

    Header();

    Print(L"ImageHandle = 0x%X, SystemTable = 0x%X\n", ImageHandle, SystemTable);

    PatchSystemTable(SystemTable);

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

    BootEfiDevicePath = FileDevicePath(LoadedImage->DeviceHandle, BootEfiFilePath);
    if (BootEfiDevicePath == NULL)
    {
        Print(L"Failed to create device path for file!\n");
        goto reboot;
    }

    GopShimDevicePath = FileDevicePath(LoadedImage->DeviceHandle, GopShimFilePath);
    if (GopShimDevicePath == NULL)
    {
        Print(L"Failed to create device path for GOP shim!\n");
        goto reboot;
    }

    // Load GopShim
    Print(L"Loading GopShim...");
    Status = BS->LoadImage(FALSE,
        ImageHandle,
        GopShimDevicePath,
        NULL,
        0,
        &GopShimLoadedImageHandle);

    if (EFI_ERROR(Status))
    {
        Print(L"WARNING: %r when attempting to load GopShim from %s\n", Status, GopShimFilePath);
        Print(L"Most bootloaders and operating systems will not have working video.\n");
        // continue
    }
    else
    {
        // Start GopShim
        Status = BS->StartImage(GopShimLoadedImageHandle, NULL, NULL);
        if (EFI_ERROR(Status))
        {
            Print(L"Failed to start GopShim EFI file! (%r)\n", Status);
            goto reboot;
        }

        Print(L"Success\n");
    }

    // Load bootia32
    Print(L"Loading %s...", BootEfiFilePath);

    Status = BS->LoadImage(FALSE,
        ImageHandle,
        BootEfiDevicePath,
        NULL,
        0,
        &BootEfiLoadedImageHandle);

    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load %s (%r)\n", BootEfiFilePath, Status);
        goto reboot;
    }

    Print(L"Success\n");

    // Start bootia32
    Print(L"Starting %s...\n", BootEfiFilePath);
    Status = BS->StartImage(BootEfiLoadedImageHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to start %s (%r)\n", BootEfiFilePath, Status);
        goto reboot;
    }

    Print(L"EFI file exited with status %r\n", Status);

reboot:
    Print(L"Rebooting in 15 seconds...\n");
    gBS->Stall(15 * 1000000);
    gST->RuntimeServices->ResetSystem(EfiResetCold,
        EFI_SUCCESS,
        0,
        NULL);

    return EFI_INVALID_PARAMETER;
}
