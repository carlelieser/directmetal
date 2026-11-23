#include <directmetal/backend/device.hpp>
#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/commands.hpp>
#include <directmetal/bridge/control.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/staging_ring.hpp>
#include <directmetal/bridge/stream_writer.hpp>
#include <directmetal/com/boundary.hpp>
#include <directmetal/com/object.hpp>
#include <directmetal/com/private_data.hpp>
#include <directmetal/com/ref.hpp>
#include <directmetal/formats/formats.hpp>
#include <directmetal/memory/range_allocator.hpp>
#include <directmetal/memory/ring_allocator.hpp>
#include <directmetal/runtime/config.hpp>
#include <directmetal/runtime/hud.hpp>
#include <directmetal/runtime/log.hpp>

static_assert(sizeof(void*) == 4 || sizeof(void*) == 8);
