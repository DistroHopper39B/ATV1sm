#include <atvlib.h>


#define SAFE_STRING_CONSTRAINT_CHECK(Expression, Status)  \
  do { \
    ASSERT (Expression); \
    if (!(Expression)) { \
      return Status; \
    } \
  } while (FALSE)

#define RSIZE_MAX 1000000

VOID *EFIAPI AllocatePool(IN UINTN AllocationSize)
{
    EFI_STATUS Status;
    VOID *Buffer = NULL;

    Status = gBS->AllocatePool(EfiBootServicesData, AllocationSize, (VOID **) &Buffer);
    if (Status != EFI_SUCCESS || Buffer == NULL)
    {
        return NULL;
    }

    return Buffer;
}

VOID *
AllocateCopyPool (
        UINTN            AllocationSize,
        CONST VOID       *Buffer
)
{
    VOID  *Memory;

    ASSERT (Buffer != NULL);

    Memory = AllocatePool (AllocationSize);
    if (Memory != NULL) {
        Memory = memcpy (Memory, Buffer, AllocationSize);
    }
    return Memory;
}

VOID *
AllocateZeroPool (
        UINTN  AllocationSize
)
{
    VOID * Memory;
    Memory = AllocatePool(AllocationSize);
    ASSERT (Memory != NULL);
    memset(Memory, 0, AllocationSize);
    return Memory;
}

VOID EFIAPI FreePool(IN VOID *Buffer)
{
    gBS->FreePool(Buffer);
}

VOID *
ReallocatePool (
        UINTN            OldSize,
        UINTN            NewSize,
        VOID             *OldBuffer  OPTIONAL
)
{
    VOID  *NewBuffer;

    NewBuffer = AllocateZeroPool (NewSize);
    if (NewBuffer != NULL && OldBuffer != NULL) {
        memcpy (NewBuffer, OldBuffer, MIN (OldSize, NewSize));
        FreePool(OldBuffer);
    }
    return NewBuffer;
}

UINT16
WriteUnaligned16 (
        UINT16                    *Buffer,
        UINT16                    Value
)
{
    ASSERT (Buffer != NULL);

    return *Buffer = Value;
}

UINT16
ReadUnaligned16 (
        CONST UINT16              *Buffer
)
{
    ASSERT (Buffer != NULL);

    return *Buffer;
}

UINT64
ReadUnaligned64 (
        CONST UINT64              *Buffer
)
{
    ASSERT (Buffer != NULL);

    return *Buffer;
}

UINT64
WriteUnaligned64 (
        UINT64                    *Buffer,
        UINT64                    Value
)
{
    ASSERT (Buffer != NULL);

    return *Buffer = Value;
}

UINTN
StrnLenS (
        CONST CHAR16              *String,
        UINTN                     MaxSize
)
{
    UINTN     Length;

    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // If String is a null pointer or MaxSize is 0, then the StrnLenS function returns zero.
    //
    if ((String == NULL) || (MaxSize == 0)) {
        return 0;
    }

    Length = 0;
    while (String[Length] != 0) {
        if (Length >= MaxSize - 1) {
            return MaxSize;
        }
        Length++;
    }
    return Length;
}

BOOLEAN
InternalSafeStringIsOverlap (
        IN VOID    *Base1,
        IN UINTN   Size1,
        IN VOID    *Base2,
        IN UINTN   Size2
)
{
    if ((((UINTN)Base1 >= (UINTN)Base2) && ((UINTN)Base1 < (UINTN)Base2 + Size2)) ||
        (((UINTN)Base2 >= (UINTN)Base1) && ((UINTN)Base2 < (UINTN)Base1 + Size1))) {
        return TRUE;
    }
    return FALSE;
}

BOOLEAN
InternalSafeStringNoStrOverlap (
        IN CHAR16  *Str1,
        IN UINTN   Size1,
        IN CHAR16  *Str2,
        IN UINTN   Size2
)
{
    return !InternalSafeStringIsOverlap (Str1, Size1 * sizeof(CHAR16), Str2, Size2 * sizeof(CHAR16));
}

