// Copyright (c) 2026 Sylvain Ladoux
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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

    LOG_INFO(logger, "Request handled", extra(user, request_id, retry_count));
    fs::remove(log_path);
}
