# Getting Started

## Integrating logchain

# Getting Started

## Compiler support

`logchain` requires a C++26 compiler supporting static reflection (P2996).  
At the time of writing, GCC 16 (or later) is the recommended compiler.  

The library is header-only and has no external dependencies.  

## Building logchain

After cloning the repository, configure and build the project using one of the provided CMake presets.

```bash
git clone https://github.com/Aquilon-io/logchain.git
cd logchain

cmake --preset=release
cmake --build --preset=release
```

The default configuration builds the library and the example programs.  
Unit tests can be enabled through the corresponding CMake option.

## CMake options

The following CMake options control the project configuration.

| Option | Default | Description |
| :------------------------------ | :---: | :---------- |
| `LOGCHAIN_BUILD_EXAMPLES`       | `ON`  | Build the example programs. |
| `LOGCHAIN_BUILD_TESTS`          | `OFF` | Build the unit tests. |
| `LOGCHAIN_BUILD_STRESS_TESTS`   | `OFF` | Build the stress tests. |
| `LOGCHAIN_FETCH_GTEST`          | `OFF` | Download GoogleTest with `FetchContent` if it is not found locally. |
| `LOGCHAIN_WARNINGS_AS_ERRORS`   | `ON`  | Treat compiler warnings as errors. |

## Using logchain in a CMake project

`logchain` is a header-only library and exposes the CMake target `logchain::logchain`.

Add the library to your project using your preferred dependency management strategy (for example `add_subdirectory()`, `FetchContent`, or a package manager), then link your target against `logchain::logchain`.

```cmake
add_executable(my_application
    main.cpp
)

target_link_libraries(my_application
    PRIVATE
        logchain::logchain
)
```

## Basic usage 

```cpp
#include "logger.hpp"
#include "sink.hpp"

#include <filesystem>
#include <string>

using namespace logchain;

struct User {
    std::string name{"bob"};
};

int main() {

    const std::filesystem::path log_path{"/tmp/application.log"};

    // Declare one or several seeks
    ConsoleSink console_sink;
    FileSink file_sink{log_path};

    // Declare logger
    Logger logger{make_target(console_sink), make_target(file_sink)};

    User user{"bob"};
    std::string request_id{"req-A"};
    int retry_count{3};

    // Log a message and possibly add extra info 
    LOG(logger, "Request handled", user, request_id, retry_count);
}
```



