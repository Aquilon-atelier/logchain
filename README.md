# logchain
C++26 structured logging with persistent context chains and static reflection

## Key concepts

### Reflection-driven

TODO: Explain

### Thread safety

`logchain` is thread-safe and reentrant. Concurrent calls to `logger::log()` are safe.


## Examples



## Installation

Please consult [Getting Started](docs/getting_started.md) for further details on building and integrating the library as a third-party.

## Requirements

This library require a compiler that support C++26 reflection.  
Compatible compilers include GCC 16 or later.

This library is header-only.  
It requires no external dependencies.