INTN
StrCmp (
        CONST CHAR16              *FirstString,
        CONST CHAR16              *SecondString
)
{
    //
    // ASSERT both strings are less long than PcdMaximumUnicodeStringLength
    //
    ASSERT (StrSize (FirstString) != 0);
    ASSERT (StrSize (SecondString) != 0);

    while ((*FirstString != L'\0') && (*FirstString == *SecondString)) {
        FirstString++;
        SecondString++;
    }
    return *FirstString - *SecondString;
}

RETURN_STATUS
StrCpyS (
        CHAR16       *Destination,
        UINTN        DestMax,
        CONST CHAR16 *Source
)
{
    UINTN            SourceLen;

    ASSERT (((UINTN) Destination & BIT0) == 0);
    ASSERT (((UINTN) Source & BIT0) == 0);

    //
    // 1. Neither Destination nor Source shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

    //
    // 2. DestMax shall not be greater than RSIZE_MAX.
    //
    if (RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }

    //
    // 3. DestMax shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

    //
    // 4. DestMax shall be greater than StrnLenS(Source, DestMax).
    //
    SourceLen = StrnLenS (Source, DestMax);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

    //
    // 5. Copying shall not take place between objects that overlap.
    //
    SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoStrOverlap (Destination, DestMax, (CHAR16 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

    //
    // The StrCpyS function copies the string pointed to by Source (including the terminating
    // null character) into the array pointed to by Destination.
    //
    while (*Source != 0) {
        *(Destination++) = *(Source++);
    }
    *Destination = 0;

    return RETURN_SUCCESS;
}

BOOLEAN
InternalIsDecimalDigitCharacter (
        CHAR16                    Char
)
{
    return (BOOLEAN) (Char >= L'0' && Char <= L'9');
}

RETURN_STATUS
StrDecimalToUint64S (
        CONST CHAR16             *String,
        CHAR16             **EndPointer,  OPTIONAL
        UINT64             *Data
)
{
    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. Neither String nor Data shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

    //
    // 2. The length of String shall not be greater than RSIZE_MAX.
    //
    if (RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *) String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == L' ') || (*String == L'\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == L'0') {
        String++;
    }

    *Data = 0;

    while (InternalIsDecimalDigitCharacter (*String)) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINT64, then MAX_UINT64 is stored in *Data and
        // RETURN_UNSUPPORTED is returned.
        //
        if (*Data > ((MAX_UINT64 - (*String - L'0'))/10)) {
            *Data = MAX_UINT64;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR16 *) String;
            }
            return RETURN_UNSUPPORTED;
        }

        *Data = (*Data) * 10 + (*String - L'0');
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *) String;
    }
    return RETURN_SUCCESS;
}


UINT64
StrDecimalToUint64 (
        CONST CHAR16              *String
)
{
    UINT64     Result;

    StrDecimalToUint64S (String, (CHAR16 **) NULL, &Result);
    return Result;
}

CHAR16
InternalCharToUpper (
        CHAR16                    Char
)
{
    if (Char >= L'a' && Char <= L'z') {
        return (CHAR16) (Char - (L'a' - L'A'));
    }

    return Char;
}

UINTN
InternalHexCharToUintn (
        CHAR16                    Char
)
{
    if (InternalIsDecimalDigitCharacter (Char)) {
        return Char - L'0';
    }

    return (10 + InternalCharToUpper (Char) - L'A');
}

BOOLEAN
InternalIsHexaDecimalDigitCharacter (
        CHAR16                    Char
)
{

    return (BOOLEAN) (InternalIsDecimalDigitCharacter (Char) ||
                      (Char >= L'A' && Char <= L'F') ||
                      (Char >= L'a' && Char <= L'f'));
}

