#include "call_log.hpp"

#include "call_log_platform.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <new>

namespace directmetal::frontends {
namespace {

using namespace call_log_platform;

constexpr std::size_t suffix_bytes = 8;
constexpr std::size_t max_line_length = 512;

void report_failure(const char* operation, const std::string& target) noexcept {
    std::fprintf(stderr, "directmetal: call log %s failed for %s: error %lu\n", operation,
                 target.c_str(), last_error());
}

std::string random_suffix() {
    std::array<std::byte, suffix_bytes> bytes{};
    if (!fill_random(bytes)) {
        return {};
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string suffix;
    for (const std::byte byte : bytes) {
        const auto value = std::to_integer<unsigned>(byte);
        suffix += digits[value >> 4];
        suffix += digits[value & 0xF];
    }
    return suffix;
}

FileHandle open_call_log() noexcept {
    try {
        const std::optional<std::string> directory = log_directory();
        if (!directory) {
            return no_file;
        }
        const std::string suffix = random_suffix();
        if (suffix.empty()) {
            report_failure("random suffix", *directory);
            return no_file;
        }
        const std::string name =
            executable_name() + "-" + std::to_string(process_id()) + "-" + suffix + ".log";
        const FileHandle file = open_append(*directory, name);
        if (file == no_file) {
            report_failure("open", *directory + "/" + name);
        }
        return file;
    } catch (const std::bad_alloc&) {
        std::fputs("directmetal: call log open failed: out of memory\n", stderr);
        return no_file;
    }
}

// The environment is read on the first recording and the result kept for the process.
FileHandle call_log_file() noexcept {
    static const FileHandle file = open_call_log();
    return file;
}

} // namespace

void append_to_call_log(const char* key) noexcept {
    const FileHandle file = call_log_file();
    if (file == no_file) {
        return;
    }
    std::array<char, max_line_length> line{};
    const std::size_t length = std::strlen(key);
    if (length >= line.size()) {
        std::fprintf(stderr, "directmetal: call log skipped key longer than %zu bytes: %.64s\n",
                     line.size() - 1, key);
        return;
    }
    std::memcpy(line.data(), key, length);
    line[length] = '\n';
    if (!write_line(file, {line.data(), length + 1})) {
        std::fprintf(stderr, "directmetal: call log write failed for %s: error %lu\n", key,
                     last_error());
    }
}

} // namespace directmetal::frontends
