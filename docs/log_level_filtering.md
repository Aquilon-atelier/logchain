Previous step [Log rotation](log_rotation.md) · [Index](index.md) · Next step [Call Site: Source Location and Stack Traces](source_location.md)

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

If the selected level is disabled, the log arguments are **not evaluated**:

```cpp
logger.set_minimum_level(LogLevel::warning);

LOG_DEBUG(logger, "state = {}", build_expensive_debug_state());   // build_expensive_debug_state() is not called
LOG_WARN(logger, "state = {}", build_expensive_debug_state());    // it is called
```

The logger expression is evaluated exactly once:

```cpp
LOG_WARN(get_logger(), "message");
```

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
When the level is disabled, the body of the `if` is not executed, so the log arguments are **not evaluated** either.

The proxy only holds a reference to the logger and should be used as a short-lived expression. Do not store it beyond the lifetime of the logger.

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

However, because function arguments are evaluated before entering a function, this form **cannot avoid evaluating its arguments**:

```cpp
logger.log(LogLevel::debug, expensive());   // expensive() is called even if DEBUG is disabled
```

## Choosing an API

| API                              | Avoids argument evaluation | Dynamic level |
| -------------------------------- | -------------------------: | ------------: |
| `LOG_DEBUG(logger, ...)`         |                        Yes |            No |
| `if (auto log = logger.debug())` |                        Yes |            No |
| `LOG(logger, level, ...)`        |                        Yes |           Yes |
| `logger.log(level, ...)`         |                         No |           Yes |

* For a statically known level where the cost of the arguments matters, use the macro or the lazy proxy.
* For a level chosen at run time, use `LOG(logger, level, ...)`.
* Use `logger.log(level, ...)` when you prefer the explicit, macro-free API and the arguments are cheap.

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

## Going further

Where exactly the level is checked, what each API evaluates and what it costs, and how to avoid a collision with the macro names of another library (`LOGCHAIN_NO_SHORT_MACROS`) are described in [Evaluation Model and Level Filtering](advanced/evaluation_model.md).

Previous step [Log rotation](log_rotation.md) · [Index](index.md) · Next step [Call Site: Source Location and Stack Traces](source_location.md)