RETURN_STATUS
StrHexToBytes (
        CONST CHAR16       *String,
        UINTN              Length,
        UINT8              *Buffer,
        UINTN              MaxBufferSize
)
{
    UINTN                  Index;

    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. None of String or Buffer shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Buffer != NULL), RETURN_INVALID_PARAMETER);

    //
    // 2. Length shall not be greater than RSIZE_MAX.
    //
    if (RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }

    //
    // 3. Length shall not be odd.
    //
    SAFE_STRING_CONSTRAINT_CHECK (((Length & BIT0) == 0), RETURN_INVALID_PARAMETER);

    //
    // 4. MaxBufferSize shall equal to or greater than Length / 2.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((MaxBufferSize >= Length / 2), RETURN_BUFFER_TOO_SMALL);

    //
    // 5. String shall not contains invalid hexadecimal digits.
    //
    for (Index = 0; Index < Length; Index++) {
        if (!InternalIsHexaDecimalDigitCharacter (String[Index])) {
            break;
        }
    }
    if (Index != Length) {
        return RETURN_UNSUPPORTED;
    }

    //
    // Convert the hex string to bytes.
    //
    for(Index = 0; Index < Length; Index++) {

        //
        // For even characters, write the upper nibble for each buffer byte,
        // and for even characters, the lower nibble.
        //
        if ((Index & BIT0) == 0) {
            Buffer[Index / 2]  = (UINT8) InternalHexCharToUintn (String[Index]) << 4;
        } else {
            Buffer[Index / 2] |= (UINT8) InternalHexCharToUintn (String[Index]);
        }
    }
    return RETURN_SUCCESS;
}


UINT16
SwapBytes16 (
        UINT16                    Value
)
{
    return (UINT16) ((Value<< 8) | (Value>> 8));
}


UINT32
SwapBytes32 (
        UINT32                    Value
)
{
    UINT32  LowerBytes;
    UINT32  HigherBytes;

    LowerBytes  = (UINT32) SwapBytes16 ((UINT16) Value);
    HigherBytes = (UINT32) SwapBytes16 ((UINT16) (Value >> 16));
    return (LowerBytes << 16 | HigherBytes);
}

UINT64
InternalMathSwapBytes64 (
        UINT64                    Operand
)
{
    UINT64  LowerBytes;
    UINT64  HigherBytes;

    LowerBytes  = (UINT64) SwapBytes32 ((UINT32) Operand);
    HigherBytes = (UINT64) SwapBytes32 ((UINT32) (Operand >> 32));

    return (LowerBytes << 32 | HigherBytes);
}

UINT64
SwapBytes64 (
        UINT64                    Value
)
{
    return InternalMathSwapBytes64 (Value);
}

static
EFI_GUID *
CopyGuid (
        EFI_GUID         *DestinationGuid,
        CONST EFI_GUID  *SourceGuid
)
{
    WriteUnaligned64 (
            (UINT64*)DestinationGuid,
            ReadUnaligned64 ((CONST UINT64*)SourceGuid)
    );
    WriteUnaligned64 (
            (UINT64*)DestinationGuid + 1,
            ReadUnaligned64 ((CONST UINT64*)SourceGuid + 1)
    );
    return DestinationGuid;
}

RETURN_STATUS
StrToGuid (
        CONST CHAR16       *String,
        EFI_GUID           *Guid
)
{
    RETURN_STATUS          Status;
    EFI_GUID               LocalGuid;

    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. None of String or Guid shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Guid != NULL), RETURN_INVALID_PARAMETER);

    //
    // Get aabbccdd in big-endian.
    //
    Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data1), (UINT8 *) &LocalGuid.Data1, sizeof (LocalGuid.Data1));
    if (RETURN_ERROR (Status) || String[2 * sizeof (LocalGuid.Data1)] != L'-') {
        return RETURN_UNSUPPORTED;
    }
    //
    // Convert big-endian to little-endian.
    //
    LocalGuid.Data1 = SwapBytes32 (LocalGuid.Data1);
    String += 2 * sizeof (LocalGuid.Data1) + 1;

    //
    // Get eeff in big-endian.
    //
    Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data2), (UINT8 *) &LocalGuid.Data2, sizeof (LocalGuid.Data2));
    if (RETURN_ERROR (Status) || String[2 * sizeof (LocalGuid.Data2)] != L'-') {
        return RETURN_UNSUPPORTED;
    }
    //
    // Convert big-endian to little-endian.
    //
    LocalGuid.Data2 = SwapBytes16 (LocalGuid.Data2);
    String += 2 * sizeof (LocalGuid.Data2) + 1;

    //
    // Get gghh in big-endian.
    //
    Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data3), (UINT8 *) &LocalGuid.Data3, sizeof (LocalGuid.Data3));
    if (RETURN_ERROR (Status) || String[2 * sizeof (LocalGuid.Data3)] != L'-') {
        return RETURN_UNSUPPORTED;
    }
    //
    // Convert big-endian to little-endian.
    //
    LocalGuid.Data3 = SwapBytes16 (LocalGuid.Data3);
    String += 2 * sizeof (LocalGuid.Data3) + 1;

    //
    // Get iijj.
    //
    Status = StrHexToBytes (String, 2 * 2, &LocalGuid.Data4[0], 2);
    if (RETURN_ERROR (Status) || String[2 * 2] != L'-') {
        return RETURN_UNSUPPORTED;
    }
    String += 2 * 2 + 1;

    //
    // Get kkllmmnnoopp.
    //
    Status = StrHexToBytes (String, 2 * 6, &LocalGuid.Data4[2], 6);
    if (RETURN_ERROR (Status)) {
        return RETURN_UNSUPPORTED;
    }

    CopyGuid (Guid, &LocalGuid);
    return RETURN_SUCCESS;
}

