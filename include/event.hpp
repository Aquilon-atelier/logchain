#pragma once

#include <string_view>

namespace logchain {

struct LogEvent {
    std::string_view message;
};

} // namespace logchain
