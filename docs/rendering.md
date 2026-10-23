Previous step [Getting Started](getting_started.md) · [Index](index.md) · Next step [Log rotation](log_rotation.md)

# Rendering

## Overview

In `logchain`, a sink defines **where** log events are written, while a renderer defines **how** those events are represented.  
A renderer can therefore be associated with any compatible sink. The same renderer can be used by multiple sinks, and different sinks can use different renderers.  

By default, each sink uses its default renderer:

* `ConsoleSink` uses the human-readable console renderer.
* `FileSink` uses the JSON renderer.

The renderer is selected when creating the target passed to the logger.

## Using the default renderers

The simplest configuration is to let each sink use its default renderer. `target(sink)` creates a target with the default renderer of the sink:

```cpp
ConsoleSink console_sink;
FileSink file_sink{"/tmp/application.log"};

Logger logger{
    target(console_sink),
    target(file_sink)
};
```

With this configuration, console output is human-readable while the file contains structured JSON.

## Selecting a renderer explicitly

A renderer can be explicitly associated with a sink by building the `Target` yourself, renderer first:

```cpp
ConsoleSink console_sink;
FileSink file_sink{"/tmp/application.log"};

Logger logger{
    Target{JsonRenderer{}, console_sink},
    Target{ConsoleRenderer{}, file_sink}
};
```

This configuration deliberately reverses the default renderers:

* `ConsoleSink` now uses `JsonRenderer`.
* `FileSink` now uses `ConsoleRenderer`.

The sink determines the destination, but the renderer determines the representation.

Renderers can also be configured through their options, for example to choose the colors of the console renderer or whether the call site is printed:

```cpp
Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, console_sink}
```

## Example

Consider the following log call:

```cpp
LOG_INFO(logger,
         "Request handled",
         extra(request_id, retry_count));
```

With the default configuration:

```cpp
Logger logger{
    target(console_sink),
    target(file_sink)
};
```

the console receives the human-readable representation, while the file receives the JSON representation.  

With the renderers explicitly swapped:

```cpp
Logger logger{
    Target{JsonRenderer{}, console_sink},
    Target{ConsoleRenderer{}, file_sink}
};
```

the same log event is rendered as JSON on the console and as human-readable text in the file.  
No change to the logging call is required.  
This separation allows the same structured event to be rendered differently depending on its destination.

## Call site and stack traces

Both renderers can print where an event was logged (file, line and function) through their `source` option, and the stack trace of the exception that caused an error through their `stacktrace` option.  
See [Call Site: Source Location and Stack Traces](source_location.md).

Previous step [Getting Started](getting_started.md) · [Index](index.md) · Next step [Log rotation](log_rotation.md)
