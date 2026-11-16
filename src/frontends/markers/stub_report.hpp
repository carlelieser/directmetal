#pragma once

namespace directmetal::frontends {

// Logs a warning that the unimplemented entry point `key` was called.
void report_stub(const char* key) noexcept;

} // namespace directmetal::frontends
