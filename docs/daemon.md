Previous step [Call Site: Source Location and Stack Traces](source_location.md) · [Index](index.md) · Next step [Advanced](advanced/index.md)

# Running as a Daemon (systemd)

## Overview

When a program runs as a `systemd` service, systemd connects its standard output and standard error to the **journal** (`journald`). A daemon therefore needs no log file, no rotation policy and no special sink: it writes to the console, and `journalctl` shows the result.

`logchain` adapts to this situation automatically. `ConsoleRenderer` has a `journal` option, of type `JournalMode`, which is `Auto` by default:

* in a terminal, or anywhere that is not the journal, nothing changes;
* when the output is connected to the journal, each line is shaped for it: it carries its **priority**, and the timestamp that the console layout normally prints is left out, because the journal records its own.

The same executable can therefore be started from a shell or as a service, with no code path to select.

## Minimal example

```cpp
#include "logchain/logger.hpp"

#include <string>

using namespace logchain;

ConsoleSink console_sink;

Logger logger{target(console_sink)};

int main() {
    const std::string request_id{"req-A"};
    const int retry_count{3};

    LOG_INFO(logger, "Request handled", extra(request_id, retry_count));
    LOG_ERROR(logger, "Request failed", extra(request_id));
}
```

`target(console_sink)` selects the default renderer of the sink, a `ConsoleRenderer` whose `journal` option is `JournalMode::Auto`. Nothing in the code mentions systemd.

Started from a terminal, the program prints:

```text
2026-11-10T14:02:11.104233Z [INFO ] Request handled  request_id=req-A  retry_count=3
2026-11-10T14:02:11.104301Z [ERROR] Request failed  request_id=req-A
```

Started as a service, the same program writes the following on its standard output:

```text
<6>[INFO ] Request handled  request_id=req-A  retry_count=3
<3>[ERROR] Request failed  request_id=req-A
```

The leading `<6>` and `<3>` are priorities. journald removes them from the message and records them as the priority of the entry, so that `journalctl` shows:

```text
Nov 10 14:02:11 myhost myapp[1234]: [INFO ] Request handled  request_id=req-A  retry_count=3
Nov 10 14:02:11 myhost myapp[1234]: [ERROR] Request failed  request_id=req-A
```

## The service

No special setting is needed. The unit only has to start the program:

```ini
[Unit]
Description=My application

[Service]
ExecStart=/usr/local/bin/myapp
```

This works with the defaults of systemd (`StandardOutput=journal`, `StandardError=journal` and `SyslogLevelPrefix=true`). The priority prefix is only interpreted when `SyslogLevelPrefix=` is true: see [Pitfalls](advanced/journal_mode.md#pitfalls).

## What you get in `journalctl`

Without a priority, systemd gives every line written by a service the same default priority (`info`), and a filter on the priority is useless. With the priority of each event, the usual filters work:

```bash
journalctl -u myapp                 # everything
journalctl -u myapp -p warning      # warnings and more severe events
journalctl -u myapp -p err -f       # follow errors and fatal events
journalctl -u myapp -o verbose      # show PRIORITY and the other fields of each entry
```

The level of the event is mapped to a priority as follows:

| `LogLevel` | Prefix | Priority |
| :--------- | :----: | :------- |
| `trace`, `debug` | `<7>` | debug |
| `info` | `<6>` | info |
| `warning` | `<4>` | warning |
| `error` | `<3>` | err |
| `fatal` | `<2>` | crit |

`-p err` also shows `fatal` events, since `crit` is more severe than `err`.

The level name stays in the text of the line (`[ERROR]`), because `journalctl` does not print the priority in its default output.

## Choosing the mode explicitly

`Auto` is the right choice for a program that is started both ways. The mode can also be set explicitly, with the `journal` option of the renderer:

```cpp
Target{ConsoleRenderer{.journal = JournalMode::Always}, console_sink}
Target{ConsoleRenderer{.journal = JournalMode::Never}, console_sink}
```

| `JournalMode` | Meaning |
| :------------ | :------ |
| `Auto` (default) | Journal layout when the standard output is connected to the journal by systemd. Console layout otherwise. |
| `Always` | Journal layout, whatever the output is connected to. |
| `Never` | Console layout, even under systemd. |

Use `Always` when the output reaches the journal in a way that `Auto` cannot recognize, for example when the program is started with `systemd-cat`. Use `Never` to keep the exact layout of a terminal (timestamp included) in the journal.

## A log file next to the journal

A daemon often does not need anything else, since the journal stores and rotates its entries. If you also want a file, add a second target. Each target keeps its own renderer:

```cpp
ConsoleSink console_sink;
FileSink file_sink{"/var/log/myapp/application.log"};

Logger logger{target(console_sink), target(file_sink)};
```

The journal receives the console layout with priorities, and the file receives JSON, exactly as without systemd. The `journal` option only concerns `ConsoleRenderer`: `JsonRenderer` and `FileSink` are not affected.

## Multi-line events

journald turns each **line** of the output into an entry of its own. When an event spans several lines (a stack trace, or a message that contains a line feed), every line carries the priority of the event, so that all of them are kept by the same `-p` filter:

```text
<3>[ERROR:main.cpp:36] Request failed: connection refused  request_id=req-A
<3>  #0 connect(int) at /home/me/app/service.cpp:16
<3>  #1 main at /home/me/app/main.cpp:34
```

The entries are separate in the journal. Another process writing at the same time can insert its own entries between them.

## What the journal does not receive

This mode writes plain text. The context fields (`request_id=req-A`) are part of the message: journald does not index them, and `journalctl REQUEST_ID=req-A` cannot find them. Search the text instead:

```bash
journalctl -u myapp --grep 'request_id=req-A'
```

A `JsonRenderer` writing to the standard output does not use this mode either: each JSON object is stored as the text of a message, with the default priority.

## Going further

How the journal is detected, the exact layouts, what the mode costs, and the situations where `Auto` does not do what you expect (`std::cerr`, `SyslogLevelPrefix=no`, a wrapper script) are described in [Journal Mode: Output for systemd](advanced/journal_mode.md).

Previous step [Call Site: Source Location and Stack Traces](source_location.md) · [Index](index.md) · Next step [Advanced](advanced/index.md)
