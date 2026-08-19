#include "logger.hpp"
#include "sink.hpp"

#include <filesystem>
#include <string>

using namespace logchain;

// ===========================================================================
// 5. Démo
// ===========================================================================

int main() {
    namespace fs = std::filesystem;

    ConsoleSink console_sink;
    std::string log_path{"application.log"};
    FileSink file_sink{log_path};

    Logger logger{console_sink, file_sink};

    std::string user{"bob"};
    std::string request_id{"req-A"};
    int retry_count{3};

    logger.log("Request handled", user, request_id, retry_count);
    fs::remove(log_path);
}
