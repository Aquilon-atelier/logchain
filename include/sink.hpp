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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace logchain {

// ===========================================================================
// Sink: the system's only polymorphic boundary (runtime choice)
// ===========================================================================

class Sink {
  public:
    virtual ~Sink() = default;

    virtual void write(std::string_view message) = 0;
};

class ConsoleSink final : public Sink {
  public:
    explicit ConsoleSink(std::ostream &stream = std::cout) : stream_(stream) {}

    void write(std::string_view message) override {
        std::lock_guard lock(mutex_);
        stream_ << message << '\n';
    }

  private:
    std::ostream &stream_;
    std::mutex mutex_;
};

class FileSink final : public Sink {
  public:
    explicit FileSink(std::filesystem::path path) : path_(std::move(path)), stream_(path_, std::ios::app) {
        if (!stream_) {
            throw std::runtime_error("FileSink: impossible d'ouvrir '" + path_.string() + "'");
        }
    }

    void write(std::string_view message) override {
        std::lock_guard lock(mutex_);
        stream_ << message << '\n';
        stream_.flush();
    }

  private:
    std::filesystem::path path_;
    std::mutex mutex_;
    std::ofstream stream_;
};

} // namespace logchain
