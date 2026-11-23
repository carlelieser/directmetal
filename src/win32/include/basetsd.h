#pragma once

// Fixed-size and pointer-size integer types of the Windows SDK, with Windows sizes on macOS.
// https://learn.microsoft.com/windows/win32/winprog/windows-data-types

#include <cstddef>
#include <cstdint>

typedef std::int8_t INT8;
typedef std::int16_t INT16;
typedef std::int32_t INT32;
typedef std::int64_t INT64;
typedef std::uint8_t UINT8;
typedef std::uint16_t UINT16;
typedef std::uint32_t UINT32;
typedef std::uint64_t UINT64;
typedef std::int32_t LONG32;
typedef std::uint32_t ULONG32;
typedef std::int64_t LONG64;
typedef std::uint64_t ULONG64;
typedef std::uint32_t DWORD32;
typedef std::uint64_t DWORD64;

typedef std::intptr_t INT_PTR;
typedef std::uintptr_t UINT_PTR;
typedef std::intptr_t LONG_PTR;
typedef std::uintptr_t ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;
typedef ULONG_PTR SIZE_T;
typedef LONG_PTR SSIZE_T;
typedef INT_PTR* PINT_PTR;
typedef UINT_PTR* PUINT_PTR;
typedef LONG_PTR* PLONG_PTR;
typedef ULONG_PTR* PULONG_PTR;
typedef SIZE_T* PSIZE_T;

static_assert(sizeof(INT_PTR) == sizeof(void*));
static_assert(sizeof(SIZE_T) == sizeof(std::size_t));
