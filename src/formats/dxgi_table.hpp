#pragma once

#include "dxgi_compressed_formats.hpp"
#include "dxgi_depth_formats.hpp"
#include "dxgi_packed_formats.hpp"
#include "dxgi_row.hpp"
#include "dxgi_small_formats.hpp"
#include "dxgi_video_formats.hpp"
#include "dxgi_wide_formats.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

// Every DXGI_FORMAT row, sorted by value, and the FormatInfo array all_dxgi_formats() returns.
namespace directmetal::formats::tables {

inline constexpr std::span<const DxgiRow> dxgi_row_groups[] = {
    wide_format_rows,  depth_format_rows,      packed_format_rows,
    small_format_rows, compressed_format_rows, video_format_rows,
};

constexpr std::size_t dxgi_row_count() {
    std::size_t count = 0;
    for (const std::span<const DxgiRow> group : dxgi_row_groups) {
        count += group.size();
    }
    return count;
}

inline constexpr auto dxgi_rows = [] {
    std::array<DxgiRow, dxgi_row_count()> rows{};
    auto output = rows.begin();
    for (const std::span<const DxgiRow> group : dxgi_row_groups) {
        output = std::copy(group.begin(), group.end(), output);
    }
    std::sort(rows.begin(), rows.end(),
              [](const DxgiRow& left, const DxgiRow& right) { return left.format < right.format; });
    return rows;
}();

constexpr FormatInfo to_format_info(const DxgiRow& row) {
    return {row.format,
            row.name,
            row.pixel_format,
            row.depth_pixel_format,
            row.substitution,
            row.swizzle,
            row.kind,
            row.typeless_family,
            row.block_width,
            row.block_height,
            row.bytes_per_block,
            row.plane_count};
}

inline constexpr auto dxgi_format_infos = [] {
    std::array<FormatInfo, dxgi_rows.size()> infos{};
    std::transform(dxgi_rows.begin(), dxgi_rows.end(), infos.begin(), to_format_info);
    return infos;
}();

constexpr bool has_unique_values() {
    return std::adjacent_find(dxgi_rows.begin(), dxgi_rows.end(),
                              [](const DxgiRow& left, const DxgiRow& right) {
                                  return left.format == right.format;
                              }) == dxgi_rows.end();
}

static_assert(has_unique_values(), "DXGI_FORMAT table rows must have distinct values");

} // namespace directmetal::formats::tables
