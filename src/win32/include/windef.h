#pragma once

// Base Windows data types, handles and calling conventions for native macOS builds (ADR 0010).
// Sizes follow Windows, not the LP64 host: LONG and ULONG are 32-bit, WCHAR is a 16-bit char16_t.
// https://learn.microsoft.com/windows/win32/winprog/windows-data-types

#include <basetsd.h>

#include <type_traits>

// Native builds have one calling convention.
#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define STDMETHODCALLTYPE
#define STDMETHODVCALLTYPE
#define STDAPICALLTYPE
#define STDAPIVCALLTYPE

#define CONST const
#define VOID void
#define FAR
#define NEAR

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define EXTERN_C extern "C"
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#define DECLSPEC_SELECTANY __attribute__((weak))
#define DECLSPEC_NOVTABLE
#define DECLSPEC_UUID(uuid)
#define DECLSPEC_NOTHROW
#define DECLSPEC_IMPORT
#define DECLSPEC_EXPORT __attribute__((visibility("default")))
#define FORCEINLINE inline __attribute__((always_inline))

typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;

typedef char CHAR;
typedef unsigned char UCHAR;
typedef std::uint8_t BYTE;
typedef std::uint8_t BOOLEAN;
typedef std::int16_t SHORT;
typedef std::uint16_t USHORT;
typedef std::uint16_t WORD;
typedef int INT;
typedef unsigned int UINT;
typedef std::int32_t LONG;
typedef std::uint32_t ULONG;
typedef std::uint32_t DWORD;
typedef std::int32_t BOOL;
typedef std::int64_t LONGLONG;
typedef std::uint64_t ULONGLONG;
typedef std::uint64_t DWORDLONG;
typedef float FLOAT;
typedef double DOUBLE;
typedef char16_t WCHAR;
typedef WCHAR OLECHAR;
typedef DWORD LCID;
typedef DWORD COLORREF;

typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef UCHAR* PUCHAR;
typedef WORD* PWORD;
typedef WORD* LPWORD;
typedef INT* PINT;
typedef INT* LPINT;
typedef UINT* PUINT;
typedef LONG* PLONG;
typedef LONG* LPLONG;
typedef ULONG* PULONG;
typedef DWORD* PDWORD;
typedef DWORD* LPDWORD;
typedef BOOL* PBOOL;
typedef BOOL* LPBOOL;
typedef FLOAT* PFLOAT;
typedef CHAR* PCHAR;
typedef CHAR* LPSTR;
typedef CHAR* PSTR;
typedef const CHAR* LPCSTR;
typedef const CHAR* PCSTR;
typedef WCHAR* PWCHAR;
typedef WCHAR* LPWSTR;
typedef WCHAR* PWSTR;
typedef const WCHAR* LPCWSTR;
typedef const WCHAR* PCWSTR;
typedef OLECHAR* LPOLESTR;
typedef const OLECHAR* LPCOLESTR;
typedef OLECHAR* BSTR;

typedef LONG HRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;

// Each handle is a distinct pointer type, as with STRICT on Windows.
#define DECLARE_HANDLE(name)                                                                       \
    struct name##__ {                                                                              \
        int unused;                                                                                \
    };                                                                                             \
    typedef struct name##__* name

DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HGLRC);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;

typedef struct _LUID {
    DWORD LowPart;
    LONG HighPart;
} LUID, *PLUID;

// Only the named member `u` of the SDK's anonymous struct is provided: anonymous structs are an
// extension in ISO C++.
typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

// Bitwise operators for a flags enum.
// https://learn.microsoft.com/windows/win32/api/winnt/nf-winnt-define_enum_flag_operators
#define DIRECTMETAL_ENUM_BITS(type, value) static_cast<::std::underlying_type_t<type>>(value)
#define DEFINE_ENUM_FLAG_OPERATORS(type)                                                           \
    extern "C++" {                                                                                 \
    constexpr type operator|(type left, type right) noexcept {                                     \
        return type(DIRECTMETAL_ENUM_BITS(type, left) | DIRECTMETAL_ENUM_BITS(type, right));       \
    }                                                                                              \
    constexpr type operator&(type left, type right) noexcept {                                     \
        return type(DIRECTMETAL_ENUM_BITS(type, left) & DIRECTMETAL_ENUM_BITS(type, right));       \
    }                                                                                              \
    constexpr type operator^(type left, type right) noexcept {                                     \
        return type(DIRECTMETAL_ENUM_BITS(type, left) ^ DIRECTMETAL_ENUM_BITS(type, right));       \
    }                                                                                              \
    constexpr type operator~(type value) noexcept {                                                \
        return type(~DIRECTMETAL_ENUM_BITS(type, value));                                          \
    }                                                                                              \
    inline type& operator|=(type& left, type right) noexcept {                                     \
        return left = left | right;                                                                \
    }                                                                                              \
    inline type& operator&=(type& left, type right) noexcept {                                     \
        return left = left & right;                                                                \
    }                                                                                              \
    inline type& operator^=(type& left, type right) noexcept {                                     \
        return left = left ^ right;                                                                \
    }                                                                                              \
    }

// Spellings the pinned mingw-w64 Direct3D headers use. Nameless members are unions only, which are
// standard C++.
typedef BOOL WINBOOL;
#define __C89_NAMELESS
#define __C89_NAMELESSUNIONNAME
#define __C89_NAMELESSUNIONNAME1
#define __C89_NAMELESSUNIONNAME2
#define __C89_NAMELESSUNIONNAME3
#define __C89_NAMELESSUNIONNAME4
#define __C89_NAMELESSUNIONNAME5

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                             \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) |                 \
     ((DWORD)(BYTE)(ch3) << 24))

static_assert(sizeof(LONG) == 4 && sizeof(ULONG) == 4 && sizeof(DWORD) == 4);
static_assert(sizeof(BOOL) == 4 && sizeof(HRESULT) == 4 && sizeof(UINT) == 4);
static_assert(std::is_same_v<WCHAR, char16_t> && sizeof(WCHAR) == 2);
static_assert(sizeof(LUID) == 8 && sizeof(LARGE_INTEGER) == 8);
static_assert(sizeof(RECT) == 16);
