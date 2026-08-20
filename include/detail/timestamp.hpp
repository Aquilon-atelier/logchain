#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace logchain::detail {

inline std::string format_timestamp(std::chrono::system_clock::time_point timestamp) {
    using namespace std::chrono;

    const auto whole_seconds = time_point_cast<std::chrono::seconds>(timestamp);

    const auto micros = duration_cast<microseconds>(timestamp - whole_seconds).count();

    const std::time_t time = system_clock::to_time_t(whole_seconds);

    std::tm utc{};

#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    std::ostringstream out;

    out << std::setfill('0') << std::setw(4) << utc.tm_year + 1900 << '-' << std::setw(2) << utc.tm_mon + 1 << '-'
        << std::setw(2) << utc.tm_mday << 'T' << std::setw(2) << utc.tm_hour << ':' << std::setw(2) << utc.tm_min << ':'
        << std::setw(2) << utc.tm_sec << '.' << std::setw(6) << micros << 'Z';

    return out.str();
}

} // namespace logchain::detail
