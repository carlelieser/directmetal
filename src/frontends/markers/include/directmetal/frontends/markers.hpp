#pragma once

#include <windows.h>

// Source markers read by tools/api/report_status.py; see docs/api/README.md.
namespace directmetal::frontends {

// Logs an unimplemented entry point on the unix side. Returns whether the bridge accepted it.
bool report_stub(const char* key) noexcept;

} // namespace directmetal::frontends

#define DM_STUB(key)                                                                               \
    do {                                                                                           \
        [[maybe_unused]] static const bool dm_is_stub_reported =                                   \
            ::directmetal::frontends::report_stub(key);                                            \
        return E_NOTIMPL;                                                                          \
    } while (0)
