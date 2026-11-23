#pragma once

#include <directmetal/backend/commands.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace directmetal::backend::testing {

// A timeline the test completes by hand with signal_cpu, standing in for the command lane's.
class FakeTimeline final : public Timeline {
public:
    std::uint64_t completed_value() const noexcept override { return completed_.load(); }

    Status wait(std::uint64_t value, std::chrono::nanoseconds) noexcept override {
        return completed_value() >= value ? Status::success : Status::timeout;
    }

    void signal_cpu(std::uint64_t value) noexcept override {
        std::vector<std::function<void()>> ready;
        {
            const std::lock_guard lock(mutex_);
            completed_.store(value);
            std::erase_if(waiting_, [&](auto& waiter) {
                if (waiter.first > value) {
                    return false;
                }
                ready.push_back(std::move(waiter.second));
                return true;
            });
        }
        for (auto& callback : ready) {
            callback();
        }
    }

    void notify(std::uint64_t value, std::function<void()> callback) override {
        {
            const std::lock_guard lock(mutex_);
            if (completed_.load() < value) {
                waiting_.emplace_back(value, std::move(callback));
                return;
            }
        }
        callback();
    }

    std::size_t waiting_count() {
        const std::lock_guard lock(mutex_);
        return waiting_.size();
    }

private:
    std::atomic<std::uint64_t> completed_ = 0;
    std::mutex mutex_;
    std::vector<std::pair<std::uint64_t, std::function<void()>>> waiting_;
};

class NullRecorder final : public CommandRecorder {
public:
    void clear_texture(Texture&, SubresourceRange, ClearColor) override {}
    void clear_depth_stencil(Texture&, SubresourceRange, const DepthStencilClear&) override {}
    void copy_buffer(const BufferCopy&) override {}
    void copy_buffer_to_texture(const BufferTextureCopy&) override {}
    void copy_texture_to_buffer(const BufferTextureCopy&) override {}
    void copy_texture(const TextureCopy&) override {}
    void signal(Timeline&, std::uint64_t) override {}
};

// Submits only advance last_submitted_value; the test completes them through progress().
class FakeQueue final : public Queue {
public:
    CommandRecorder& recorder() noexcept override { return recorder_; }

    Status submit() noexcept override {
        ++submitted_;
        return Status::success;
    }

    std::uint64_t last_submitted_value() const noexcept override { return submitted_.load(); }
    Timeline& progress() noexcept override { return progress_; }
    FakeTimeline& fake_progress() noexcept { return progress_; }

private:
    NullRecorder recorder_;
    FakeTimeline progress_;
    std::atomic<std::uint64_t> submitted_ = 0;
};

} // namespace directmetal::backend::testing
