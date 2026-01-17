/* Can I even distribute this */
/* Copyright 2026 Sylas Hollander */


#include <atvlib.h>

EFI_HANDLE gImageHandle;
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;

extern RETURN_STATUS
UnicodeStrToAsciiStrS (
        CONST CHAR16              *Source,
        CHAR8                     *Destination,
        UINTN                     DestMax
);

// Diagnostics core function
unsigned int gCoreFunction;

CHAR16 *EfiFilePath = L"\\EFI\\BOOT\\bootia32.efi";

int main(int param_1, char **param_2)
{
    EFI_STATUS Status;
    EFI_HANDLE LoadedImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_DEVICE_PATH *DevicePath;

    EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    // These are the addresses for the handle and system table.
    // Not sure how Rairii got these, but they seem to work...
    gImageHandle = *(EFI_HANDLE *)((size_t)gCoreFunction - 0x11D8);
    gST = *(EFI_SYSTEM_TABLE **)((size_t)gCoreFunction + 0x424);
    gBS = gST->BootServices;

    cons_init(NULL, 0xFFFFFF00, 0x00000000);

    // Get loaded image protocol
    Status = gBS->OpenProtocol(gImageHandle,
                               &LoadedImageProtocol,
                               (VOID **) &LoadedImage,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (Status != EFI_SUCCESS)
    {
        printf("Failed to get EFI loaded image protocol! (%d)\n", EFI_ERROR(Status));
        goto hang;
    }

    DevicePath = FileDevicePath(LoadedImage->DeviceHandle, EfiFilePath);
    if (DevicePath == NULL)
    {
        printf("Failed to create device path for file!\n");
    }

    // Now we load bootia32.
    Status = gBS->LoadImage(FALSE,
                            gImageHandle,
                            DevicePath,
                            NULL,
                            0,
                            &LoadedImageHandle);

    if (Status != EFI_SUCCESS)
    {
        printf("Failed to load EFI file! (%d)\n", EFI_ERROR(Status));
        goto hang;
    }

    Status = gBS->StartImage(LoadedImageHandle,
                             NULL,
                             NULL);

    if (Status != EFI_SUCCESS)
    {
        printf("Failed to start EFI file! (%d)\n", EFI_ERROR(Status));
        goto hang;
    }

    printf("Returned success but didn't load!\n");
    goto hang;

    hang:
    while (1);

    return 0;
}
