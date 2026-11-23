#include "platform.hpp"

#include <windows.h>

#include <array>
#include <vector>

namespace directmetal::platform {
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

std::optional<std::string> environment_variable(const std::string& name) {
    const std::wstring wide_name = to_utf16(name);
    const DWORD length = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (length <= 1) {
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(length);
    const DWORD copied = GetEnvironmentVariableW(wide_name.c_str(), buffer.data(), length);
    // The variable can change or vanish between the two calls.
    const bool is_complete = copied != 0 && copied < length;
    if (!is_complete) {
        return std::nullopt;
    }
    return to_utf8({buffer.data(), copied});
}

FileHandle open_append(const std::string& path) noexcept {
    try {
        const std::wstring wide_path = to_utf16(path);
        HANDLE file =
            CreateFileW(wide_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        return file == INVALID_HANDLE_VALUE ? no_file : reinterpret_cast<FileHandle>(file);
    } catch (const std::bad_alloc&) {
        return no_file;
    }
}

bool write_all(FileHandle file, std::string_view data) noexcept {
    while (!data.empty()) {
        DWORD written = 0;
        const BOOL is_written = WriteFile(reinterpret_cast<HANDLE>(file), data.data(),
                                          static_cast<DWORD>(data.size()), &written, nullptr);
        if (!is_written || written == 0) {
            return false;
        }
        data.remove_prefix(written);
    }
    return true;
}

void close_file(FileHandle file) noexcept {
    CloseHandle(reinterpret_cast<HANDLE>(file));
}

ReadResult read_text_file(const std::string& path) {
    const std::wstring wide_path = to_utf16(path);
    HANDLE file = CreateFileW(wide_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        const bool is_missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        return {is_missing ? ReadStatus::missing : ReadStatus::failed, {}};
    }
    ReadResult result{ReadStatus::ok, {}};
    std::array<char, 4096> buffer;
    while (true) {
        DWORD count = 0;
        const BOOL is_read =
            ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr);
        if (!is_read || result.text.size() + count > max_text_file_bytes) {
            result = {ReadStatus::failed, {}};
            break;
        }
        if (count == 0) {
            break;
        }
        result.text.append(buffer.data(), count);
    }
    CloseHandle(file);
    return result;
}

std::string executable_path() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            return to_utf8({buffer.data(), length});
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace directmetal::platform
