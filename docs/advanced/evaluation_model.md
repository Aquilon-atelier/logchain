Previous step [Stack Traces](stack_traces.md) · [Advanced](index.md) · Next step [FileSink: Guarantees and Edge Cases](file_sink.md)

# Evaluation Model and Level Filtering

## In brief

`logchain` filters events by level **before** doing any work for them. What differs between the logging APIs is whether the **arguments** of a disabled statement are still evaluated.  
This page states precisely what each API evaluates, what it costs, and how the macros are built.

**Read this page if** you log from performance-sensitive code, if your log arguments have side effects or are expensive, or if a `logchain` macro collides with another library. For everyday use, the [Log Level and Filtering](../log_level_filtering.md) guide is enough.

## Reference

### Where the level is checked

```text
LOG_DEBUG(logger, "x = {}", f())
        │
        ▼
 logger.is_enabled(Debug)? ── no ──► nothing else runs, f() is not called
        │ yes
        ▼
 evaluate message and arguments
        │
        ▼
 logger.log_unchecked(...)  ──► build event ──► render ──► sinks
```

`is_enabled(level)` is a single relaxed atomic load of the minimum level of that logger. It takes no lock and allocates nothing. `log_unchecked` is the entry point that **assumes the check has been done**; it is public so that the macros and the proxy can call it, and it is not meant to be called directly.

### What each API evaluates

| API | Level checked before arguments are evaluated | Arguments of a disabled statement | Level |
| :-- | :-: | :-- | :-- |
| `LOG_TRACE` ... `LOG_FATAL` | Yes | **Not evaluated** | Fixed at the call site |
| `LOG(logger, level, ...)` | Yes | **Not evaluated** | Any expression |
| `if (auto log = logger.debug())` | Yes | **Not evaluated** (the body is skipped) | Fixed at the call site |
| `logger.log(level, ...)` | No: the check is inside the call | **Evaluated** (they are function arguments) | Any expression |

All four skip the logging pipeline when the level is disabled. Only the first three also skip the evaluation of the arguments.

### Macros

The macros expand to a `do { ... } while (0)` block:

1. the `logger` expression is bound **once** to a local reference, so `LOG_WARN(get_logger(), "...")` calls `get_logger()` exactly once, even when the level is disabled;
2. `is_enabled(level)` is tested;
3. only if it is enabled, the message and the arguments are evaluated and passed to `log_unchecked`.

Consequences:

* a macro is a statement, not an expression: it cannot be used inside a larger expression;
* arguments that contain a comma outside parentheses (a template argument list, for instance) need an extra pair of parentheses, as for any macro;
* `extra(a, b, c)` expands to named arguments built from the **spelling** of each expression (`"a"`, `"b"`, `"c"`). It is itself a macro and is only evaluated when the level is enabled.

### Lazy proxy

`logger.debug()` returns a `LogProxy` holding **a reference** to the logger. Its conversion to `bool` is the same relaxed atomic load as `is_enabled`. Its call operator takes the message as a `Located`, so that the call site is captured in your expression, not in the library.

* Use it as a short-lived expression, as the condition of an `if`. Do not store it beyond the lifetime of the logger.
* Call it only after the conversion to `bool` returned `true`.

### Macro names and collisions

The short macro names (`LOG`, `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_FATAL`, `extra`) are provided by default. They can collide with other libraries (glog, Abseil...). Define `LOGCHAIN_NO_SHORT_MACROS` **before** including `logchain` to disable them. The prefixed forms remain available:

```cpp
#define LOGCHAIN_NO_SHORT_MACROS
#include "logchain/logger.hpp"

LOGCHAIN_LOG_INFO(logger, "Request handled", LOGCHAIN_EXTRA(request_id, retry_count));
LOGCHAIN_LOG(logger, level, "dynamic level");
```

## Behavior and guarantees

* The minimum level is a **per-logger** setting. Changing one logger does not change another.
* Filtering never depends on the sinks or the renderers: a disabled event is never built, rendered or written.
* The `logger` expression of a macro is evaluated once. The message and arguments are evaluated at most once, and only if the level is enabled.
* `logger.log(level, ...)` evaluates its arguments **before** entering the function. No implementation of this API can avoid that.

## Cost

| Situation | Cost |
| :-------- | :--- |
| Disabled statement, macro or proxy | One relaxed atomic load and a branch. The arguments are not evaluated. |
| Disabled statement, `logger.log(...)` | One relaxed atomic load, plus whatever it costs to evaluate the arguments. |
| Enabled statement | Evaluation of the arguments, construction of the event, rendering, writing. |

## Design rationale

**Why check the level in the macro and not only in `log()`?** A function cannot skip the evaluation of its own arguments. Placing the check in the macro, and the call in its body, is the only way to give `LOG_DEBUG(logger, expensive())` the property that `expensive()` is not called.

**Why also offer a proxy?** The proxy provides the same property without the preprocessor, at the price of an `if`. Projects that forbid macros can keep early filtering.

**Why keep `log()`?** It is the explicit, macro-free form, and the only one that accepts a level that is not a compile-time expression without a macro. Its cost model is simple and honest: arguments are evaluated.

**Why capture the call site in the macro and the proxy?** The location must be that of your statement. See [Call Site](../source_location.md#every-logging-api-captures-the-call-site).

## Pitfalls

| Pitfall | Consequence | What to do |
| :------ | :---------- | :--------- |
| `logger.log(LogLevel::debug, expensive())` | `expensive()` is called even when `DEBUG` is disabled | Use `LOG_DEBUG` or the proxy. |
| Relying on a side effect in the arguments of a macro (`LOG_DEBUG(logger, "{}", ++n)`) | `n` changes only when the level is enabled | Keep arguments free of side effects. |
| Storing `auto log = logger.debug();` beyond the logger | Dangling reference | Use the proxy in the condition of an `if`. |
| Another library defining `LOG_INFO`, `LOG`... | Macro redefinition | Define `LOGCHAIN_NO_SHORT_MACROS`, then use the `LOGCHAIN_*` forms. |
| Defining `LOGCHAIN_NO_SHORT_MACROS` after the include | No effect | Define it before the first `logchain` include, or on the command line. |

Previous step [Stack Traces](stack_traces.md) · [Advanced](index.md) · Next step [FileSink: Guarantees and Edge Cases](file_sink.md)
