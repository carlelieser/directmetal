#pragma once

// IUnknown, the root COM interface.
// https://learn.microsoft.com/windows/win32/api/unknwn/nn-unknwn-iunknown

#include <rpcndr.h>

#include <objbase.h>

extern "C++" {

struct IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;

    template <typename Q> HRESULT STDMETHODCALLTYPE QueryInterface(Q** object) {
        return QueryInterface(__uuidof(Q), reinterpret_cast<void**>(object));
    }
};

__CRT_UUID_DECL(IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x46)

} // extern "C++"

typedef IUnknown* LPUNKNOWN;

DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x46);
