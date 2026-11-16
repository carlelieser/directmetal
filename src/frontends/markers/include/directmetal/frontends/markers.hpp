#pragma once

#include <atomic>
#include <cstdint>

// Source markers read by tools/api/report_status.py; recording follows docs/api/README.md.
namespace directmetal::frontends {

enum class MarkerKind : std::uint8_t { implemented, stub };

using MarkerSite = std::atomic<std::uint8_t>;

// Records `key` and, for stubs, logs a warning naming it.
void record_call(const char* key, MarkerKind kind) noexcept;

// True for exactly one call per site per process.
inline bool claim_site(MarkerSite& site) noexcept {
    if (site.load(std::memory_order_relaxed) != 0) {
        return false;
    }
    std::uint8_t expected = 0;
    return site.compare_exchange_strong(expected, 1, std::memory_order_relaxed);
}

} // namespace directmetal::frontends

#define DIRECTMETAL_RECORD_SITE(key, kind)                                                         \
    do {                                                                                           \
        static ::directmetal::frontends::MarkerSite directmetal_marker_site{0};                    \
        if (::directmetal::frontends::claim_site(directmetal_marker_site)) {                       \
            ::directmetal::frontends::record_call(key,                                             \
                                                  ::directmetal::frontends::MarkerKind::kind);     \
        }                                                                                          \
    } while (0)

#define DM_IMPL(key) DIRECTMETAL_RECORD_SITE(key, implemented)

#define DM_STUB_RETURN(key, value)                                                                 \
    do {                                                                                           \
        DIRECTMETAL_RECORD_SITE(key, stub);                                                        \
        return value;                                                                              \
    } while (0)

#define DM_STUB(key) DM_STUB_RETURN(key, E_NOTIMPL)

#define DM_STUB_VOID(key)                                                                          \
    do {                                                                                           \
        DIRECTMETAL_RECORD_SITE(key, stub);                                                        \
        return;                                                                                    \
    } while (0)
