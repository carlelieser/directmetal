#include "factory.hpp"

#include "queue.hpp"
#include "resource_access.hpp"
#include "timeline.hpp"

namespace directmetal::backend::commands {

Result<std::unique_ptr<Queue>> create_queue(Device& device) {
    @autoreleasepool {
        id<MTLDevice> metal = resources::metal_device(device);
        id<MTLCommandQueue> queue = [metal newCommandQueue];
        id<MTLSharedEvent> progress = [metal newSharedEvent];
        if (queue == nil || progress == nil) {
            return Result<std::unique_ptr<Queue>>::failure(Status::out_of_memory);
        }
        auto created = std::make_unique<MetalQueue>(device, queue, progress);
        resources::track_queue(device, *created);
        return Result<std::unique_ptr<Queue>>::success(std::move(created));
    }
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
