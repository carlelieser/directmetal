#include <directmetal/com/object.hpp>

namespace directmetal::com {

ULONG RefCounts::add_ref() noexcept {
    const ULONG previous = public_count_.fetch_add(1, std::memory_order_relaxed);
    if (previous == 0) {
        add_private_ref();
    }
    return previous + 1;
}

ULONG RefCounts::release() noexcept {
    const ULONG previous = public_count_.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1) {
        release_private();
    }
    return previous - 1;
}

void RefCounts::add_private_ref() noexcept {
    private_count_.fetch_add(1, std::memory_order_relaxed);
}

bool RefCounts::release_private() noexcept {
    return private_count_.fetch_sub(1, std::memory_order_acq_rel) == 1;
}

} // namespace directmetal::com
