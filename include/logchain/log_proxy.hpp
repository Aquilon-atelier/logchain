// Copyright (c) 2026 Sylvain Ladoux
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <logchain/event.hpp>
#include <logchain/log_level.hpp>

#include <utility>

namespace logchain {

// Does not own the logger: `logger_` is a reference. The proxy is intended
// for immediate, transient use:
//
//     if (auto log = logger.debug())
//         log("...", ...);
//
// Do not store it beyond the lifetime of `logger`.
template <LogLevel Level, typename LoggerT> class LogProxy {
  public:
    explicit LogProxy(LoggerT &logger) noexcept : logger_(logger) {}

// A single atomic load (relaxed).
    [[nodiscard]] explicit operator bool() const noexcept { return logger_.is_enabled(Level); }

    // Only call if operator bool() has just returned true.
    // `message` is a Located so that the call site is captured HERE, in the
    // caller's expression: it must not travel through the Args pack first.
    template <typename... Args> void operator()(Located message, Args &&...args) {
        logger_.log_unchecked(Level, message, std::forward<Args>(args)...);
    }

  private:
    LoggerT &logger_;
};

} // namespace logchain