RETURN_STATUS
StrHexToUint64S (
        CONST CHAR16             *String,
        CHAR16             **EndPointer,  OPTIONAL
        UINT64             *Data
)
{
    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. Neither String nor Data shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

    //
    // 2. The length of String shall not be greater than RSIZE_MAX.
    //
    if (RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *) String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == L' ') || (*String == L'\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == L'0') {
        String++;
    }

    if (InternalCharToUpper (*String) == L'X') {
        if (*(String - 1) != L'0') {
            *Data = 0;
            return RETURN_SUCCESS;
        }
        //
        // Skip the 'X'
        //
        String++;
    }

    *Data = 0;

    while (InternalIsHexaDecimalDigitCharacter (*String)) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINT64, then MAX_UINT64 is stored in *Data and
        // RETURN_UNSUPPORTED is returned.
        //
        if (*Data > ((MAX_UINT64 - InternalHexCharToUintn (*String))>>4)) {
            *Data = MAX_UINT64;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR16 *) String;
            }
            return RETURN_UNSUPPORTED;
        }

        *Data =  ((*Data) << 4) + InternalHexCharToUintn (*String);
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *) String;
    }
    return RETURN_SUCCESS;
}

UINT64
StrHexToUint64 (
        CONST CHAR16             *String
)
{
    UINT64    Result;

    StrHexToUint64S (String, (CHAR16 **) NULL, &Result);
    return Result;
}

RETURN_STATUS
StrToIpv4Address (
        CONST CHAR16       *String,
        CHAR16             **EndPointer,
        EFI_IPv4_ADDRESS       *Address,
        UINT8              *PrefixLength
)
{
    RETURN_STATUS          Status;
    UINTN                  AddressIndex;
    UINT64                 Uint64;
    EFI_IPv4_ADDRESS       LocalAddress;
    UINT8                  LocalPrefixLength;
    CHAR16                 *Pointer;

    LocalPrefixLength = MAX_UINT8;
    LocalAddress.Addr[0] = 0;

    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. None of String or Guid shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

    for (Pointer = (CHAR16 *) String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
        if (!InternalIsDecimalDigitCharacter (*Pointer)) {
            //
            // D or P contains invalid characters.
            //
            break;
        }

        //
        // Get D or P.
        //
        Status = StrDecimalToUint64S ((CONST CHAR16 *) Pointer, &Pointer, &Uint64);
        if (RETURN_ERROR (Status)) {
            return RETURN_UNSUPPORTED;
        }
        if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
            //
            // It's P.
            //
            if (Uint64 > 32) {
                return RETURN_UNSUPPORTED;
            }
            LocalPrefixLength = (UINT8) Uint64;
        } else {
            //
            // It's D.
            //
            if (Uint64 > MAX_UINT8) {
                return RETURN_UNSUPPORTED;
            }
            LocalAddress.Addr[AddressIndex] = (UINT8) Uint64;
            AddressIndex++;
        }

        //
        // Check the '.' or '/', depending on the AddressIndex.
        //
        if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
            if (*Pointer == L'/') {
                //
                // '/P' is in the String.
                // Skip "/" and get P in next loop.
                //
                Pointer++;
            } else {
                //
                // '/P' is not in the String.
                //
                break;
            }
        } else if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
            if (*Pointer == L'.') {
                //
                // D should be followed by '.'
                //
                Pointer++;
            } else {
                return RETURN_UNSUPPORTED;
            }
        }
    }

    if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
        return RETURN_UNSUPPORTED;
    }

    memcpy (Address, &LocalAddress, sizeof (*Address));
    if (PrefixLength != NULL) {
        *PrefixLength = LocalPrefixLength;
    }
    if (EndPointer != NULL) {
        *EndPointer = Pointer;
    }

    return RETURN_SUCCESS;
}

