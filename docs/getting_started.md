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

The following example creates a logger with both a console and a file sink:

```cpp
#include "logger.hpp"

#include <filesystem>
#include <string>

using namespace logchain;

struct User {
    std::string name{"bob"};
    unsigned int age;
};

int main() {
    const std::filesystem::path log_path{"/tmp/application.log"};

    ConsoleSink console_sink;
    FileSink file_sink{log_path};

    Logger logger{target(console_sink), target(file_sink)};

    User user{"bob", 35};
    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(logger,
             "Request handled",
             extra(request_id, user, retry_count));
}
```

The same event can be rendered differently by the configured sinks.  
For example, the console renderer can produce a human-readable representation:

```text
2026-08-20T17:31:30.501852Z [INFO ] Request handled  request_id=req-A  user={name=bob age=35}  retry_count=3
```

while a JSON renderer can preserve the structured fields:

```json
{
  "timestamp": "2026-08-20T15:42:17.381Z",
  "level": "INFO",
  "message": "Request handled",
  "user":{"name":"bob","age":35},
  "request_id": "req-A",
  "retry_count": 3
}
```

The macro is optional. The same event can be created using the explicit API:

```cpp
logger.log_named(
    LogLevel::info,
    "Request handled",
    named("request_id", request_id),
    named("retry_count", retry_count));
```

This form does not use the preprocessor and is suitable for projects that prefer an explicit, macro-free API.

Next step : [Rendering](rendering.md)