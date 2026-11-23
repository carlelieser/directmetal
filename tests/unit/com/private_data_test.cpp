#include "counting_unknown.hpp"

#include <directmetal/com/private_data.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {

using directmetal::com::PrivateDataResult;
using directmetal::com::PrivateDataStore;

constexpr GUID data_guid = {
    0x1f0e2d3c, 0x4b5a, 0x6978, {0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf0}};
constexpr GUID other_guid = {
    0x1f0e2d3c, 0x4b5a, 0x6978, {0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf1}};

std::string read_string(const PrivateDataStore& store, REFGUID guid) {
    std::array<char, 64> buffer{};
    UINT size = UINT(buffer.size());
    EXPECT_EQ(store.get(guid, &size, buffer.data()), PrivateDataResult::ok);
    return std::string(buffer.data(), size);
}

TEST(PrivateDataTest, GetReturnsStoredBytesAndSize) {
    PrivateDataStore store;
    ASSERT_EQ(store.set(data_guid, 5, "hello"), PrivateDataResult::ok);

    EXPECT_EQ(read_string(store, data_guid), "hello");
}

TEST(PrivateDataTest, GetWithNullDataReportsSize) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    UINT size = 0;

    EXPECT_EQ(store.get(data_guid, &size, nullptr), PrivateDataResult::ok);
    EXPECT_EQ(size, 5u);
}

TEST(PrivateDataTest, GetWithSmallBufferIsMoreDataWithRequiredSize) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    std::array<char, 4> buffer = {'x', 'x', 'x', 'x'};
    UINT size = UINT(buffer.size());

    EXPECT_EQ(store.get(data_guid, &size, buffer.data()), PrivateDataResult::more_data);
    EXPECT_EQ(size, 5u);
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "xxxx");
}

TEST(PrivateDataTest, GetUnknownGuidIsNotFoundWithZeroSize) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    UINT size = 7;

    EXPECT_EQ(store.get(other_guid, &size, nullptr), PrivateDataResult::not_found);
    EXPECT_EQ(size, 0u);
}

TEST(PrivateDataTest, GetWithNullSizeIsInvalidArgument) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    char buffer[8];

    EXPECT_EQ(store.get(data_guid, nullptr, buffer), PrivateDataResult::invalid_argument);
}

TEST(PrivateDataTest, SetWithSizeAndNullDataIsInvalidArgumentAndKeepsEntry) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");

    EXPECT_EQ(store.set(data_guid, 3, nullptr), PrivateDataResult::invalid_argument);
    EXPECT_EQ(read_string(store, data_guid), "hello");
}

TEST(PrivateDataTest, SetWithZeroSizeRemovesEntry) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    UINT size = 0;

    EXPECT_EQ(store.set(data_guid, 0, nullptr), PrivateDataResult::ok);
    EXPECT_EQ(store.get(data_guid, &size, nullptr), PrivateDataResult::not_found);
}

TEST(PrivateDataTest, SetOverwritesAndKeepsOtherEntries) {
    PrivateDataStore store;
    store.set(data_guid, 5, "hello");
    store.set(other_guid, 5, "other");

    store.set(data_guid, 3, "bye");
    EXPECT_EQ(read_string(store, data_guid), "bye");
    EXPECT_EQ(read_string(store, other_guid), "other");
}

TEST(PrivateDataTest, SetInterfaceHoldsReferenceAndGetAddsOne) {
    CountingUnknown object;
    {
        PrivateDataStore store;
        ASSERT_EQ(store.set_interface(data_guid, &object), PrivateDataResult::ok);
        EXPECT_EQ(object.count, 2u);
        IUnknown* returned = nullptr;
        UINT size = sizeof(returned);

        EXPECT_EQ(store.get(data_guid, &size, &returned), PrivateDataResult::ok);
        EXPECT_EQ(size, sizeof(IUnknown*));
        EXPECT_EQ(returned, &object);
        EXPECT_EQ(object.count, 3u);
        returned->Release();
    }
    EXPECT_EQ(object.count, 1u);
}

TEST(PrivateDataTest, OverwritingOrRemovingInterfaceReleasesIt) {
    CountingUnknown first;
    CountingUnknown second;
    PrivateDataStore store;
    store.set_interface(data_guid, &first);

    store.set_interface(data_guid, &second);
    EXPECT_EQ(first.count, 1u);
    store.set(data_guid, 5, "bytes");
    EXPECT_EQ(second.count, 1u);
    store.set_interface(other_guid, &first);
    EXPECT_EQ(store.set_interface(other_guid, nullptr), PrivateDataResult::ok);
    EXPECT_EQ(first.count, 1u);
    UINT size = 0;
    EXPECT_EQ(store.get(other_guid, &size, nullptr), PrivateDataResult::not_found);
}

TEST(PrivateDataTest, ReplacedInterfaceIsReleasedOutsideTheLock) {
    CountingUnknown object;
    PrivateDataStore store;
    store.set_interface(data_guid, &object);
    UINT size_in_release = 1;
    object.on_release = [&] { store.get(other_guid, &size_in_release, nullptr); };

    store.set(data_guid, 0, nullptr);
    EXPECT_EQ(size_in_release, 0u);
    object.on_release = nullptr;
}

TEST(PrivateDataTest, StoredInterfaceIsReferencedOutsideTheLock) {
    CountingUnknown object;
    PrivateDataStore store;
    UINT size_in_add_ref = 1;
    object.on_add_ref = [&] { store.get(other_guid, &size_in_add_ref, nullptr); };

    store.set_interface(data_guid, &object);
    object.on_add_ref = nullptr;
    EXPECT_EQ(size_in_add_ref, 0u);
    EXPECT_EQ(object.count, 2u);
}

} // namespace
