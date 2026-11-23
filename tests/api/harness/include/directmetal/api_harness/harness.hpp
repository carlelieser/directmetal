#pragma once

// Shared support for API test programs. Programs print one JSON document to stdout and log to
// stderr; everything platform-specific stays in the harness sources.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace directmetal::api_harness {

enum class Driver { hardware, warp };

struct Options {
    Driver driver = Driver::hardware;
};

// Parses `--driver hardware|warp`. Prints usage to stderr and returns false on bad arguments.
bool parse_options(int argc, char** argv, Options& options);

// Makes stdout byte-exact and keeps failures from opening dialogs.
void prepare_process();

std::string format_hresult(std::int32_t result);
std::string to_hex(const void* data, std::size_t size);

class JsonWriter {
public:
    void begin_object();
    void end_object();
    void begin_array();
    void end_array();
    void key(std::string_view name);
    void string(std::string_view text);
    void number(std::int64_t value);
    void boolean(bool value);
    void null();

    // Writes the document and a newline to stdout.
    void print() const;

private:
    void begin_value();

    std::string text_;
    std::vector<bool> has_items_;
    bool is_after_key_ = false;
};

void log(std::string_view message);

} // namespace directmetal::api_harness
