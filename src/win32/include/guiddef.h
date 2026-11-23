#pragma once

// GUIDs, interface IDs and __uuidof for native macOS builds.
// https://learn.microsoft.com/windows/win32/api/guiddef/ns-guiddef-guid

#include <windef.h>

#include <cstring>
#include <type_traits>

typedef struct _GUID {
    ULONG Data1;
    USHORT Data2;
    USHORT Data3;
    UCHAR Data4[8];
} GUID;

typedef GUID IID;
typedef GUID CLSID;
typedef GUID FMTID;
typedef GUID* LPGUID;
typedef const GUID* LPCGUID;
typedef IID* LPIID;
typedef CLSID* LPCLSID;
typedef const GUID& REFGUID;
typedef const IID& REFIID;
typedef const CLSID& REFCLSID;
typedef const FMTID& REFFMTID;

static_assert(sizeof(GUID) == 16);
static_assert(std::is_trivially_copyable_v<GUID>);

inline bool IsEqualGUID(REFGUID left, REFGUID right) noexcept {
    return std::memcmp(&left, &right, sizeof(GUID)) == 0;
}

inline bool operator==(REFGUID left, REFGUID right) noexcept {
    return IsEqualGUID(left, right);
}

#define IsEqualIID(left, right) IsEqualGUID(left, right)
#define IsEqualCLSID(left, right) IsEqualGUID(left, right)

// Without Microsoft extensions there is no __declspec(uuid). Headers attach an interface's GUID
// with __CRT_UUID_DECL, which specializes InterfaceGuid; __uuidof accepts a type or an expression.
namespace directmetal::win32 {

template <typename Interface> struct InterfaceGuid;

template <typename Interface> constexpr const GUID& uuid_of() noexcept {
    return InterfaceGuid<Interface>::value;
}

} // namespace directmetal::win32

#define __uuidof(type_or_expression)                                                               \
    ::directmetal::win32::uuid_of<::std::remove_cv_t<__typeof__(type_or_expression)>>()

#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                           \
    extern "C++" {                                                                                 \
    template <> struct directmetal::win32::InterfaceGuid<type> {                                   \
        static constexpr GUID value = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}};               \
    };                                                                                             \
    }

// One definition per program: an inline variable, so INITGUID is not needed.
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                               \
    extern "C++" inline constexpr GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
