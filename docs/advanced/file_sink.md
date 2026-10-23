Previous step [Evaluation Model and Level Filtering](evaluation_model.md) · [Advanced](index.md)

# FileSink: Guarantees and Edge Cases

## In brief

This page describes how `FileSink` behaves when things are not ordinary: several loggers or threads sharing a sink, a log entry larger than the size limit, a failing disk. It also lists what `ConsoleSink` does and does not guarantee.

**Read this page if** you share a `FileSink`, if you write from several threads, or if you need to know how rotation and write errors behave. For configuring rotation, see [Log Rotation](../log_rotation.md).

## Reference

### Construction and ownership

`FileSink` owns one physical log file. It is neither copyable nor movable.

The constructor throws:

| Exception | When |
| :-------- | :--- |
| `std::invalid_argument` | The path is empty; or, with rotation enabled, `max_files` or `max_file_size` is zero. |
| `std::runtime_error` | The path exists but is not a regular file; its size or status cannot be read; it cannot be opened; or **another `FileSink` of the same process already owns it**. |

When the constructor throws, nothing stays reserved: the path is available again.

### Rotation algorithm

Before each write, with rotation enabled, `FileSink` computes the size the file would have after the write (`current size + message size + 1` for the line feed):

* if the file is not empty **and** that size would exceed `max_file_size`, the file is rotated first;
* the entry is then written to the current file.

On rotation:

* `max_files == 1`: the current file is closed and reopened **truncated**. No archive is kept.
* otherwise the oldest archive (`.N`, with `N = max_files - 1`) is removed, the others are shifted by one (`.2` becomes `.3`...), the current file becomes `.1`, and a new empty current file is opened.

The size of an existing file is taken into account when the sink opens it: a `FileSink` appends to an existing file and counts what it already contains.

## Behavior and guarantees

### Thread safety

* `FileSink::write` is protected by a mutex. A `FileSink` can be **shared by several `Logger` instances and used from several threads**. An entry is written in one piece: entries from different threads are never interleaved.
* A rotation is performed under the same mutex, so no entry is written to a half-rotated file.
* The registry of owned paths is protected by its own mutex.

### One owner per path

A path has a single owner **within a process**. The check is made on a normalized key (`weakly_canonical`, which resolves symbolic links and `..` while accepting a file that does not exist yet), so `./app.log`, an absolute path and a symbolic link to the same file are the same file.

The registry lives in the process. Two **processes** writing and rotating the same file are not detected and are not supported: they would interfere with each other's rotation.

### Entries larger than the limit

An entry is never split. If a single entry is larger than `max_file_size`:

* an **empty** file is not rotated just for that entry (rotating would not help): the entry is written, and the file exceeds the limit;
* the **next** entry triggers the rotation.

`max_file_size` is therefore a threshold for rotation, not a hard limit on the size of a file.

### Durability

Each write is flushed to the operating system (`flush()` on the stream) before `write` returns. The entry survives the end of the process, including a crash of the program. `logchain` does not call `fsync`: the entry is not guaranteed to be on disk if the machine itself loses power.

The price is a system call for every event. This is by design, and is the current behavior of the library.

### Errors

`FileSink::write` reports a failure by throwing `std::runtime_error`:

* the stream is in an error state after the write or after the flush;
* a step of the rotation fails (closing, removing, renaming, reopening).

If a rotation fails midway, the state of the files and of the stream is not guaranteed, and the following writes may throw again.

### `ConsoleSink`

* `write` is protected by a mutex and writes the message followed by a line feed on the stream given at construction (`std::cout` by default).
* It does not flush explicitly. When the output appears depends on the stream and on its buffering.
* The mutex belongs to the sink, not to the stream: **two `ConsoleSink` on the same stream do not exclude each other**. Share one sink between loggers.

## Cost

* One mutex acquisition per write.
* One flush (a system call) per entry for `FileSink`.
* The rotation cost (a few renames, bounded by `max_files`) is paid by the write that triggers it, and only then.

## Design rationale

**Why one owner per path?** Rotation moves and truncates files. Two sinks with independent state on the same path would corrupt each other's rotation. Failing at construction is simpler and safer than diagnosing interleaved files afterwards.

**Why a process-wide registry in a header-only library?** An `inline` variable is shared by every translation unit of the program, so the check works without a library to link.

**Why not rotate an empty file?** Rotating a file that contains nothing would only produce an empty archive, and the oversized entry would not fit in the new file either. The entry is written to the current file, and the rotation happens on the next entry.

**Why flush every entry?** A log is most valuable when the program fails. A buffered entry that was never flushed is exactly the one you will miss. The cost is accepted by default.

## Pitfalls

| Pitfall | Consequence | What to do |
| :------ | :---------- | :--------- |
| Two `FileSink` on the same path | `std::runtime_error` at construction | Share a single `FileSink` between loggers. |
| Two processes on the same file with rotation | Interfering rotations | One file per process. |
| Treating `max_file_size` as a hard limit | A file can exceed it by up to one entry | Leave a margin. |
| Relying on the names of archives | Naming is an implementation detail | Do not parse them in your application. |
| Ignoring exceptions of `write` | A full disk or a permission error reaches the caller of the log statement | Decide where to handle them in your application. |
| Two `ConsoleSink` on the same stream | Entries can interleave | Share one `ConsoleSink`. |

Previous step [Evaluation Model and Level Filtering](evaluation_model.md) · [Advanced](index.md)