RETURN_STATUS
StrToIpv6Address (
        CONST CHAR16       *String,
        CHAR16             **EndPointer,
        EFI_IPv6_ADDRESS   *Address,
        UINT8              *PrefixLength
)
{
    RETURN_STATUS          Status;
    UINTN                  AddressIndex;
    UINT64                 Uint64;
    EFI_IPv6_ADDRESS       LocalAddress;
    UINT8                  LocalPrefixLength;
    CONST CHAR16           *Pointer;
    CHAR16                 *End;
    UINTN                  CompressStart;
    BOOLEAN                ExpectPrefix;

    LocalPrefixLength = MAX_UINT8;
    CompressStart     = ARRAY_SIZE (Address->Addr);
    ExpectPrefix      = FALSE;

    ASSERT (((UINTN) String & BIT0) == 0);

    //
    // 1. None of String or Guid shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

    for (Pointer = String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
        if (!InternalIsHexaDecimalDigitCharacter (*Pointer)) {
            if (*Pointer != L':') {
                //
                // ":" or "/" should be followed by digit characters.
                //
                return RETURN_UNSUPPORTED;
            }

            //
            // Meet second ":" after previous ":" or "/"
            // or meet first ":" in the beginning of String.
            //
            if (ExpectPrefix) {
                //
                // ":" shall not be after "/"
                //
                return RETURN_UNSUPPORTED;
            }

            if (CompressStart != ARRAY_SIZE (Address->Addr) || AddressIndex == ARRAY_SIZE (Address->Addr)) {
                //
                // "::" can only appear once.
                // "::" can only appear when address is not full length.
                //
                return RETURN_UNSUPPORTED;
            } else {
                //
                // Remember the start of zero compressing.
                //
                CompressStart = AddressIndex;
                Pointer++;

                if (CompressStart == 0) {
                    if (*Pointer != L':') {
                        //
                        // Single ":" shall not be in the beginning of String.
                        //
                        return RETURN_UNSUPPORTED;
                    }
                    Pointer++;
                }
            }
        }

        if (!InternalIsHexaDecimalDigitCharacter (*Pointer)) {
            if (*Pointer == L'/') {
                //
                // Might be optional "/P" after "::".
                //
                if (CompressStart != AddressIndex) {
                    return RETURN_UNSUPPORTED;
                }
            } else {
                break;
            }
        } else {
            if (!ExpectPrefix) {
                //
                // Get X.
                //
                Status = StrHexToUint64S (Pointer, &End, &Uint64);
                if (RETURN_ERROR (Status) || End - Pointer > 4) {
                    //
                    // Number of hexadecimal digit characters is no more than 4.
                    //
                    return RETURN_UNSUPPORTED;
                }
                Pointer = End;
                //
                // Uint64 won't exceed MAX_UINT16 if number of hexadecimal digit characters is no more than 4.
                //
                ASSERT (AddressIndex + 1 < ARRAY_SIZE (Address->Addr));
                LocalAddress.Addr[AddressIndex] = (UINT8) ((UINT16) Uint64 >> 8);
                LocalAddress.Addr[AddressIndex + 1] = (UINT8) Uint64;
                AddressIndex += 2;
            } else {
                //
                // Get P, then exit the loop.
                //
                Status = StrDecimalToUint64S (Pointer, &End, &Uint64);
                if (RETURN_ERROR (Status) || End == Pointer || Uint64 > 128) {
                    //
                    // Prefix length should not exceed 128.
                    //
                    return RETURN_UNSUPPORTED;
                }
                LocalPrefixLength = (UINT8) Uint64;
                Pointer = End;
                break;
            }
        }

        //
        // Skip ':' or "/"
        //
        if (*Pointer == L'/') {
            ExpectPrefix = TRUE;
        } else if (*Pointer == L':') {
            if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
                //
                // Meet additional ":" after all 8 16-bit address
                //
                break;
            }
        } else {
            //
            // Meet other character that is not "/" or ":" after all 8 16-bit address
            //
            break;
        }
        Pointer++;
    }

    if ((AddressIndex == ARRAY_SIZE (Address->Addr) && CompressStart != ARRAY_SIZE (Address->Addr)) ||
        (AddressIndex != ARRAY_SIZE (Address->Addr) && CompressStart == ARRAY_SIZE (Address->Addr))
            ) {
        //
        // Full length of address shall not have compressing zeros.
        // Non-full length of address shall have compressing zeros.
        //
        return RETURN_UNSUPPORTED;
    }
    memcpy (&Address->Addr[0], &LocalAddress.Addr[0], CompressStart);
    if (AddressIndex > CompressStart) {
        memset (&Address->Addr[CompressStart], 0,  ARRAY_SIZE (Address->Addr) - AddressIndex);
        memcpy (
                &Address->Addr[CompressStart + ARRAY_SIZE (Address->Addr) - AddressIndex],
                &LocalAddress.Addr[CompressStart],
                AddressIndex - CompressStart
        );
    }

    if (PrefixLength != NULL) {
        *PrefixLength = LocalPrefixLength;
    }
    if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *) Pointer;
    }

    return RETURN_SUCCESS;
}

