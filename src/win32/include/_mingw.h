#pragma once

// Every mingw-w64 base header includes this one; the pinned Direct3D and DXGI headers do not.
// Reaching it means a base header was taken from the pinned tree instead of this shim.
#error "a mingw-w64 base header was included; declare what it provided in src/win32/include"
