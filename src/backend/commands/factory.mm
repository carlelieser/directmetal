#include "factory.hpp"

#include "resource_access.hpp"
#include "timeline.hpp"

namespace directmetal::backend::commands {

Result<std::unique_ptr<Queue>> create_queue(Device&) {
    return Result<std::unique_ptr<Queue>>::failure(Status::unsupported);
}

Result<std::unique_ptr<Timeline>> create_timeline(Device& device, std::uint64_t initial_value) {
    @autoreleasepool {
        id<MTLSharedEvent> event = [resources::metal_device(device) newSharedEvent];
        if (event == nil) {
            return Result<std::unique_ptr<Timeline>>::failure(Status::out_of_memory);
        }
        event.signaledValue = initial_value;
        return Result<std::unique_ptr<Timeline>>::success(std::make_unique<MetalTimeline>(event));
    }
}

} // namespace directmetal::backend::commands
