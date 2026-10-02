# Log Rotation

## Overview

`FileSink` supports automatic log rotation based on the size of the current log file.  
Rotation is enabled by default and can be configured through `FileSinkSettings`:

```cpp
FileSink sink(FileSinkSettings{
    .path = log_path,
    .max_files = 5,
    .max_file_size = 10 * 1024 * 1024
});

Logger logger(target(sink));
```

The settings define:

* `path`: the path of the log file.
* `rotation`: whether automatic log rotation is enabled. It defaults to `Rotation::enabled`.
* `max_files`: the maximum number of log files retained, including the current file.
* `max_file_size`: the maximum size of the current log file before rotation.

When rotation is enabled and the configured size is reached, the current log file is rotated and a new log file is created.

## Default settings

`FileSink` uses the following default rotation settings:

* `rotation = Rotation::enabled`
* `max_files = 5`
* `max_file_size = 10 * 1024 * 1024` (10 MiB)

The default settings apply to both constructors:

```cpp
FileSink sink("/tmp/application.log");
```

and:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log"
});
```

These two forms are equivalent.

With the default settings, the files retained by `FileSink` are:

```text
application.log
application.log.1
application.log.2
application.log.3
application.log.4
```

`max_files` includes the current file.

## Configuring rotation

Rotation settings can be customized without explicitly specifying `rotation`, since rotation is enabled by default.

For example:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 3,
    .max_file_size = 1024 * 1024
});

Logger logger(target(sink));
```

This configuration keeps up to three files in total:

```text
application.log
application.log.1
application.log.2
```

The exact naming of rotated files is an implementation detail and should not be relied upon by applications.

## Disabling rotation

Rotation can be explicitly disabled with `Rotation::disabled`:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .rotation = Rotation::disabled
});

Logger logger(target(sink));
```

When rotation is disabled, `max_files` and `max_file_size` have no effect.  
The current log file is allowed to grow without automatic size-based rotation.

## `max_files = 1`

Setting `max_files` to `1` does **not** disable rotation.

It means that rotation is still performed when the size threshold is reached, but no archived file is retained:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 1,
    .max_file_size = 1024 * 1024
});
```

When the threshold is reached, the current file is truncated and writing continues in the new current file.

Use `Rotation::disabled` when rotation itself should be disabled.

## Multiple loggers

A log file path must be owned by a single `FileSink`.  
Two different `FileSink` instances must not use the same log file path:

```cpp
FileSink sink1(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 3,
    .max_file_size = 1024 * 1024
});

FileSink sink2(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 3,
    .max_file_size = 1024 * 1024
});
```

This configuration is not supported.

In particular, rotation requires a single owner for the log file. Multiple independent sinks writing to the same path could otherwise interfere with each other's writes and rotation state.  
If multiple loggers need to write to the same log file, they should share the same `FileSink`:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 3,
    .max_file_size = 1024 * 1024
});

Logger logger1(target(sink));
Logger logger2(target(sink));
```

The `FileSink` is then shared by both loggers, providing a single point of synchronization and rotation management.


Previous step [Rendering](rendering.md)
