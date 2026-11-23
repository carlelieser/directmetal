#pragma once

#include <directmetal/backend/types.hpp>
#include <directmetal/bridge/protocol.hpp>

// Control calls with their NTSTATUS mapped to backend::Status; failures are logged with the
// function and status.
namespace directmetal::bridge {

backend::Status report(FunctionCode code, Status status) noexcept;
backend::Status invoke(FunctionCode code, void* params) noexcept;

// The value extract takes from params once the call has succeeded.
template <typename Value, typename Params, typename Extract>
backend::Result<Value> invoke_for(FunctionCode code, Params& params, Extract extract) {
    const backend::Status status = invoke(code, &params);
    if (status != backend::Status::success) {
        return backend::Result<Value>::failure(status);
    }
    return backend::Result<Value>::success(extract(params));
}

} // namespace directmetal::bridge
