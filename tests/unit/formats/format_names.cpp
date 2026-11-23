// Prints the name of every table entry, one per line, prefixed with its table: "dxgi" or "d3d9".
// check_format_coverage.py compares the output with the API inventories.
#include <directmetal/formats/formats.hpp>

#include <cstdio>

int main() {
    for (const auto& format : directmetal::formats::all_dxgi_formats()) {
        std::printf("dxgi %.*s\n", static_cast<int>(format.name.size()), format.name.data());
    }
    for (const auto& format : directmetal::formats::all_d3d9_formats()) {
        std::printf("d3d9 %.*s\n", static_cast<int>(format.name.size()), format.name.data());
    }
    return 0;
}
