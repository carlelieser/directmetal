#pragma once

#include <windows.h>

// Source markers read by tools/api/report_status.py; see docs/api/README.md.
namespace directmetal::frontends {

// Logs an unimplemented entry point on the unix side, or on the PE side with the bridge status
// when the unix side is unreachable.
void report_stub(const char* key) noexcept;

// One per marker site; its static initialization reports the site once per process.
class StubSite {
public:
    explicit StubSite(const char* key) noexcept { report_stub(key); }
};

} // namespace directmetal::frontends

#define DM_STUB(key)                                                                               \
    do {                                                                                           \
        static const ::directmetal::frontends::StubSite dm_stub_site{key};                         \
        return E_NOTIMPL;                                                                          \
    } while (0)
