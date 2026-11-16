#include "stub_report.hpp"

#include <cstdio>

namespace directmetal::frontends {

void report_stub(const char* key) noexcept {
    std::fprintf(stderr, "directmetal: stub %s\n", key);
}

} // namespace directmetal::frontends
