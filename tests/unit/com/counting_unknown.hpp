#pragma once

#include <unknwn.h>

#include <functional>

// Counts references; on_add_ref and on_release run inside every AddRef and Release.
class CountingUnknown final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** object) override {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        if (on_add_ref) {
            on_add_ref();
        }
        return ++count;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        if (on_release) {
            on_release();
        }
        return --count;
    }

    ULONG count = 1;
    std::function<void()> on_add_ref;
    std::function<void()> on_release;
};
