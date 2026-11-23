#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// A unix side for the D3D11 front end without a GPU: the test executable defines the bridge
// transport exports, answers every control call from this state and decodes submitted chunks.
// Submits complete immediately.
namespace fake_bridge {

using directmetal::bridge::Opcode;
using directmetal::bridge::Status;

struct Command {
    Opcode opcode;
    std::vector<std::byte> payload;
};

struct State {
    // Results the next calls of each kind return.
    Status initialize_status = directmetal::bridge::status_success;
    Status create_texture_status = directmetal::bridge::status_success;
    int shared_allocations = 0;
    int shared_frees = 0;
    int devices_created = 0;
    int devices_destroyed = 0;
    int submits = 0;
    std::vector<std::uint64_t> imported_sizes;
    int memory_releases = 0;
    std::vector<directmetal::backend::TextureDesc> textures;
    std::vector<Command> commands;
    directmetal::bridge::StreamControl* control = nullptr;
};

State& state();
// Clears the state; call before each test.
void reset();
// Makes the stream's error sticky, as the host does after a failed command.
void fail_stream();

template <typename Payload> std::vector<Payload> commands_of() {
    std::vector<Payload> found;
    for (const Command& command : state().commands) {
        if (command.opcode == directmetal::bridge::opcode_of<Payload>) {
            Payload payload;
            std::memcpy(&payload, command.payload.data(), sizeof(payload));
            found.push_back(payload);
        }
    }
    return found;
}

} // namespace fake_bridge
