#pragma once

#include <directmetal/export.hpp>

#include <string_view>

namespace directmetal {

DIRECTMETAL_UNIX_API std::string_view version() noexcept;

} // namespace directmetal
