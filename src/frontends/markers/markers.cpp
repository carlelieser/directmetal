#include "stub_report.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::frontends {

void record_call(const char* key, MarkerKind kind) noexcept {
    if (kind == MarkerKind::stub) {
        report_stub(key);
    }
}

} // namespace directmetal::frontends
