[Advanced](index.md) · Next step [Evaluation Model and Level Filtering](evaluation_model.md)

# Stack Traces

## In brief

A stack trace answers "how did the program get there?" for an event caused by an exception. `logchain` does not capture anything when an event is logged.  
The trace is captured **where the exception is thrown**, travels with the exception, and is printed when the exception is declared as the cause of an `error` or `fatal` event.

Stack traces use `std::stacktrace` (C++23). They are **opt-in at compile time** and cost nothing when disabled.

**Read this page if** you need the exact conditions under which a trace is printed, its cost, the JSON layout, or if you build without CMake. For a short introduction, see [Call Site: Source Location](../source_location.md#stack-traces).

## Requirements

* C++23 or later, with `std::stacktrace` (`__cpp_lib_stacktrace`).
* A libstdc++ configured with `--enable-libstdcxx-backtrace` (the default of most builds), and its extension library `libstdc++exp` (recent GCC; older versions use `libstdc++_libbacktrace` instead). The CMake check below tells you which one your toolchain provides.
* RTTI: the trace is found with `dynamic_cast`, so `-fno-rtti` is not supported.

If `LOGCHAIN_WITH_STACKTRACE` is defined and `std::stacktrace` is not available, compilation stops with an explicit `#error` instead of silently disabling the feature.

## Enabling stack traces

### With CMake

```text
cmake --preset stacktrace          # or: -DLOGCHAIN_WITH_STACKTRACE=ON
```

| CMake variable | Default | Meaning |
| :------------- | :-----: | :------ |
| `LOGCHAIN_WITH_STACKTRACE` | `OFF` | `ON` enables stack traces. |
| `LOGCHAIN_STACKTRACE_LIBRARY` | `stdc++exp` | Library providing the backend of `std::stacktrace`. Use `stdc++_libbacktrace` for older GCC, or an empty value if none is needed. |

When the option is `ON`, the configuration step compiles **and links** a small program that uses `std::stacktrace`. If that fails, configuration stops with an error that tells you what to check. The feature is never disabled silently.

On success, the `logchain::logchain` target carries `LOGCHAIN_WITH_STACKTRACE` as an interface compile definition and links the library as an interface dependency. Every target that links `logchain::logchain` therefore gets both, in the right order on the command line. The same applies to an installed package: the choice is made when `logchain` is configured, and `find_package(logchain)` imports the target as it was built.

### Without CMake

Three things are required, and none of them can be provided by a header-only library:

```bash
g++ -std=c++26 -freflection -I<logchain>/include \
    -g -DLOGCHAIN_WITH_STACKTRACE \
    main.cpp -o app -lstdc++exp
```

* define `LOGCHAIN_WITH_STACKTRACE` in **every** translation unit of the program (see [Pitfalls](#pitfalls));
* compile with C++23 or later;
* link `-lstdc++exp`, **after** the source files and objects. `libstdc++exp` is a static library: the linker only takes from it the symbols it already needs, so placed before the sources it has no effect.

The symptom of a missing or misplaced library is a link error:

```text
undefined reference to `std::__stacktrace_impl::_S_current(int (*)(void*, unsigned long), void*, int)'
```

## Reference

### `throw_traced`

```cpp
template <typename E>
[[noreturn]] void throw_traced(E error, std::stacktrace captured = std::stacktrace::current());
```

Throws `error` as a `traced<E>`, together with the trace of the caller. `E` must be a non-final class derived from `std::exception`.

```cpp
void connect(int attempt) {
    if (attempt > 1) {
        throw_traced(std::runtime_error{"connection refused"});
    }
}
```

The `captured` parameter is a **default argument** and must not be passed. Because default arguments are evaluated in the caller's expression, `std::stacktrace::current()` runs in the function that calls `throw_traced`, exactly as `std::source_location::current()` does for the call site. The first frame of the trace is therefore the function that threw, even when the optimizer inlines `throw_traced` itself.

If `error` already carries a trace (it is a `traced<E>`), it is thrown as is and **the trace of its first throw is kept**.

### `traced<E>`

`traced<E>` derives from both `E` and `traced_base`. It **is a** `E`: every existing `catch (const E &)` handler keeps working. It can be built explicitly, for instance to wrap an existing exception:

```cpp
throw traced<std::logic_error>(std::logic_error{"..."}, std::stacktrace::current());
```

`traced_base::trace()` returns the `std::stacktrace`.

### `caused_by`

```cpp
LOG_ERROR(logger, "Request failed: {}", caused_by(ex), ex.what(), extra(request_id));
```

`caused_by(ex)` declares the exception that caused the event. It must be the **first argument after the message**, and it is **not** a format argument. It works with every logging API (macros, `logger.log(...)`, lazy proxies) and combines with format arguments, `extra(...)` and `named(...)`.

`caused_by` only stores a pointer to the exception, valid for the duration of the log call.

### Renderer option

Both renderers have a `stacktrace` option, `true` by default. It is independent of `source`:

```cpp
ConsoleRenderer{.stacktrace = false}
JsonRenderer{.source = SourceMode::Short}   // stacktrace = true
```

## When is a trace printed?

A trace is printed only when **all** of the following hold:

1. `LOGCHAIN_WITH_STACKTRACE` is defined;
2. the log call receives the exception through `caused_by(...)`;
3. that exception was thrown with `throw_traced` (or is a `traced<E>`);
4. the level of the event is `error` or `fatal`;
5. the `stacktrace` option of the renderer is `true`.

In every other case the output is **unchanged**. A `caused_by(ex)` on a warning, or on an exception that carries no trace, is accepted and prints nothing extra. An exception that is caught and logged by message only, without `caused_by`, never prints a trace.

## Output layout

### Console

The trace is printed **below** the event line, one line per frame, innermost frame first. With colors enabled, frames are gray.

```text
[2026-10-23T17:59:37.773827Z:ERROR:main.cpp:36] Request failed: connection refused  request_id=req-A
  #0 connect(int) at /home/me/app/service.cpp:16
  #1 main at /home/me/app/main.cpp:34
  #2 __libc_start_call_main
  #3 __libc_start_main_alias_2
  #4 _start
```

The file and the line are omitted when they are unknown, as for the C runtime frames above. A frame with no known function is printed as `??`.

With `SourceMode::Full`, the `function=` field stays at the end of the event line, and the frames follow on their own lines.

### JSON

A single reserved `stacktrace` key is added **after `source` and before the context fields**. It is an array of frames, innermost first, so that the event stays on **one line**:

```json
{"timestamp":"2026-10-23T17:59:37.773827Z","level":"ERROR","message":"Request failed: connection refused","stacktrace":[{"function":"connect(int)","file":"/home/me/app/service.cpp","line":16},{"function":"main","file":"/home/me/app/main.cpp","line":34}],"request_id":"req-A"}
```

An unknown file is `""` and an unknown line is `0`. As for `source`, using one reserved key means that your own context names can never collide with the library's field.

## What the trace contains

* The first frame is the function that called `throw_traced`. Frames of `logchain` itself are not part of the trace.
* Function names come from the symbol table. Files and lines come from the debug information: compile with `-g` to get them. In a stripped binary, frames have neither.
* With optimization, the compiler may inline functions. Inlined calls are reported according to the debug information, and frames without any information are dropped.
* The spelling of function names is the one produced by the implementation of `std::stacktrace_entry::description()`, so it depends on the standard library.

## Behavior and guarantees

* **Copies keep the trace.** Copying a `traced<E>` (a rethrow by value, `std::exception_ptr`, `std::rethrow_exception`) keeps the trace captured at the original throw. It is never recaptured where the exception is copied or logged.
* **No double wrapping.** `throw_traced` on an exception that already carries a trace throws it unchanged.
* **Inert when disabled.** Without `LOGCHAIN_WITH_STACKTRACE`, `traced<E>` is `E` itself, `throw_traced(e)` is a plain `throw`, and `caused_by(...)` is accepted and ignored. Code using them needs no `#ifdef`. As for any argument of the `LOG_*` macros, `caused_by(...)` is not evaluated when the level is disabled.
* **Independent from the call site.** `stacktrace` and `source` are separate options and can be combined freely.
* **Events without exception.** Events built by hand, or by a custom event type that has no exception, never print a trace.

## Cost

| When | What is paid |
| :--- | :----------- |
| At the throw | `throw_traced` walks the stack once and stores the addresses. Symbols are **not** resolved here. This is paid on **every** `throw_traced`, whether or not the exception is logged. |
| At the log call | Symbols, files and lines are resolved **only** when a trace is actually printed (conditions above). Level filtering keeps disabled statements free, as for any event. |
| Disabled | Nothing. |

Use `throw_traced` for exceptional failures, not for regular control flow.

## Design rationale

**Why capture at the throw and not at the log call?** A trace taken in a `catch` block describes the handler, not the failure. The information is only available where the exception is thrown, so it has to travel with the exception.

**Why `throw_traced` and not a hook on every exception?** C++ offers no standard way to observe an arbitrary `throw`, and `logchain` uses standard C++ only (apart from P2996). An explicit function keeps the cost visible, and opt-in at each throw site. Exceptions thrown by other libraries have no trace, and that is stated rather than approximated.

**Why `caused_by` instead of inspecting the active exception?** The log call receives the exception object, so the link between the event and its cause is explicit. A trace is not attached to an event that merely happens to be logged inside a `catch` block, and nothing depends on `catch (...)`.

**Why only `error` and `fatal`?** Resolving symbols is the expensive part, and a trace is useful when something failed. Lower levels keep their cost model.

**Why separate from `SourceMode`?** They answer different questions: "which statement logged this?" and "how did the program get there?". Each can be enabled per renderer without the other.

**Why only `std::stacktrace`?** No dependency, and no behavior that varies with a third-party library.

## Pitfalls

| Pitfall | Consequence | What to do |
| :------ | :---------- | :--------- |
| `-lstdc++exp` before the sources, or missing | Link error on `std::__stacktrace_impl::_S_current` | Put it after the sources and objects, or use CMake. |
| `LOGCHAIN_WITH_STACKTRACE` defined in some translation units only | The layout of `traced<E>` differs between units: undefined behavior | Define it everywhere; with CMake, link `logchain::logchain`. |
| `-fno-rtti` | Compilation error (`dynamic_cast`) when the feature is enabled | Keep RTTI enabled. |
| A wrapper around `throw_traced` | The first frame is the wrapper; the real throw site comes second | Call `throw_traced` where the failure is detected. |
| No `-g` | Frames without file or line | Compile with `-g`. |
| An exception thrown without `throw_traced` | No trace, even with `caused_by` | Throw it with `throw_traced`. |
| Passing the second argument of `throw_traced` | The trace is whatever you pass | Do not pass it. |

[Advanced](index.md) · Next step [Evaluation Model and Level Filtering](evaluation_model.md)
