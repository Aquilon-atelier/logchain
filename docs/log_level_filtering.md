Previous step [Log rotation](log_rotation.md)  
Next step [Call Site: Source Location and Stack Traces](source_location.md)


# Log Level Filtering

## Log levels

logchain defines six ordered log levels:

```cpp
enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};
```

`Logger` supports runtime filtering of log messages through a minimum log level.
The minimum level is configured with `set_minimum_level()`:

```cpp
Logger logger;
logger.set_minimum_level(LogLevel::warning);
```

Only messages whose level is greater than or equal to the configured minimum level are processed.

The following levels are enabled:

```text
WARNING
ERROR
FATAL
```

while these levels are filtered out:

```text
TRACE
DEBUG
INFO
```

The default minimum level is `LogLevel::trace`, so all log levels are enabled by default.

## Querying the minimum level

The current minimum level can be retrieved with:

```cpp
LogLevel level = logger.minimum_level();
```

The filtering decision can also be queried directly:

```cpp
if (logger.is_enabled(LogLevel::debug)) {
    // DEBUG logging is currently enabled.
}
```

`is_enabled(level)` returns `true` when `level` is greater than or equal to the current minimum level.

In other words, the filtering rule is equivalent to:

```cpp
level >= logger.minimum_level()
```

## Filtering and evaluation of log arguments

Level filtering is performed before the logging pipeline is invoked.

With the macro API:

```cpp
LOG_DEBUG(logger, expensive());
```

if `DEBUG` is disabled, `expensive()` is **not evaluated**.

This is an important property of the macro and lazy proxy APIs: disabled log messages can avoid the cost of constructing or computing their arguments.

For example:

```cpp
logger.set_minimum_level(LogLevel::warning);

LOG_DEBUG(logger, expensive());
LOG_WARN(logger, expensive());
```

The first call does not evaluate `expensive()`, while the second one does.

The same property is provided by the C++ lazy proxy API:

```cpp
if (auto log = logger.debug()) {
    log(expensive());
}
```

When `DEBUG` is disabled, the body of the `if` is not executed and therefore `expensive()` is not evaluated.

This makes the lazy proxy API suitable when avoiding the cost of log arguments is important.

## Lazy level-specific API

`Logger` provides one lazy proxy for each log level:

```cpp
logger.trace()
logger.debug()
logger.info()
logger.warning()
logger.error()
logger.fatal()
```

They can be used as follows:

```cpp
if (auto log = logger.debug()) {
    log("value = {}", value);
}
```

The proxy has an explicit conversion to `bool` and is intended to be used directly as the condition of an `if`.

The logger is not owned by the proxy. The proxy only holds a reference to the logger and should therefore be used as a short-lived expression:

```cpp
if (auto log = logger.debug()) {
    log("debug message");
}
```

A proxy should not be stored beyond the lifetime of the logger it references.

## Macro API

The logging macros provide early level filtering and avoid evaluating log arguments when the corresponding level is disabled:

```cpp
LOG_TRACE(logger, "trace message");
LOG_DEBUG(logger, "debug message");
LOG_INFO(logger, "info message");
LOG_WARN(logger, "warning message");
LOG_ERROR(logger, "error message");
LOG_FATAL(logger, "fatal message");
```

For example:

```cpp
logger.set_minimum_level(LogLevel::warning);

LOG_DEBUG(logger, "filtered out");
LOG_INFO(logger, "filtered out");
LOG_WARN(logger, "accepted");
LOG_ERROR(logger, "accepted");
LOG_FATAL(logger, "accepted");
```

The generic macro can be used when the level is determined dynamically:

```cpp
LogLevel level = get_log_level();
LOG(logger, level, "message");
```

This form preserves early filtering, so log arguments are not evaluated when the selected level is disabled.

The logger expression is evaluated exactly once:

```cpp
LOG_WARN(get_logger(), "message");
```

This is important when the logger expression has side effects or performs non-trivial work.

## Generic `log()` API

The generic C++ API is:

```cpp
logger.log(level, arguments...);
```

For example:

```cpp
logger.log(LogLevel::warning, "warning message");
```

It performs the same level filtering before invoking the logging pipeline.

However, because function arguments are evaluated before entering the function, this form cannot prevent evaluation of its arguments:

```cpp
logger.log(LogLevel::debug, expensive());
```

Even when `DEBUG` is disabled, `expensive()` has already been evaluated before `Logger::log()` is entered.

Therefore, when avoiding argument evaluation is required, prefer either the macro API:

```cpp
LOG_DEBUG(logger, expensive());
```

or the lazy proxy API:

```cpp
if (auto log = logger.debug()) {
    log(expensive());
}
```

## Choosing an API

The logging APIs provide different trade-offs:

| API                              | Early filtering | Avoids argument evaluation | Dynamic level |
| -------------------------------- | --------------: | -------------------------: | ------------: |
| `LOG_DEBUG(logger, ...)`         |             Yes |                        Yes |            No |
| `if (auto log = logger.debug())` |             Yes |                        Yes |            No |
| `LOG(logger, level, ...)`        |             Yes |                        Yes |           Yes |
| `logger.log(level, ...)`         |             Yes |                         No |           Yes |

For a statically known log level where argument evaluation should be avoided, use the macro or lazy proxy API.

For a dynamically selected level, use:

```cpp
LOG(logger, level, ...);
```

This preserves early filtering and therefore avoids evaluating the log arguments when the selected level is disabled.

Use:

```cpp
logger.log(level, ...);
```

when the convenience of the generic C++ API is preferred and argument evaluation is acceptable.

## Runtime reconfiguration

The minimum level can be changed at any time:

```cpp
Logger logger;

logger.set_minimum_level(LogLevel::error);

// TRACE, DEBUG, INFO and WARNING are filtered.
LOG_WARN(logger, "filtered");
LOG_ERROR(logger, "accepted");
LOG_FATAL(logger, "accepted");

logger.set_minimum_level(LogLevel::debug);

// DEBUG and all higher levels are now accepted.
LOG_DEBUG(logger, "accepted");
```

The minimum level is a per-`Logger` setting; it is not a global setting.

Changing the minimum level of one logger does not change the filtering configuration of another logger:

```cpp
Logger logger1;
Logger logger2;

logger1.set_minimum_level(LogLevel::error);
logger2.set_minimum_level(LogLevel::debug);
```

Here, `logger1` processes only `ERROR` and `FATAL`, while `logger2` processes `DEBUG`, `INFO`, `WARNING`, `ERROR`, and `FATAL`.

## Early filtering

Level filtering is intentionally performed as early as possible.

For the macro API and lazy proxy API, a disabled log statement does not enter the logging pipeline and does not evaluate its arguments.  

This is particularly useful for debug and trace logging containing expensive operations:

```cpp
LOG_DEBUG(logger, "state = {}", build_expensive_debug_state());
```

When `DEBUG` is disabled, `build_expensive_debug_state()` is not called.

The generic `log()` API still avoids entering the logging pipeline for disabled messages, but cannot avoid evaluation of ordinary function arguments:

```cpp
logger.log(LogLevel::debug, build_expensive_debug_state());
```

For performance-sensitive logging, prefer the early-filtering APIs when the level is known at the call site.

Previous step [Log rotation](log_rotation.md)
Next step [Call Site: Source Location and Stack Traces](source_location.md)

