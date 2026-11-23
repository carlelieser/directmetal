#include <directmetal/api_harness/harness.hpp>

#include <array>
#include <cstdio>

namespace directmetal::api_harness {

namespace {

constexpr std::array<char, 16> hex_digits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                             '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

void append_escaped(std::string& text, std::string_view raw) {
    for (const char character : raw) {
        const auto code = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            text += '\\';
            text += character;
        } else if (code < 0x20) {
            text += "\\u00";
            text += hex_digits[code >> 4];
            text += hex_digits[code & 0xf];
        } else {
            text += character;
        }
    }
}

} // namespace

std::string format_hresult(std::int32_t result) {
    std::array<char, 11> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "0x%08x", static_cast<std::uint32_t>(result));
    return buffer.data();
}

std::string to_hex(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::string text;
    text.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        text += hex_digits[bytes[index] >> 4];
        text += hex_digits[bytes[index] & 0xf];
    }
    return text;
}

void JsonWriter::begin_value() {
    if (is_after_key_) {
        is_after_key_ = false;
        return;
    }
    if (!has_items_.empty()) {
        if (has_items_.back()) {
            text_ += ',';
        }
        has_items_.back() = true;
    }
}

void JsonWriter::begin_object() {
    begin_value();
    text_ += '{';
    has_items_.push_back(false);
}

void JsonWriter::end_object() {
    has_items_.pop_back();
    text_ += '}';
}

void JsonWriter::begin_array() {
    begin_value();
    text_ += '[';
    has_items_.push_back(false);
}

void JsonWriter::end_array() {
    has_items_.pop_back();
    text_ += ']';
}

void JsonWriter::key(std::string_view name) {
    begin_value();
    text_ += '"';
    append_escaped(text_, name);
    text_ += "\":";
    is_after_key_ = true;
}

void JsonWriter::string(std::string_view text) {
    begin_value();
    text_ += '"';
    append_escaped(text_, text);
    text_ += '"';
}

void JsonWriter::number(std::int64_t value) {
    begin_value();
    text_ += std::to_string(value);
}

void JsonWriter::boolean(bool value) {
    begin_value();
    text_ += value ? "true" : "false";
}

void JsonWriter::null() {
    begin_value();
    text_ += "null";
}

void JsonWriter::print() const {
    std::fwrite(text_.data(), 1, text_.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void log(std::string_view message) {
    std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()), message.data());
}

} // namespace directmetal::api_harness
