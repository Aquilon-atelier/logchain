Previous step [Log Level and Filtering](log_level_filtering.md)  

# Call Site: Source Location and Stack Traces

## Overview

Every log event can carry the **call site** that produced it: source file, line and, optionally, the enclosing function.

The call site is captured with `std::source_location` (C++20). No compiler extension and no preprocessor trick are involved: the capture is done by a default argument evaluated in the caller's call expression, so `LOG_*` macros, the lazy proxies and the generic `logger.log()` API all report the line of **your** logging statement.

What is printed is chosen **per renderer** with the `source` option, using `SourceMode`:

| `SourceMode` | Console renderer | JSON renderer |
| :----------- | :--------------- | :------------ |
| `Never` (default) | Nothing: output is identical to previous versions. | Nothing: no `source` key. |
| `Short` | File **basename** and line, in the line prefix. | `"source":{"file":<basename>,"line":N}` |
| `Full` | File path **as given to the compiler**, line, plus a `function=` field. | `"source":{"file":<path>,"line":N,"function":<name>}` |

Because the option belongs to the renderer, the same event can show its location on the console and not in a file (or the opposite):

```cpp
ConsoleRenderer{.source = SourceMode::Short}
JsonRenderer{.source = SourceMode::Full}
```

## Console layout

Without a call site (`SourceMode::Never`), the console layout is unchanged:

```text
2026-10-23T08:15:02.104233Z [INFO ] Request handled  request_id=req-A  retry_count=3
```

With `Short` or `Full`, the timestamp, the level and the location are grouped in a single prefix:

```text
[<timestamp>:<LEVEL>:<file>:<line>] <message>  <fields>
```

```text
[2026-10-23T08:15:02.104233Z:INFO:main.cpp:18] Request handled  request_id=req-A  retry_count=3
```

A few details worth knowing:

* The level name is **not padded** in the located prefix (`WARN`, not `WARN `).
* With `Full`, the line additionally ends with a `function=` field, **after** the user fields. The value is quoted when it contains spaces, which is almost always the case:

  ```text
  [2026-10-23T08:15:02.104233Z:ERROR:/home/me/app/main.cpp:29] Request failed  function="int main()"
  ```

* With colors enabled (`ColorMode::Always`, or `Auto` on a terminal), only the level keeps its color; the rest of the prefix (timestamp, file, line) and the `function=` field are rendered in gray.

## JSON layout

With `Short` or `Full`, a single reserved `source` object is inserted **between `message` and the context fields**:

```json
{"timestamp":"2026-10-23T08:15:02.104233Z","level":"INFO","message":"Request handled","source":{"file":"main.cpp","line":18},"request_id":"req-A","retry_count":3}
```

```json
{"timestamp":"2026-10-23T08:15:02.104233Z","level":"INFO","message":"Request handled","source":{"file":"/home/me/app/main.cpp","line":18,"function":"void handle_request()"},"request_id":"req-A","retry_count":3}
```

Using one reserved key means that your own context names (`file`, `line`, `function`...) can never collide with the call-site fields.

## Complete example

The following program combines most of the behaviors described above: two targets with different `SourceMode`s, the macro API, the lazy proxy, the generic API with a `named()` field, and a format argument.

```cpp
#include "logchain/logger.hpp"                                                         // 1

#include <string>                                                                      // 3

using namespace logchain;                                                              // 5

ConsoleSink console_sink;                                                              // 7
FileSink file_sink{"/tmp/application.log"};                                            // 8

Logger logger{                                                                         // 10
    Target{ConsoleRenderer{.colors = ColorMode::Auto, .source = SourceMode::Short}, console_sink},
    Target{JsonRenderer{.source = SourceMode::Full}, file_sink}};                      // 12

void handle_request() {                                                                // 14
    const std::string request_id{"req-A"};
    const int retry_count{3};

    LOG_INFO(logger, "Request handled", extra(request_id, retry_count));               // 18

    if (auto log = logger.debug()) {
        log("cache lookup {}", 42);                                                    // 21
    }
}

int main() {                                                                           // 25
    handle_request();

    logger.log(LogLevel::warning, "slow request {}", 1250, named("request_id", std::string{"req-B"})); // 28
    LOG_ERROR(logger, "Request failed");                                               // 29
}
```

> The trailing `// N` comments are the line numbers used in the output below. Each logging statement is kept on a single line, as in the unit tests, so that the expected line is unambiguous.

Console output (`SourceMode::Short`, `main.cpp` being the file name):

```text
[2026-10-23T08:15:02.104233Z:INFO:main.cpp:18] Request handled  request_id=req-A  retry_count=3
[2026-10-23T08:15:02.104301Z:DEBUG:main.cpp:21] cache lookup 42
[2026-10-23T08:15:02.104352Z:WARN:main.cpp:28] slow request 1250  request_id=req-B
[2026-10-23T08:15:02.104398Z:ERROR:main.cpp:29] Request failed
```

`/tmp/application.log` (`SourceMode::Full`, the path is the one passed to the compiler):

