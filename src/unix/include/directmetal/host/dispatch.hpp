#pragma once

#include <directmetal/backend/device.hpp>
#include <directmetal/bridge/protocol.hpp>

#include <cstdint>
#include <memory>
#include <vector>

// The unix side of the bridge: handle table, control calls and one consumer thread per stream.
// Both Wine call tables and the native bridge enter through dispatch(). Stream threads are plain
// pthreads that never call into Wine.
//
// Handles are never reused within a process: each slot carries a 24-bit generation that is
// incremented on reuse, and a slot whose generation is exhausted is retired. Every decoded command
// is validated against the descriptions of the resources it names before it reaches the backend;
// a violation makes the stream's error sticky.
namespace directmetal::host {

// Source of backend devices; tests substitute a fake to run the host without a GPU.
class DeviceProvider {
public:
    virtual ~DeviceProvider() = default;
    virtual backend::Result<std::unique_ptr<backend::Device>>
    create(const backend::DeviceRequest& request) = 0;
    virtual std::vector<backend::Capabilities> adapters() = 0;
};

class Host {
public:
    explicit Host(DeviceProvider& provider);
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host();

    // code is a bridge::FunctionCode index; params is that code's parameter struct. Thread-safe.
    bridge::Status call(std::uint32_t code, void* params) noexcept;

private:
    class State;
    std::unique_ptr<State> state_;
};

// The process-wide Host backed by the Metal backend, created on first use. The Metal device is
// created on the first device call, never while directmetal.so is being loaded.
bridge::Status dispatch(std::uint32_t code, void* params) noexcept;

} // namespace directmetal::host
