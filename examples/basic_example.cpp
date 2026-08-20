#include "logger.hpp"

#include <filesystem>
#include <string>

using namespace logchain;

// ===========================================================================
// A simple example using the macro-based version of the API.
// ===========================================================================
struct User {
    std::string name;
    unsigned int age;
};

int main() {
    namespace fs = std::filesystem;
    const std::filesystem::path log_path{"/tmp/application.log"};

    ConsoleSink console_sink;
    FileSink file_sink{log_path};

    Logger logger{target(console_sink), target(file_sink)};

    User user{"bob", 35};
    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(logger, "Request handled", user, request_id, retry_count);
    fs::remove(log_path);
}
