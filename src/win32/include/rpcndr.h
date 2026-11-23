#pragma once

// Interface declaration macros used by MIDL-generated headers.
// https://learn.microsoft.com/windows/win32/midl/com-c-and-cpp-interfaces

#include <rpc.h>

#define __RPC_FAR
#define __RPC_API
#define __RPC_USER
#define __RPC_STUB
#define RPC_ENTRY

#define MIDL_INTERFACE(uuid) struct
#define BEGIN_INTERFACE
#define END_INTERFACE
#define CONST_VTBL const

typedef unsigned char boolean;
typedef unsigned char byte;
