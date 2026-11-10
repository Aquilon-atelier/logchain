[Advanced](index.md) · Previous step [FileSink: Guarantees and Edge Cases](file_sink.md)

# Journal Mode: Output for systemd

## In brief

Journal mode is an option of `ConsoleRenderer`. When it is active, every line of the console output is prefixed with the **priority** of the event (`<3>`, `<6>`...), and the timestamp is left out. This is the convention of `sd-daemon(3)`: for a process whose standard output is connected to the journal, journald reads the prefix, records it as the priority of the entry and removes it from the message.

It is a **text transformation of the console output**. No socket is opened, `libsystemd` is not used, and the sink is not involved: the same `ConsoleSink` serves a terminal and the journal.

**Read this page if** you need the exact layouts, the way the journal is detected, the cost of the mode, or if `Auto` does not do what you expect. For a short introduction, see [Running as a Daemon](../daemon.md).

## Requirements

* None for `JournalMode::Always` and `JournalMode::Never`: they only change the text of the lines.
* For `JournalMode::Auto` to ever select the journal layout: Linux, and a program started by systemd with its standard output connected to the journal. The detection uses POSIX functions (`getenv`, `fstat`). On Windows, `Auto` behaves as `Never`.
* For the priorities to be recorded by journald: `SyslogLevelPrefix=true` in the unit (it is the default of systemd). See [Pitfalls](#pitfalls).
* No library to link and no compile-time option.

## Reference

### `JournalMode`

```cpp
enum class JournalMode { Auto, Always, Never };

ConsoleRenderer{.journal = JournalMode::Auto}   // the default
```

| `JournalMode` | Journal layout is used... |
| :------------ | :------------------------ |
| `Auto` (default) | when the standard output is connected to the journal by systemd (see [Detection](#detection)). |
| `Always` | always. |
| `Never` | never: the console layout is unchanged. |

`journal` is the last member of `ConsoleRenderer`. As for any aggregate with designated initializers, the members must be written in declaration order: `.colors`, `.source`, `.stacktrace`, `.journal`. Code written before the option existed compiles unchanged.

The option exists on `ConsoleRenderer` only. `JsonRenderer` has no journal mode.

### Priorities

| `LogLevel` | Prefix | syslog name | `journalctl -p` |
| :--------- | :----: | :---------- | :-------------- |
| `trace` | `<7>` | `LOG_DEBUG` | `debug` |
| `debug` | `<7>` | `LOG_DEBUG` | `debug` |
| `info` | `<6>` | `LOG_INFO` | `info` |
| `warning` | `<4>` | `LOG_WARNING` | `warning` |
| `error` | `<3>` | `LOG_ERR` | `err` |
| `fatal` | `<2>` | `LOG_CRIT` | `crit` |

`trace` and `debug` share a priority: the journal has no level below `debug`. `emerg` (0) and `alert` (1) are not used, since they are meant for conditions that concern the whole system rather than one program.

### Detection

When a unit connects the standard output or the standard error of a service to the journal, systemd sets the environment variable `JOURNAL_STREAM` to `<device>:<inode>`, the device and inode numbers of the socket of that stream.

`Auto` selects the journal layout when **all** of the following hold:

1. `JOURNAL_STREAM` is set and has the form `<number>:<number>`, in decimal, with nothing else in it;
2. `fstat` on the standard output (file descriptor 1) succeeds;
3. the device and the inode of the standard output are those of the variable.

The comparison in (3) is what makes the detection reliable. The variable alone is inherited by every child process: a program started by a script of the service whose output was redirected to a file would otherwise print priorities into that file.

The result is computed **once per process**, at the first line rendered with `Auto`, and kept. Changing `JOURNAL_STREAM` or reopening the standard output afterwards is not noticed. A service does not do either.

## Output layout

Without journal mode, the console layouts are unchanged (see [Call Site](../source_location.md#console-layout)). With journal mode, they become:

| `SourceMode` | Layout |
| :----------- | :----- |
| `Never` (default) | `<N>[LEVEL] message  fields` |
| `Short` | `<N>[LEVEL:file:line] message  fields` |
| `Full` | `<N>[LEVEL:path:line] message  fields  function="..."` |

where `LEVEL` is padded as in the console layout when there is no call site (`INFO `), and not padded in the located layouts (`INFO`).

For the same event as in the Guide, with `SourceMode::Short` and then `Never`:

```text
<6>[INFO:main.cpp:18] Request handled  request_id=req-A  retry_count=3
<6>[INFO ] Request handled  request_id=req-A  retry_count=3
```

The timestamp is the only element removed. The level, the call site, the message, the fields and the `function=` field are those of the console layout.

### Several lines

The prefix is added to **every line** of the rendered event, including the lines of a stack trace and the lines of a message that contains line feeds:

```text
<3>[ERROR:main.cpp:36] Request failed: connection refused  request_id=req-A
<3>  #0 connect(int) at /home/me/app/service.cpp:16
<3>  #1 main at /home/me/app/main.cpp:34
```

### Colors

Colors do not depend on journal mode. `ColorMode::Auto` finds that the standard output is not a terminal under systemd and prints no escape sequence. With `ColorMode::Always`, the escape sequences are written after the prefix.

## Behavior and guarantees

* **Nothing changes outside the journal.** With `Auto` and an output that is not the journal, or with `Never`, the output is byte for byte the console layout.
* **The same event always has the same priority.** The mapping is fixed (table above) and does not depend on the output.
* **Every line carries the priority.** An event is never partly prefixed.
* **The sink is not involved.** `ConsoleSink` writes the rendered text and a line feed, as usual. The mode applies to whatever sink the renderer is attached to.
* **Standard C++ and POSIX only.** No compiler extension, no dependency on `libsystemd`.
* **No structured data.** The context fields stay text inside the message. journald does not index them.

## Cost

| When | What is paid |
| :--- | :----------- |
| First line with `Auto` | One `getenv`, and one `fstat` if the variable is set. Once per process. |
| Every line with `Auto` or `Never`, journal layout not selected | One branch. |
| Every line, journal layout | One extra pass over the rendered line, and one allocation, to insert the prefixes. |

## Design rationale

**Why an option of the renderer, and not a sink?** Under systemd, the standard output already is the journal. The only difference is the shape of the lines, which is the job of the renderer. A `ConsoleSink` and a `ConsoleRenderer` are all a daemon needs, and the same binary keeps working in a terminal.

**Why `Auto` by default?** Journald removes the prefix from the message, so the text seen in `journalctl` is that of the console layout, with the right priority. The layout only changes for a process that is really connected to the journal. Everywhere else the output is identical to before.

**Why compare with `fstat` and not just read `JOURNAL_STREAM`?** The variable is inherited by child processes and does not say where *this* standard output goes. The device and the inode do.

**Why omit the timestamp?** journald records the time at which it received the line. Keeping the timestamp of the event in the text would show two times on every line of `journalctl`. `Never` brings it back.

**Why prefix every line?** journald makes one entry per line. A line without a prefix would get the default priority (`info`) and escape a filter such as `-p err`, which is exactly what a stack trace must not do.

**Why not the native journal protocol?** It would add structured fields, at the price of platform-specific code and a different kind of sink. This mode keeps `logchain` portable, and is enough to get priorities.

## Pitfalls

| Pitfall | Consequence | What to do |
| :------ | :---------- | :--------- |
| `SyslogLevelPrefix=no` in the unit | The `<N>` prefix is not interpreted and appears in the message | Remove the setting (the default is `true`), or use `JournalMode::Never`. |
| A `ConsoleSink` built on `std::cerr` | `Auto` examines the standard **output**, not the standard error: it may not select the journal layout | Use `JournalMode::Always`. |
| The program is started with `systemd-cat`, or by a script that connects its output to the journal | No `JOURNAL_STREAM` for this stream: `Auto` selects the console layout | Use `JournalMode::Always`. |
| A script of the unit redirects the output (`myapp > file`) | `Auto` selects the console layout, as intended: the output is not the journal | Nothing. |
| `JournalMode::Always` on a terminal, a file or a `FileSink` | The prefixes are written literally | Use `Always` only for an output that goes to the journal. |
| Relying on the standard error for priorities | systemd gives the same default priority (`info`) to standard output and standard error | Rely on the prefix. |
| A very long line | journald splits lines beyond its `LineMax=` limit | Keep events short. |
| Comparing the exact console output in tests | A test run by a service whose output is the journal gets the journal layout | Set `.journal = JournalMode::Never` in such tests. |
| Expecting `journalctl FIELD=value` to find a context field | Fields are text in the message | Search the text (`journalctl --grep`). |

[Advanced](index.md) · Previous step [FileSink: Guarantees and Edge Cases](file_sink.md)