```json
{"timestamp":"2026-10-23T08:15:02.104233Z","level":"INFO","message":"Request handled","source":{"file":"/home/me/app/main.cpp","line":18,"function":"void handle_request()"},"request_id":"req-A","retry_count":3}
{"timestamp":"2026-10-23T08:15:02.104301Z","level":"DEBUG","message":"cache lookup 42","source":{"file":"/home/me/app/main.cpp","line":21,"function":"void handle_request()"}}
{"timestamp":"2026-10-23T08:15:02.104352Z","level":"WARN","message":"slow request 1250","source":{"file":"/home/me/app/main.cpp","line":28,"function":"int main()"},"request_id":"req-B"}
{"timestamp":"2026-10-23T08:15:02.104398Z","level":"ERROR","message":"Request failed","source":{"file":"/home/me/app/main.cpp","line":29,"function":"int main()"}}
```

The exact `function` spelling is the one produced by `std::source_location::function_name()`, so it depends on the compiler (the examples above follow GCC). The timestamps are illustrative.

## Every logging API captures the call site

The location is the line of the logging statement, whichever API you use:

```cpp
// Generic API
logger.log(LogLevel::info, "hello {}", 42, named("user", std::string{"bob"}), named("id", 7));

// Generic macro (dynamic level) and per-level macros
LOG(logger, LogLevel::warning, "macro {}", id, extra(user));
LOG_ERROR(logger, "level macro");

// Lazy proxy
if (auto log = logger.info()) {
    log("via proxy {}", 1);
}
```

The message can be a string literal, a `std::string`, a `std::string_view` or a `const char *`: all convert to the message parameter in one step.

### Wrappers: do not forward the message through a pack

The location is captured where the message argument is **written**. If you build your own helper around `logger.log()` and forward the message through a parameter pack, the reported location will be that of your helper, not of its caller:

```cpp
// The location reported is this line, inside the helper.
template <typename... Args>
void my_log(Args &&...args) {
    logger.log(LogLevel::info, std::forward<Args>(args)...);
}
```

Prefer the `LOG_*` macros, which expand in the caller's function, or declare the message parameter yourself with the library's `Located` type, as `LogProxy` does:

```cpp
template <typename... Args>
void my_log(Located message, Args &&...args) {
    logger.log(LogLevel::info, message, std::forward<Args>(args)...);
}
```

`Located` carries the message and its `std::source_location`; it is passed through the pipeline unchanged and never captured a second time.

## Events without a call site

A `LogEvent` has an optional location (`where`). Events built without one keep the default layout, **even when the renderer is configured with `SourceMode::Full`**: no prefix change, no `function=` field and no `source` key.

```cpp
const LogEvent ev{.level = LogLevel::info,
                  .timestamp = std::chrono::system_clock::now(),
                  .message = "m"};

ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full}.render(ev, make_context());
// -> "<timestamp> [INFO ] m"

JsonRenderer{.source = SourceMode::Full}.render(ev, make_context());
// -> no "source" key
```

This matters if you render events yourself with the renderers outside of a `Logger`.

## Testing the location in your own code

The unit tests of the library (`tests/unit/source_location_test.cpp`) use a tiny capturing sink and compare the line number against `__LINE__` taken **on the same source line** as the logging call. The same technique works for testing your own logging:

```cpp
#include <gtest/gtest.h>

#include <logchain/logger.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace logchain;

struct CaptureSink final : Sink {
    std::vector<std::string> lines;

    void write(std::string_view message) override { lines.emplace_back(message); }

    [[nodiscard]] const std::string &last() const { return lines.back(); }
};

static bool contains(const std::string &s, std::string_view needle) {
    return s.find(needle) != std::string::npos;
}

TEST(MyLogging, ConsoleShortReportsTheCallingLine) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;
    LOG_ERROR(lg, "level macro"); line = __LINE__;   // same line on purpose

    const auto basename = detail::source_basename(__FILE__);
    EXPECT_TRUE(contains(sink.last(), ":ERROR:" + std::string{basename} + ":" + std::to_string(line) + "] level macro"));
}

TEST(MyLogging, JsonShortPlacesSourceBetweenMessageAndContext) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{.source = SourceMode::Short}, sink}};

    lg.log(LogLevel::error, "json short", named("user", std::string{"bob"}));

    const auto &s = sink.last();
    EXPECT_FALSE(contains(s, "\"function\""));
    EXPECT_LT(s.find("\"message\""), s.find("\"source\""));
    EXPECT_LT(s.find("\"source\""), s.find("\"user\""));
}
```

Keep the `line = __LINE__;` assignment on the logging line itself: reformatting such a line (for example with `clang-format`) breaks the expectation.

## Behavior summary

The following behaviors are covered by the unit tests:

| Behavior | Console | JSON |
| :------- | :------ | :--- |
| `Never` leaves the output unchanged | ✔ | no `source` key |
| `Short` uses the file basename | ✔ | ✔ |
| `Full` adds the function | trailing `function=` field | `function` in `source` |
| Position of the location | line prefix | between `message` and context |
| Colors (`ColorMode::Always`) | level colored, rest gray | not applicable |
| Event without location | default layout | no `source` key |
| Macros, proxy and generic API | line of the statement | line of the statement |

## Stack traces

> **Not implemented yet.**

Support for stack traces (`std::stacktrace`) is planned. It will complement the call site described on this page: where `SourceMode` answers "which statement logged this event?", a stack trace answers "how did the program get there?".

This section will document the following once the feature is available:

* how to enable stack traces on a renderer;
* how they are rendered by the console and JSON renderers;
* their cost, and how level filtering keeps it away from disabled log statements.

Until then, use `SourceMode::Full` to get the file, the line and the enclosing function of each event.

Previous step [Log Level and Filtering](log_level_filtering.md)
