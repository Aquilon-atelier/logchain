# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `JournalMode` option (`journal`) for `ConsoleRenderer`: when the standard output is connected to the systemd journal, each line carries its priority (`<N>`) and the timestamp is omitted. See [Running as a Daemon (systemd)](docs/daemon.md).

### Changed

- `ConsoleRenderer` now defaults to `JournalMode::Auto`. The console output changes only for a process whose standard output is connected to the journal; use `.journal = JournalMode::Never` to keep the previous output.

## [1.0.0] - 2026-10-28

### Added

- Initial stable release.
- C++26 static reflection.
- Enable CMake Config mode.
- Enable source location and stack trace.

[Unreleased]: https://github.com/Aquilon-atelier/logchain/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Aquilon-atelier/logchain/releases/tag/v1.0.0
