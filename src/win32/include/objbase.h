#pragma once

// COM interface declaration macros.
// https://learn.microsoft.com/windows/win32/api/combaseapi/

#include <guiddef.h>
#include <winerror.h>

#define interface struct

#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define STDAPI EXTERN_C HRESULT STDAPICALLTYPE
#define STDAPI_(type) EXTERN_C type STDAPICALLTYPE
#define PURE = 0
#define THIS_
#define THIS void
#define DECLARE_INTERFACE(name) interface name
#define DECLARE_INTERFACE_(name, base) interface name : public base

// https://learn.microsoft.com/windows/win32/api/combaseapi/nf-combaseapi-iid_ppv_args
template <typename T> void** IID_PPV_ARGS_Helper(T** pp) {
    return reinterpret_cast<void**>(pp);
}

#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)

#include <unknwn.h>
