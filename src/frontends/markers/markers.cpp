#include "call_log.hpp"
#include "stub_report.hpp"

#include <directmetal/frontends/markers.hpp>

namespace directmetal::frontends {

void record_call(const char* key, MarkerKind kind) noexcept {
    append_to_call_log(key);
    if (kind == MarkerKind::stub) {
        report_stub(key);
    }
}

} // namespace directmetal::frontends
