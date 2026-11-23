#include "d3d9_table.hpp"

#include <directmetal/formats/formats.hpp>

#include <algorithm>

namespace directmetal::formats {

const D3d9FormatInfo* find_d3d9(std::uint32_t d3dformat) noexcept {
    const auto row = std::lower_bound(tables::d3d9_rows.begin(), tables::d3d9_rows.end(), d3dformat,
                                      [](const D3d9FormatInfo& candidate, std::uint32_t value) {
                                          return candidate.value < value;
                                      });
    const bool is_found = row != tables::d3d9_rows.end() && row->value == d3dformat;
    return is_found ? &*row : nullptr;
}

std::span<const D3d9FormatInfo> all_d3d9_formats() noexcept {
    return tables::d3d9_rows;
}

} // namespace directmetal::formats
