#include <directmetal/backend/device.hpp>
#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/staging_ring.hpp>
#include <directmetal/bridge/stream_writer.hpp>
#include <directmetal/formats/formats.hpp>
#include <directmetal/host/dispatch.hpp>
#include <directmetal/memory/range_allocator.hpp>
#include <directmetal/memory/ring_allocator.hpp>
#include <directmetal/runtime/config.hpp>
#include <directmetal/runtime/hud.hpp>
#include <directmetal/runtime/log.hpp>

static_assert(sizeof(void*) == 8);
