#pragma once

#include <directmetal/com/object.hpp>

#include <unknwn.h>

#include <atomic>

// Interfaces and an object for exercising the COM base through IUnknown.
struct ITestBase : public IUnknown {
    virtual int STDMETHODCALLTYPE base_value() = 0;
};

struct ITestDerived : public ITestBase {
    virtual int STDMETHODCALLTYPE derived_value() = 0;
};

struct ITestExtra : public IUnknown {
    virtual int STDMETHODCALLTYPE extra_value() = 0;
};

struct ITestUnlisted : public IUnknown {};

__CRT_UUID_DECL(ITestBase, 0x6b1d2f10, 0x3c4e, 0x4a51, 0x9e, 0x0c, 0x51, 0x7a, 0x2b, 0x01, 0x00,
                0x01)
__CRT_UUID_DECL(ITestDerived, 0x6b1d2f10, 0x3c4e, 0x4a51, 0x9e, 0x0c, 0x51, 0x7a, 0x2b, 0x01, 0x00,
                0x02)
__CRT_UUID_DECL(ITestExtra, 0x6b1d2f10, 0x3c4e, 0x4a51, 0x9e, 0x0c, 0x51, 0x7a, 0x2b, 0x01, 0x00,
                0x03)
__CRT_UUID_DECL(ITestUnlisted, 0x6b1d2f10, 0x3c4e, 0x4a51, 0x9e, 0x0c, 0x51, 0x7a, 0x2b, 0x01, 0x00,
                0x04)

// What a TestObject's hooks and destructor observed.
struct Lifetime {
    std::atomic<int> first_public_refs{0};
    std::atomic<int> last_public_releases{0};
    std::atomic<int> destructions{0};
};

class TestObject final : public directmetal::com::Object<ITestDerived, ITestExtra> {
public:
    explicit TestObject(Lifetime& lifetime) noexcept : lifetime_(lifetime) {}

    int STDMETHODCALLTYPE base_value() override { return 1; }
    int STDMETHODCALLTYPE derived_value() override { return 2; }
    int STDMETHODCALLTYPE extra_value() override { return 3; }

protected:
    ~TestObject() override { lifetime_.destructions.fetch_add(1); }

    void* find_interface(REFIID iid) noexcept override {
        return directmetal::com::find_in_table(
            directmetal::com::interface_table<TestObject, IUnknown, ITestBase, ITestDerived,
                                              ITestExtra>(),
            iid, this);
    }

    void on_first_public_ref() noexcept override { lifetime_.first_public_refs.fetch_add(1); }

    void on_last_public_release() noexcept override {
        // The object is still alive here: destruction comes after the hook.
        if (lifetime_.destructions.load() == 0) {
            lifetime_.last_public_releases.fetch_add(1);
        }
    }

private:
    Lifetime& lifetime_;
};
