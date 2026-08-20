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
// 3. Sink : seule frontière polymorphique du système (choix runtime)
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
