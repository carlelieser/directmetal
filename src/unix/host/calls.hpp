#pragma once

#include "host_context.hpp"

#include <directmetal/bridge/control.hpp>

// One function per control call. Each receives a copy of the caller's parameters, which the
// dispatcher copies back only on success, so the PE side cannot change them mid-call.
namespace directmetal::host {

bridge::Status query_version(HostContext& context, bridge::QueryVersionParams& params);
bridge::Status log_call(HostContext& context, bridge::LogCallParams& params, CallerAbi abi);
bridge::Status initialize(HostContext& context, bridge::InitializeParams& params, CallerAbi abi);
bridge::Status enumerate_adapters(HostContext& context, bridge::EnumerateAdaptersParams& params);
bridge::Status create_device(HostContext& context, bridge::CreateDeviceParams& params);
bridge::Status destroy_device(HostContext& context, bridge::DestroyDeviceParams& params);
bridge::Status create_buffer(HostContext& context, bridge::CreateBufferParams& params);
bridge::Status import_memory(HostContext& context, bridge::ImportMemoryParams& params);
bridge::Status create_texture(HostContext& context, bridge::CreateTextureParams& params);
bridge::Status release_memory(HostContext& context, bridge::ReleaseMemoryParams& params);
bridge::Status create_timeline(HostContext& context, bridge::CreateTimelineParams& params);
bridge::Status destroy_timeline(HostContext& context, bridge::DestroyTimelineParams& params);
bridge::Status wait_timeline(HostContext& context, bridge::WaitTimelineParams& params);
bridge::Status open_stream(HostContext& context, bridge::OpenStreamParams& params);
bridge::Status submit_stream(HostContext& context, bridge::SubmitStreamParams& params);
bridge::Status close_stream(HostContext& context, bridge::CloseStreamParams& params);

} // namespace directmetal::host
