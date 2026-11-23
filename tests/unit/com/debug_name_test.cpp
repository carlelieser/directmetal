#include "counting_unknown.hpp"

#include <directmetal/com/private_data.hpp>

#include <gtest/gtest.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using directmetal::com::PrivateDataStore;

// https://learn.microsoft.com/windows/win32/direct3d11/using-the-debug-layer-to-test-apps
constexpr GUID debug_name_guid = {
    0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};
constexpr GUID debug_name_wide_guid = {
    0x4cca5fd8, 0x921f, 0x42c8, {0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41}};
constexpr GUID other_guid = {
    0x1f0e2d3c, 0x4b5a, 0x6978, {0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf1}};

void set_narrow(PrivateDataStore& store, const std::string& name) {
    store.set(debug_name_guid, UINT(name.size()), name.data());
}

void set_wide(PrivateDataStore& store, const std::u16string& name) {
    store.set(debug_name_wide_guid, UINT(name.size() * sizeof(char16_t)), name.data());
}

class DebugNameTest : public testing::Test {
protected:
    void SetUp() override {
        store.on_debug_name_changed([this](std::string_view name) { names.emplace_back(name); });
    }

    PrivateDataStore store;
    std::vector<std::string> names;
};

TEST_F(DebugNameTest, NarrowNameStopsAtNul) {
    store.set(debug_name_guid, 6, "tex\0ab");

    EXPECT_EQ(store.debug_name(), "tex");
}

TEST_F(DebugNameTest, NarrowNameKeepsValidUtf8) {
    set_narrow(store, "T\xc3\xa9x\xf0\x9f\x98\x80");

    EXPECT_EQ(store.debug_name(), "T\xc3\xa9x\xf0\x9f\x98\x80");
}

TEST_F(DebugNameTest, NarrowNameReplacesInvalidUtf8BeforeTheCallback) {
    // A stray byte, an overlong '/', an encoded surrogate and a truncated sequence.
    set_narrow(store, "a\xff"
                      "b\xc0\xaf"
                      "c\xed\xa0\x80"
                      "d\xe2\x82");

    const std::string replacement = "\xef\xbf\xbd";
    const std::string expected = "a" + replacement + "b" + replacement + replacement + "c" +
                                 replacement + replacement + replacement + "d" + replacement +
                                 replacement;
    EXPECT_EQ(store.debug_name(), expected);
    EXPECT_EQ(names.back(), expected);
}

TEST_F(DebugNameTest, WideNameConvertsToUtf8) {
    set_wide(store, u"Téx\U0001F600");

    EXPECT_EQ(store.debug_name(), "T\xc3\xa9x\xf0\x9f\x98\x80");
}

TEST_F(DebugNameTest, UnpairedSurrogateBecomesReplacementCharacter) {
    set_wide(store, std::u16string{u'a', char16_t(0xd800), u'b'});

    EXPECT_EQ(store.debug_name(), "a\xef\xbf\xbd"
                                  "b");
}

TEST_F(DebugNameTest, LastSetNameWinsAndRemovalFallsBack) {
    set_narrow(store, "narrow");
    set_wide(store, u"wide");
    EXPECT_EQ(store.debug_name(), "wide");
    set_narrow(store, "again");
    EXPECT_EQ(store.debug_name(), "again");

    store.set(debug_name_guid, 0, nullptr);
    EXPECT_EQ(store.debug_name(), "wide");
    store.set(debug_name_wide_guid, 0, nullptr);
    EXPECT_EQ(store.debug_name(), "");
}

TEST_F(DebugNameTest, CallbackReceivesEveryNameChange) {
    set_narrow(store, "first");
    set_wide(store, u"second");
    store.set(debug_name_wide_guid, 0, nullptr);

    EXPECT_EQ(names, (std::vector<std::string>{"first", "second", "first"}));
}

TEST_F(DebugNameTest, CallbackIgnoresOtherGuids) {
    store.set(other_guid, 4, "data");

    EXPECT_TRUE(names.empty());
}

// A callback running under the store's lock would deadlock here.
TEST_F(DebugNameTest, CallbackRunsOutsideTheLock) {
    std::string seen;
    store.on_debug_name_changed([&](std::string_view) { seen = store.debug_name(); });

    set_narrow(store, "label");
    EXPECT_EQ(seen, "label");
}

// The interface replaced by "stale" is released between that set's commit and its callback; a
// newer name set and delivered meanwhile must stay the last one delivered.
TEST_F(DebugNameTest, StaleNameIsNotDeliveredAfterANewerOne) {
    CountingUnknown blocker;
    store.set_interface(debug_name_guid, &blocker);
    blocker.on_release = [&] {
        blocker.on_release = nullptr;
        std::thread([&] { set_narrow(store, "newer"); }).join();
    };

    set_narrow(store, "stale");
    EXPECT_EQ(store.debug_name(), "newer");
    EXPECT_EQ(names.back(), "newer");
}

TEST(DebugNameConcurrencyTest, LastDeliveredNameMatchesTheStore) {
    PrivateDataStore store;
    std::mutex delivered_mutex;
    std::string delivered;
    store.on_debug_name_changed([&](std::string_view name) {
        const std::scoped_lock lock(delivered_mutex);
        delivered = name;
    });
    std::vector<std::thread> threads;
    for (int thread_index = 0; thread_index < 8; ++thread_index) {
        threads.emplace_back([&store, thread_index] {
            for (int index = 0; index < 2000; ++index) {
                set_narrow(store, std::to_string(thread_index) + "-" + std::to_string(index));
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(delivered, store.debug_name());
}

} // namespace
