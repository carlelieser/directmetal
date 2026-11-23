#include "factory.hpp"

// Interim factories so the device and resources link before queues and timelines exist.
namespace directmetal::backend::commands {

Result<std::unique_ptr<Queue>> create_queue(Device&) {
    return Result<std::unique_ptr<Queue>>::failure(Status::unsupported);
}

Result<std::unique_ptr<Timeline>> create_timeline(Device&, std::uint64_t) {
    return Result<std::unique_ptr<Timeline>>::failure(Status::unsupported);
}

} // namespace directmetal::backend::commands
