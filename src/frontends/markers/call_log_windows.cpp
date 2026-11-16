#include "call_log_platform.hpp"

#include <windows.h>

#include <bcrypt.h>

#include <vector>

namespace directmetal::frontends::call_log_platform {
namespace {

std::string to_utf8(std::wstring_view text) {
    const int size = static_cast<int>(text.size());
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), size, result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring to_utf16(std::string_view text) {
    const int size = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), size, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), size, result.data(), length);
    return result;
}

} // namespace

std::optional<std::string> log_directory() {
    const wchar_t* name = L"DIRECTMETAL_CALL_LOG_DIR";
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length <= 1) {
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(length);
    const DWORD copied = GetEnvironmentVariableW(name, buffer.data(), length);
    // The variable can change or vanish between the two calls.
    const bool is_complete = copied != 0 && copied < length;
    if (!is_complete) {
        return std::nullopt;
    }
    return to_utf8({buffer.data(), copied});
}

std::string executable_name() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    std::wstring_view path(buffer.data(), length);
    path.remove_prefix(path.find_last_of(L"\\/") + 1);
    return to_utf8(path.substr(0, path.rfind(L'.')));
}

std::uint32_t process_id() noexcept {
    return GetCurrentProcessId();
}

bool fill_random(std::span<std::byte> buffer) noexcept {
    auto* data = reinterpret_cast<PUCHAR>(buffer.data());
    const auto size = static_cast<ULONG>(buffer.size());
    return BCryptGenRandom(nullptr, data, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

FileHandle open_append(const std::string& directory, const std::string& file_name) {
    const bool has_separator =
        !directory.empty() && directory.find_last_of("\\/") == directory.size() - 1;
    const std::wstring path = to_utf16(directory + (has_separator ? "" : "\\") + file_name);
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return file == INVALID_HANDLE_VALUE ? no_file : reinterpret_cast<FileHandle>(file);
}

bool write_line(FileHandle file, std::string_view line) noexcept {
    DWORD written = 0;
    const BOOL is_written = WriteFile(reinterpret_cast<HANDLE>(file), line.data(),
                                      static_cast<DWORD>(line.size()), &written, nullptr);
    return is_written && written == line.size();
}

unsigned long last_error() noexcept {
    return GetLastError();
}

} // namespace directmetal::frontends::call_log_platform
