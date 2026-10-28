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

### Explicit API

The core logging API does not require macros. Argument names can be provided explicitly using `named()`:

```cpp
logger.log_named(
    LogLevel::info,
    "request_handler {}",
    handler,
    named("user", user),
    named("request_id", request_id));
```

This form uses standard C++ only and gives full control over the names of structured fields.

### Reflection-assisted API

For applications that prefer a more concise syntax, `logchain` provides macros that use C++26 static reflection to derive argument names from the source code:

```cpp
LOG_INFO(logger,
         "request_handler {}",
         handler,
         extra(user, request_id));
```

Here, `extra(user, request_id)` automatically uses the source-level identifiers as field names.

The macro-based API is optional. Developers who prefer to avoid macros can use the explicit API without giving up structured logging.


## Installation

Please consult [Getting Started](docs/getting_started.md) for further details on building and integrating the library as a third-party.

## Requirements

This library require a compiler that support C++26 reflection.  
Compatible compilers include GCC 16 or later.

This library is header-only.  
It requires no external dependencies.

