// Drives the bridge directly: uploads patterned bytes through one staging ring, copies them on the
// GPU into a region of a second ring, and reads them back, over enough passes that both rings wrap
// while an earlier region is still live. Prints one pass or fail line to stdout and exits non-zero
// on failure. In 32-bit builds it runs under WoW64 and proves that VirtualAlloc'd memory below
// 4 GiB is imported without copying: the GPU reads the CPU's writes and the CPU reads the GPU's.

#include <directmetal/bridge/client.hpp>
#include <directmetal/bridge/device_client.hpp>
#include <directmetal/bridge/shared_block_allocator.hpp>
#include <directmetal/bridge/staging_ring.hpp>
#include <directmetal/bridge/stream_writer.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace {

using namespace directmetal;
using namespace directmetal::bridge;

#if defined(_WIN32) && !defined(_WIN64)
constexpr bool is_wow64 = true;
#else
constexpr bool is_wow64 = false;
#endif

constexpr std::uint64_t ring_capacity = 4 * shared_memory_alignment;
constexpr std::uint64_t region_alignment = 256;
constexpr std::uint64_t wow64_address_limit = std::uint64_t{1} << 32;
constexpr std::chrono::nanoseconds patience = std::chrono::seconds(10);
constexpr std::byte unwritten{0xcd};
// Bytes copied by each pass, multiples of 4 as copy_buffer requires. Pass 4 and pass 8 do not fit
// before the end of the 64 KiB rings and wrap to offset 0 while the previous pass is still live.
constexpr std::array<std::uint64_t, 8> pass_sizes = {20480, 20480, 20480, 20476,
                                                     12288, 20480, 4,     32768};

std::byte pattern(std::uint64_t pass, std::uint64_t index) noexcept {
    return static_cast<std::byte>((index * 31 + pass * 17 + (index >> 8)) & 0xff);
}

struct Report {
    std::string failure;
    // Times either ring handed out a region before the previous one.
    int wraps = 0;

    bool fail(std::string text) {
        failure = std::move(text);
        return false;
    }
};

std::string status_text(backend::Status status) {
    return "status " + std::to_string(static_cast<int>(status));
}

// A pass whose readback is checked once its submit completes.
struct PendingCheck {
    std::uint64_t pass;
    StagingRing::Allocation readback;
    std::uint64_t size;
};

struct RingUse {
    StagingRing ring;
    std::optional<std::uint64_t> last_offset;
    int wraps = 0;

    std::optional<StagingRing::Allocation> allocate(std::uint64_t size, std::uint64_t pass) {
        auto allocation = ring.allocate(size, region_alignment, pass);
        if (allocation.has_value() && last_offset.has_value() &&
            allocation->offset < *last_offset) {
            ++wraps;
        }
        if (allocation.has_value()) {
            last_offset = allocation->offset;
        }
        return allocation;
    }
};

class Roundtrip {
public:
    Roundtrip(StreamWriter& writer, TimelineClient progress, std::array<StagingRing, 2> rings)
        : writer_(&writer), progress_(progress), upload_{std::move(rings[0]), std::nullopt, 0},
          readback_{std::move(rings[1]), std::nullopt, 0} {}

    bool run_pass(std::uint64_t pass, std::uint64_t size, Report& report);
    // Waits for the last pass and checks it, then checks that both rings wrapped.
    bool finish(Report& report);
    int wraps() const noexcept { return upload_.wraps + readback_.wraps; }

private:
    // Writes the pattern for check.pass into upload and marks the readback region unwritten.
    // Under WoW64 the shared memory must lie below 4 GiB.
    bool fill(const StagingRing::Allocation& upload, const PendingCheck& check, Report& report);
    // Waits for the pending pass, checks its readback and retires both rings to the mirror.
    bool settle(Report& report);

    StreamWriter* writer_;
    TimelineClient progress_;
    RingUse upload_;
    RingUse readback_;
    std::optional<PendingCheck> pending_;
};

bool Roundtrip::run_pass(std::uint64_t pass, std::uint64_t size, Report& report) {
    const auto upload = upload_.allocate(size, pass);
    const auto readback = readback_.allocate(size, pass);
    const std::string name = "pass " + std::to_string(pass);
    if (!upload.has_value() || !readback.has_value()) {
        return report.fail(name + ": ring full");
    }
    // A failed allocation leaves the ring unchanged.
    if (upload_.ring.allocate(ring_capacity, region_alignment, pass).has_value()) {
        return report.fail(name + ": ring handed out space overlapping a live region");
    }
    const PendingCheck check{pass, *readback, size};
    if (!fill(*upload, check, report)) {
        return false;
    }
    writer_->record(CopyBufferCommand{upload->buffer, upload->offset, readback->buffer,
                                      readback->offset, size});
    if (!settle(report)) {
        return false;
    }
    writer_->record(SubmitCommand{0});
    pending_ = check;
    if (writer_->status() < 0) {
        return report.fail(name + ": stream status " + std::to_string(writer_->status()));
    }
    return true;
}

