#include <directmetal/api_harness/harness.hpp>

#include <array>
#include <cstdio>
#include <string_view>

namespace directmetal::api_harness {

namespace {

struct DriverName {
    std::string_view name;
    Driver driver;
};

constexpr std::array<DriverName, 2> driver_names = {{
    {"hardware", Driver::hardware},
    {"warp", Driver::warp},
}};

bool find_driver(std::string_view name, Driver& driver) {
    for (const DriverName& entry : driver_names) {
        if (entry.name == name) {
            driver = entry.driver;
            return true;
        }
    }
    return false;
}

} // namespace

bool parse_options(int argc, char** argv, Options& options) {
    const bool has_driver_flag = argc == 3 && std::string_view(argv[1]) == "--driver";
    if (has_driver_flag && find_driver(argv[2], options.driver)) {
        return true;
    }
    if (argc == 1) {
        return true;
    }
    std::fprintf(stderr, "usage: %s [--driver hardware|warp]\n", argv[0]);
    return false;
}

} // namespace directmetal::api_harness
