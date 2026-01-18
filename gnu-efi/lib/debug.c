/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    debug.c

Abstract:

    Debug library functions



Revision History

--*/

#include "lib.h"



//
// Declare runtime functions
//

//
//
//

INTN
DbgAssert (
    IN CONST CHAR8    *FileName,
    IN INTN           LineNo,
    IN CONST CHAR8    *Description
    )
{
    Print (L"%EASSERT FAILED: %a(%d): %a%N\n", FileName, LineNo, Description);

    BREAKPOINT();
    return 0;
}

