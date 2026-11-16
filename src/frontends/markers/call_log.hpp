#pragma once

namespace directmetal::frontends {

// Appends `key` to this process's call log when DIRECTMETAL_CALL_LOG_DIR is set.
void append_to_call_log(const char* key) noexcept;

} // namespace directmetal::frontends
