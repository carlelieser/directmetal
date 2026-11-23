#pragma once

// First-party replacement for the Windows SDK base headers in native macOS builds (ADR 0010). It
// declares only what the pinned Direct3D and DXGI headers and the front ends need; it declares no
// Windows API functions.

#ifdef __OBJC__
#error "the Windows header shim cannot coexist with Objective-C headers; see docs/adr/0009"
#endif

#include <basetsd.h>
#include <guiddef.h>
#include <windef.h>
#include <winerror.h>