#define ASCII_RSIZE_MAX 1000000

RETURN_STATUS
UnicodeStrToAsciiStrS (
        CONST CHAR16              *Source,
        CHAR8                     *Destination,
        UINTN                     DestMax
)
{
    UINTN            SourceLen;

    ASSERT (((UINTN) Source & BIT0) == 0);

    //
    // 1. Neither Destination nor Source shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

    //
    // 2. DestMax shall not be greater than ASCII_RSIZE_MAX or RSIZE_MAX.
    //
    if (ASCII_RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }
    if (RSIZE_MAX != 0) {
        SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }

    //
    // 3. DestMax shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

    //
    // 4. DestMax shall be greater than StrnLenS (Source, DestMax).
    //
    SourceLen = StrnLenS (Source, DestMax);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

    //
    // 5. Copying shall not take place between objects that overlap.
    //
    SAFE_STRING_CONSTRAINT_CHECK (!InternalSafeStringIsOverlap (Destination, DestMax, (VOID *)Source, (SourceLen + 1) * sizeof(CHAR16)), RETURN_ACCESS_DENIED);

    //
    // convert string
    //
    while (*Source != '\0') {
        //
        // If any Unicode characters in Source contain
        // non-zero value in the upper 8 bits, then ASSERT().
        //
        ASSERT (*Source < 0x100);
        *(Destination++) = (CHAR8) *(Source++);
    }
    *Destination = '\0';

    return RETURN_SUCCESS;
}

INTN
StrnCmp (
        CONST CHAR16              *FirstString,
        CONST CHAR16              *SecondString,
        UINTN                     Length
)
{
    if (Length == 0) {
        return 0;
    }

    //
    // ASSERT both strings are less long than PcdMaximumUnicodeStringLength.
    // Length tests are performed inside StrLen().
    //
    ASSERT (StrSize (FirstString) != 0);
    ASSERT (StrSize (SecondString) != 0);

    while ((*FirstString != L'\0') &&
           (*SecondString != L'\0') &&
           (*FirstString == *SecondString) &&
           (Length > 1)) {
        FirstString++;
        SecondString++;
        Length--;
    }

    return *FirstString - *SecondString;
}

UINTN
StrLen (
        CONST CHAR16              *String
)
{
    UINTN   Length;

    ASSERT (String != NULL);
    ASSERT (((UINTN) String & BIT0) == 0);

    for (Length = 0; *String != L'\0'; String++, Length++) {
        //
        // If PcdMaximumUnicodeStringLength is not zero,
        // length should not more than PcdMaximumUnicodeStringLength
        //
    }
    return Length;
}

UINTN
StrSize (
        CONST CHAR16              *String
)
{
    return (StrLen (String) + 1) * sizeof (*String);
}

/* GUIDs */

