#pragma once

#include "d3d9_color_formats.hpp"
#include "d3d9_other_formats.hpp"

#include <directmetal/formats/formats.hpp>

#include <algorithm>
#include <array>
#include <iterator>

// Every D3DFORMAT row, sorted by value.
namespace directmetal::formats::tables {

inline constexpr auto d3d9_rows = [] {
    std::array<D3d9FormatInfo, std::size(d3d9_color_rows) + std::size(d3d9_other_rows)> rows{};
    const auto output =
        std::copy(std::begin(d3d9_color_rows), std::end(d3d9_color_rows), rows.begin());
    std::copy(std::begin(d3d9_other_rows), std::end(d3d9_other_rows), output);
    std::sort(rows.begin(), rows.end(),
              [](const D3d9FormatInfo& left, const D3d9FormatInfo& right) {
                  return left.value < right.value;
              });
    return rows;
}();

constexpr bool has_unique_d3d9_values() {
    return std::adjacent_find(d3d9_rows.begin(), d3d9_rows.end(),
                              [](const D3d9FormatInfo& left, const D3d9FormatInfo& right) {
                                  return left.value == right.value;
                              }) == d3d9_rows.end();
}

static_assert(has_unique_d3d9_values(), "D3DFORMAT table rows must have distinct values");

} // namespace directmetal::formats::tables
