/* Glue for OpenCore compatibility layer */

#pragma once

EFI_STATUS
EFIAPI
OcOpenFileByDevicePath (
        IN OUT EFI_DEVICE_PATH_PROTOCOL  **FilePath,
        OUT    EFI_FILE_PROTOCOL         **File,
        IN     UINT64                    OpenMode,
        IN     UINT64                    Attributes
);

VOID *
OcGetFileInfo (
        IN  EFI_FILE_PROTOCOL  *File,
        IN  EFI_GUID           *InformationType,
        IN  UINTN              MinFileInfoSize,
        OUT UINTN              *RealFileInfoSize  OPTIONAL
);

EFI_STATUS
OcGetFileSize (
        IN  EFI_FILE_PROTOCOL  *File,
        OUT UINT32             *Size
);

EFI_STATUS
OcGetFileData (
        IN  EFI_FILE_PROTOCOL  *File,
        IN  UINT32             Position,
        IN  UINT32             Size,
        OUT UINT8              *Buffer
);

EFI_STATUS
FatFilterArchitecture32 (
        IN OUT UINT8   **FileData,
        IN OUT UINT32  *FileSize
);