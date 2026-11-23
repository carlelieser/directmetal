#include "test_objects.hpp"

#include <directmetal/com/ref.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace {

using directmetal::com::PrivateRef;
using directmetal::com::Ref;

TEST(LifetimeTest, NewObjectHasOnePublicReferenceAndNoHookCalls) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);

    EXPECT_EQ(object->AddRef(), 2u);
    EXPECT_EQ(object->Release(), 1u);
    EXPECT_EQ(lifetime.first_public_refs, 0);
    EXPECT_EQ(lifetime.last_public_releases, 0);
    EXPECT_EQ(object->Release(), 0u);
}

TEST(LifetimeTest, LastPublicReleaseRunsHookThenDestroys) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->AddRef();

    EXPECT_EQ(object->Release(), 1u);
    EXPECT_EQ(lifetime.last_public_releases, 0);
    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.last_public_releases, 1);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, PrivateReferenceOutlivesPublicReferences) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->add_private_ref();

    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.last_public_releases, 1);
    EXPECT_EQ(lifetime.destructions, 0);
    object->release_private();
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, PublicReferencesOutlivePrivateReferences) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->add_private_ref();

    object->release_private();
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, PublicReferencesTogetherHoldOnePrivateReference) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->AddRef();
    object->AddRef();
    object->add_private_ref();

    object->Release();
    object->Release();
    object->release_private();
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, PublicReferenceAfterZeroRunsFirstPublicRefHook) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    object->add_private_ref();
    object->Release();

    EXPECT_EQ(object->AddRef(), 1u);
    EXPECT_EQ(lifetime.first_public_refs, 1);
    EXPECT_EQ(object->AddRef(), 2u);
    EXPECT_EQ(lifetime.first_public_refs, 1);
    object->Release();
    object->release_private();
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(object->Release(), 0u);
    EXPECT_EQ(lifetime.last_public_releases, 2);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, RefAdoptsRetainsAndReleases) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    {
        Ref<ITestDerived> adopted = Ref<ITestDerived>::adopt(object);
        Ref<ITestDerived> retained = Ref<ITestDerived>::retain(object);
        Ref<ITestDerived> copied = retained;
        Ref<ITestDerived> moved = std::move(copied);

        EXPECT_FALSE(copied);
        EXPECT_TRUE(moved);
        EXPECT_EQ(moved.get(), object);
        EXPECT_EQ(moved->derived_value(), 2);
        EXPECT_EQ(object->AddRef(), 4u);
        object->Release();
    }
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, RefAssignmentReleasesThePreviousObject) {
    Lifetime first_lifetime;
    Lifetime second_lifetime;
    Ref<ITestDerived> ref = Ref<ITestDerived>::adopt(new TestObject(first_lifetime));

    ref = Ref<ITestDerived>::adopt(new TestObject(second_lifetime));
    EXPECT_EQ(first_lifetime.destructions, 1);
    EXPECT_EQ(second_lifetime.destructions, 0);
    ref = Ref<ITestDerived>();
    EXPECT_EQ(second_lifetime.destructions, 1);
}

TEST(LifetimeTest, RefDetachKeepsTheReference) {
    Lifetime lifetime;
    Ref<ITestDerived> ref = Ref<ITestDerived>::adopt(new TestObject(lifetime));

    ITestDerived* raw = ref.detach();
    EXPECT_FALSE(ref);
    EXPECT_EQ(lifetime.destructions, 0);
    EXPECT_EQ(raw->Release(), 0u);
    EXPECT_EQ(lifetime.destructions, 1);
}

TEST(LifetimeTest, PrivateRefKeepsObjectAliveWithoutPublicReferences) {
    Lifetime lifetime;
    auto* object = new TestObject(lifetime);
    {
        PrivateRef<TestObject> owner(object);
        PrivateRef<TestObject> copied = owner;
        PrivateRef<TestObject> moved = std::move(copied);

        EXPECT_FALSE(copied);
        EXPECT_EQ(object->Release(), 0u);
        EXPECT_EQ(moved->extra_value(), 3);
        owner = PrivateRef<TestObject>();
        EXPECT_EQ(lifetime.destructions, 0);
        EXPECT_EQ(moved.get(), object);
    }
    EXPECT_EQ(lifetime.destructions, 1);
}

} // namespace
