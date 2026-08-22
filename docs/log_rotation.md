# Log Rotation

## Overview

`FileSink` supports automatic log rotation based on the size of the current log file.  
Rotation is configured through `FileSinkSettings` when the sink is created:

```cpp
FileSink sink(FileSinkSettings{
    .path = log_path,
    .max_files = 1,
    .max_file_size = 20
});

Logger logger(target(sink));
```

The settings define:

* `path`: the path of the log file.
* `max_files`: the maximum number of rotated log files retained.
* `max_file_size`: the maximum size of the current log file before rotation.

When the configured size is reached, the current log file is rotated and a new log file is created.

## Configuring rotation

For example:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 3,
    .max_file_size = 1024 * 1024
});

Logger logger(target(sink));
```

This configuration keeps up to three rotated files in addition to the current log file.  
The exact naming of rotated files is an implementation detail and should not be relied upon by applications.

## Disabling rotation

If no maximum file size is configured, the log file is not rotated based on its size.  

For example:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log"
});

Logger logger(target(sink));
```

In this configuration, `FileSink` continuously writes to the same file.

## Multiple loggers

A log file path must be owned by a single `FileSink`.  
Two different loggers must **not** use the same log file path:  

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

## Example

The following example configures a small log file to make rotation easy to observe:

```cpp
FileSink sink(FileSinkSettings{
    .path = "/tmp/application.log",
    .max_files = 1,
    .max_file_size = 20
});

Logger logger(target(sink));

LOG_INFO(logger, "First message");
LOG_INFO(logger, "Second message");
LOG_INFO(logger, "Third message");
```

As the file reaches the configured size, `FileSink` rotates the current file and continues writing to a new `application.log`.  
Only the configured number of rotated files is retained.

Previous step [Rendering](rendering.md)
