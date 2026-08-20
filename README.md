# logchain
C++26 structured logging with persistent context chains and static reflection

## Key concepts

### Reflection-driven

`logchain` uses C++26 static reflection to derive argument names directly from the source code.  
This allows structured arguments to be written without explicitly repeating their names:

```cpp
LOG_INFO(logger,
         "request_handler {}",
         handler,
         extra(user, request_id));
```

The `extra(...)` arguments are automatically associated with their source-level names, producing structured fields such as `user` and `request_id`.  

This keeps logging calls concise while preserving the structure of the logged data.

### Persistent context chains

Logging context is represented as a persistent chain rather than as mutable shared state.  
Each additional context value creates a new node that refers to the existing context. Existing contexts remain unchanged and can safely be shared by multiple execution paths.  
This makes context composition naturally safe for concurrent use and avoids relying on thread-local or globally mutable logging state.  

### Structured logging

A log event is represented as structured data rather than as a pre-formatted string.  
The message and its associated fields are kept separate from their final representation. Sinks can therefore render the same event in different formats without changing the logging call itself.  

For example, the same event can be rendered as human-readable text or serialized as JSON.  

### Thread safety

`logchain` is thread-safe and reentrant. Concurrent calls to `logger::log()` are safe.

Persistent context chains can be shared between threads without synchronization. Sink implementations are responsible for synchronizing access to their underlying resources, such as a shared file.  
The library does not require a global mutable logging context.  


## Examples

## Examples

### Logging with explicit arguments

The core logging API does not require macros.

When argument names are provided explicitly, logging can be used with standard C++ only:

```cpp
logger.log_named(
    LogLevel::info,
    "request_handler {}",
    handler,
    named("user", user),
    named("request_id", request_id));
```

This form is fully explicit and does not rely on the preprocessor.

It is a good choice for projects that prefer to avoid macros or when the field name should deliberately differ from the C++ identifier.

### Logging with source-level argument names

`logchain` also provides a convenience macro that uses C++26 static reflection to derive argument names from the source code:

```cpp
LOG_INFO(logger,
         "request_handler {}",
         handler,
         extra(user, request_id));
```

Here, `extra(user, request_id)` automatically produces fields named after the corresponding C++ identifiers:

```text
user
request_id
```

No field names have to be repeated at the call site.

The macro is only syntactic sugar around the regular logging API. It does not introduce a different logging mechanism or a different event representation.

### Choosing between the two

Both forms produce the same kind of structured log event.

Use the explicit API when:

* you want to avoid macros entirely;
* you want field names to be explicit;
* the field name is intentionally different from the C++ identifier.

Use `extra(...)` when:

* you want concise logging calls;
* the field names should match the C++ identifiers;
* you want to take advantage of C++26 static reflection.

For example, these two calls are equivalent from a structured logging perspective:

```cpp
logger.log_named(
    LogLevel::info,
    "request_handler {}",
    handler,
    named("user", user),
    named("request_id", request_id));
```

and:

```cpp
LOG_INFO(logger,
         "request_handler {}",
         handler,
         extra(user, request_id));
```

The choice is therefore primarily about API style: explicit standard C++ versus reflection-assisted convenience.


## Installation

Please consult [Getting Started](docs/getting_started.md) for further details on building and integrating the library as a third-party.

## Requirements

This library require a compiler that support C++26 reflection.  
Compatible compilers include GCC 16 or later.

This library is header-only.  
It requires no external dependencies.

