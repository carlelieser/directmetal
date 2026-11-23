#include "test_objects.hpp"

#include <directmetal/com/ref.hpp>

#include <gtest/gtest.h>

namespace {

using directmetal::com::Ref;

class QueryInterfaceTest : public testing::Test {
protected:
    Lifetime lifetime;
    TestObject* object = new TestObject(lifetime);
    Ref<ITestDerived> derived = Ref<ITestDerived>::adopt(object);
};

template <typename Interface> Interface* query(IUnknown* source, HRESULT expected = S_OK) {
    void* result = reinterpret_cast<void*>(0x1);
    EXPECT_EQ(source->QueryInterface(__uuidof(Interface), &result), expected);
    return static_cast<Interface*>(result);
}

TEST_F(QueryInterfaceTest, AnswersEveryListedInterfaceIncludingAncestors) {
    ITestBase* base = query<ITestBase>(derived.get());
    ITestDerived* same = query<ITestDerived>(derived.get());
    ITestExtra* extra = query<ITestExtra>(derived.get());

    EXPECT_EQ(base->base_value(), 1);
    EXPECT_EQ(same, derived.get());
    EXPECT_EQ(extra->extra_value(), 3);
    EXPECT_EQ(base->Release(), 3u);
    EXPECT_EQ(same->Release(), 2u);
    EXPECT_EQ(extra->Release(), 1u);
}

TEST_F(QueryInterfaceTest, ReturnsOneIUnknownFromEveryInterface) {
    ITestExtra* extra = query<ITestExtra>(derived.get());
    IUnknown* from_derived = query<IUnknown>(derived.get());
    IUnknown* from_extra = query<IUnknown>(extra);

    EXPECT_EQ(from_derived, from_extra);
    EXPECT_EQ(from_derived, static_cast<IUnknown*>(derived.get()));
    from_extra->Release();
    from_derived->Release();
    extra->Release();
}

TEST_F(QueryInterfaceTest, ExtraInterfaceAnswersBaseInterfaces) {
    ITestExtra* extra = query<ITestExtra>(derived.get());
    ITestDerived* back = query<ITestDerived>(extra);

    EXPECT_EQ(back, derived.get());
    back->Release();
    extra->Release();
}

TEST_F(QueryInterfaceTest, UnlistedInterfaceIsNoInterfaceAndClearsOutput) {
    EXPECT_EQ(query<ITestUnlisted>(derived.get(), E_NOINTERFACE), nullptr);
    EXPECT_EQ(derived->AddRef(), 2u);
    EXPECT_EQ(derived->Release(), 1u);
}

TEST_F(QueryInterfaceTest, NullOutputIsPointerError) {
    EXPECT_EQ(derived->QueryInterface(__uuidof(ITestBase), nullptr), E_POINTER);
    EXPECT_EQ(derived->AddRef(), 2u);
    EXPECT_EQ(derived->Release(), 1u);
}

TEST_F(QueryInterfaceTest, SuccessAddsOnePublicReference) {
    ITestBase* base = query<ITestBase>(derived.get());

    EXPECT_EQ(derived->AddRef(), 3u);
    EXPECT_EQ(derived->Release(), 2u);
    base->Release();
}

TEST_F(QueryInterfaceTest, TemplateOverloadDeducesTheIid) {
    ITestExtra* extra = nullptr;

    EXPECT_EQ(derived->QueryInterface(&extra), S_OK);
    EXPECT_EQ(extra->extra_value(), 3);
    extra->Release();
}

} // namespace
