#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace directmetal::com {

// UTF-8 from little-endian UTF-16 code units, up to the first NUL. Unpaired surrogates become
// U+FFFD; a trailing odd byte is ignored.
std::string utf8_from_utf16(std::span<const std::byte> bytes);

} // namespace directmetal::com
