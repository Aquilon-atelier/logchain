# Rendering

## Overview

In `logchain`, a sink defines **where** log events are written, while a renderer defines **how** those events are represented.  
A renderer can therefore be associated with any compatible sink. The same renderer can be used by multiple sinks, and different sinks can use different renderers.  

By default, each sink uses its default renderer:

* `ConsoleSink` uses the human-readable console renderer.
* `FileSink` uses the JSON renderer.

The renderer is selected when creating the target passed to the logger.

## Using the default renderers

The simplest configuration is to let each sink use its default renderer:

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

A renderer can be explicitly associated with a sink:

```cpp
ConsoleSink console_sink;
FileSink file_sink{"/tmp/application.log"};

Logger logger{
    target(console_sink, JsonRenderer{}),
    target(file_sink, ConsoleRenderer{})
};
```

This configuration deliberately reverses the default renderers:

* `ConsoleSink` now uses `JsonRenderer`.
* `FileSink` now uses `ConsoleRenderer`.

The sink determines the destination, but the renderer determines the representation.

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
    target(console_sink, JsonRenderer{}),
    target(file_sink, ConsoleRenderer{})
};
```

the same log event is rendered as JSON on the console and as human-readable text in the file.  
No change to the logging call is required.  
This separation allows the same structured event to be rendered differently depending on its destination.


Previous step [Getting Started](getting_started.md)  
Next step [Log rotation](log_rotation.md)