bool Roundtrip::fill(const StagingRing::Allocation& upload, const PendingCheck& check,
                     Report& report) {
    if constexpr (is_wow64) {
        if (bridge::to_address(upload.cpu_address) + check.size > wow64_address_limit) {
            return report.fail("shared memory above 4 GiB under WoW64");
        }
    }
    for (std::uint64_t index = 0; index < check.size; ++index) {
        upload.cpu_address[index] = pattern(check.pass, index);
    }
    std::memset(check.readback.cpu_address, static_cast<int>(unwritten), check.size);
    return true;
}

bool Roundtrip::settle(Report& report) {
    if (!pending_.has_value()) {
        return true;
    }
    const PendingCheck check = *pending_;
    pending_.reset();
    const std::string name = "pass " + std::to_string(check.pass);
    if (const auto status = progress_.wait(check.pass, patience);
        status != backend::Status::success) {
        return report.fail(name + ": wait failed with " + status_text(status));
    }
    for (std::uint64_t index = 0; index < check.size; ++index) {
        if (check.readback.cpu_address[index] != pattern(check.pass, index)) {
            return report.fail(name + ": readback differs at byte " + std::to_string(index));
        }
    }
    upload_.ring.retire(progress_.completed());
    readback_.ring.retire(progress_.completed());
    return true;
}

bool Roundtrip::finish(Report& report) {
    if (!settle(report)) {
        return false;
    }
    if (upload_.wraps == 0 || readback_.wraps == 0) {
        return report.fail("a ring never wrapped");
    }
    return true;
}

bool run_passes(Roundtrip& roundtrip, Report& report) {
    std::uint64_t pass = 0;
    bool is_passing = true;
    for (const std::uint64_t size : pass_sizes) {
        is_passing = is_passing && roundtrip.run_pass(++pass, size, report);
    }
    is_passing = is_passing && roundtrip.finish(report);
    report.wraps = roundtrip.wraps();
    return is_passing;
}

// The rings are released and freed on return, after every command naming them is submitted.
Report run_rings(DeviceClient& device, StreamWriter& writer, TimelineClient progress) {
    Report report;
    auto upload = StagingRing::create(device, ring_capacity);
    auto readback = StagingRing::create(device, ring_capacity);
    if (!upload.has_value() || !readback.has_value()) {
        report.fail("StagingRing::create failed with " +
                    status_text(upload.has_value() ? readback.status() : upload.status()));
        return report;
    }
    Roundtrip roundtrip(writer, progress, {std::move(upload).value(), std::move(readback).value()});
    if (!run_passes(roundtrip, report)) {
        writer.record(SubmitCommand{0});
    }
    writer.flush();
    return report;
}

bool run_on_device(DeviceClient& device, StreamControl& control, Report& report) {
    auto opened = device.open_stream(control);
    if (!opened.has_value()) {
        return report.fail("open_stream failed with " + status_text(opened.status()));
    }
    StreamWriter writer(opened.value().stream, control);
    const TimelineClient progress(opened.value().progress_timeline, &control.completed_progress);
    report = run_rings(device, writer, progress);
    device.close_stream(opened.value().stream);
    return report.failure.empty();
}

bool run(Report& report) {
    if (const auto status = initialize("bridge_ring_roundtrip", is_wow64);
        status != backend::Status::success) {
        return report.fail("initialize failed with " + status_text(status));
    }
    auto device = DeviceClient::create(0, {});
    if (!device.has_value()) {
        return report.fail("create_device failed with " + status_text(device.status()));
    }
    StreamControl* control = shared_blocks().allocate_stream_control();
    const bool is_passing = control != nullptr
                                ? run_on_device(device.value(), *control, report)
                                : report.fail("allocating the stream control failed");
    device.value().destroy();
    if (control != nullptr) {
        shared_blocks().free(control, sizeof(StreamControl));
    }
    return is_passing;
}

} // namespace

int main() {
    Report report;
    if (!run(report)) {
        std::printf("bridge_ring_roundtrip: fail: %s\n", report.failure.c_str());
        return 1;
    }
    std::printf("bridge_ring_roundtrip: pass: %zu passes, %d wraps, wow64 %d\n", pass_sizes.size(),
                report.wraps, is_wow64 ? 1 : 0);
    return 0;
}
