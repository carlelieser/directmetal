#pragma once

#include "handle_table.hpp"
#include "mirror.hpp"
#include "objects.hpp"
#include "work_queue.hpp"

#include <directmetal/backend/commands.hpp>
#include <directmetal/bridge/stream_decoder.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace directmetal::host {

struct ExecutorSetup {
    HandleTable* table;
    // The device that owns the stream; commands may only name its objects.
    bridge::Handle device;
    backend::Queue* queue;
    std::shared_ptr<MirrorWriter> progress_mirror;
};

// Validates each decoded command against the resources it names and records it into the queue.
// Objects named by recorded commands are held until the submit that commits them, so a resource
// destroyed in between is not released while unsubmitted work still references it. Used by the
// stream's consumer thread only.
class CommandExecutor final : public bridge::CommandSink {
public:
    explicit CommandExecutor(const ExecutorSetup& setup) noexcept : setup_(setup) {}

    bridge::Status execute(const bridge::DecodedCommand& command) noexcept override;
    // Submits recorded but unsubmitted work when unsubmitted is commit, and reports what remains.
    Settled settle(Unsubmitted unsubmitted) noexcept;

private:
    using Handler = bridge::Status (CommandExecutor::*)(const bridge::DecodedCommand&);

    bridge::Status nop(const bridge::DecodedCommand& command);
    bridge::Status set_label(const bridge::DecodedCommand& command);
    bridge::Status destroy_object(const bridge::DecodedCommand& command);
    bridge::Status clear_texture(const bridge::DecodedCommand& command);
    bridge::Status clear_depth_stencil(const bridge::DecodedCommand& command);
    bridge::Status copy_buffer(const bridge::DecodedCommand& command);
    bridge::Status copy_buffer_to_texture(const bridge::DecodedCommand& command);
    bridge::Status copy_texture_to_buffer(const bridge::DecodedCommand& command);
    bridge::Status copy_texture(const bridge::DecodedCommand& command);
    bridge::Status signal_timeline(const bridge::DecodedCommand& command);
    bridge::Status submit(const bridge::DecodedCommand& command);

    // The device's live object handle names, or a null object.
    TableEntry find(bridge::Handle handle) const;
    std::shared_ptr<BufferObject> find_buffer(bridge::Handle handle) const;
    std::shared_ptr<TextureObject> find_texture(bridge::Handle handle) const;
    bridge::Status copy_between(const bridge::BufferTextureRegion& region, bool is_upload);
    void hold(std::shared_ptr<void> object);
    bridge::Status submit_recorded() noexcept;

    static const std::array<Handler, static_cast<std::size_t>(bridge::Opcode::count)> handlers;

    ExecutorSetup setup_;
    std::vector<std::shared_ptr<void>> held_;
    bool has_recorded_ = false;
};

} // namespace directmetal::host
