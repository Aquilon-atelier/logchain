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
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "size.hpp"

namespace logchain {

// ===========================================================================
// Sink: the system's only polymorphic boundary (runtime choice)
// ===========================================================================

class Sink {
  public:
    virtual ~Sink() = default;

    virtual void write(std::string_view message) = 0;
};

// ===========================================================================
// ConsoleSink
// ===========================================================================

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

// ===========================================================================
// FileSink
//
// A FileSink owns one physical log file and its rotation state.
//
// A single FileSink may safely be shared by multiple Logger instances.
// Two distinct FileSink instances may NOT own the same log file at the
// same time.
//
// Rotation policy:
//   - max_files includes the current file.
//   - max_files == 1 means no archive is retained.
//   - max_file_size is the threshold at which the next write triggers
//     rotation.
//   - archived files are named:
//         file.log.1
//         file.log.2
//         ...
//
// Defaults:
//   - rotation      = enabled
//   - max_files     = 5
//   - max_file_size = 10 MiB
//
// Rotation can be explicitly disabled with:
//   FileSinkSettings{
//       .path = "...",
//       .rotation = Rotation::disabled
//   }
// ===========================================================================

enum class Rotation { enabled, disabled };
struct FileSinkSettings {
    std::filesystem::path path;
    Rotation rotation = Rotation::enabled;
    std::size_t max_files = 5;
    std::size_t max_file_size = 10 * MiB;
};

namespace detail {

// ---------------------------------------------------------------------------
// FileSink registry
//
// The registry prevents two distinct FileSink instances from opening the
// same logical path simultaneously.
//
// These are inline variables because logchain is header-only.
// ---------------------------------------------------------------------------

inline std::mutex file_sink_registry_mutex;
inline std::unordered_set<std::string> file_sink_registry;

// ---------------------------------------------------------------------------
// Produce a stable key for a filesystem path.
//
// weakly_canonical() resolves existing path components (including symlinks)
// while still accepting a file that does not exist yet. If canonicalization
// fails, fall back to absolute + lexically_normal.
// ---------------------------------------------------------------------------

inline std::string file_sink_path_key(const std::filesystem::path &path) {
    std::error_code ec;

    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.string();
    }

    ec.clear();

    const auto absolute = std::filesystem::absolute(path, ec);
    if (!ec) {
        return absolute.lexically_normal().string();
    }

    // This should only be reached for unusual filesystem failures.
    return path.lexically_normal().string();
}

// ---------------------------------------------------------------------------
// Reserve a path in the FileSink registry.
//
// Returns the normalized key so the FileSink can release exactly the same
// entry when it is destroyed.
// ---------------------------------------------------------------------------

inline std::string reserve_file_sink_path(const std::filesystem::path &path) {
    const std::string key = file_sink_path_key(path);

    std::lock_guard lock(file_sink_registry_mutex);

    if (!file_sink_registry.insert(key).second) {
        throw std::runtime_error("FileSink: le fichier '" + path.string() + "' est deja utilise par un autre FileSink");
    }

    return key;
}

inline void release_file_sink_path(const std::string &key) noexcept {
    std::lock_guard lock(file_sink_registry_mutex);
    file_sink_registry.erase(key);
}

} // namespace detail

class FileSink final : public Sink {
  public:
    // -----------------------------------------------------------------------
    // Backward-compatible constructor.
    //
    // Existing code:
    //
    //     FileSink sink("application.log");
    //
    // automatically gets the default rotation policy.
    // -----------------------------------------------------------------------

    explicit FileSink(std::filesystem::path path) : FileSink(FileSinkSettings{.path = std::move(path)}) {}

    // -----------------------------------------------------------------------
    // Configurable constructor.
    // -----------------------------------------------------------------------

    explicit FileSink(FileSinkSettings settings)
        : path_(std::move(settings.path)), rotation_(settings.rotation), max_files_(settings.max_files),
          max_file_size_(settings.max_file_size), registry_key_(detail::reserve_file_sink_path(path_)) {
        try {
            validate_settings();
            open_existing_file();
        } catch (...) {
            detail::release_file_sink_path(registry_key_);
            throw;
        }
    }

    ~FileSink() override {
        stream_.close();
        detail::release_file_sink_path(registry_key_);
    }

    FileSink(const FileSink &) = delete;
    FileSink &operator=(const FileSink &) = delete;

    FileSink(FileSink &&) = delete;
    FileSink &operator=(FileSink &&) = delete;

    void write(std::string_view message) override {
        std::lock_guard lock(mutex_);

        const std::size_t bytes = message.size() + 1; // message + '\n'

        // Do not rotate an empty file just because a single log entry is
        // larger than max_file_size. The entry is written to the new/current
        // file and that file may therefore temporarily exceed the limit.
        if (rotation_ == Rotation::enabled && current_size_ > 0 && current_size_ + bytes > max_file_size_) {
            rotate();
        }

        stream_ << message << '\n';

        if (!stream_) {
            throw std::runtime_error("FileSink: erreur lors de l'ecriture dans '" + path_.string() + "'");
        }

        stream_.flush();

        if (!stream_) {
            throw std::runtime_error("FileSink: erreur lors du flush de '" + path_.string() + "'");
        }

        current_size_ += bytes;
    }

  private:
    void validate_settings() const {
        if (path_.empty()) {
            throw std::invalid_argument("FileSink: le chemin du fichier ne peut pas etre vide");
        }

        if (rotation_ == Rotation::enabled) {
            if (max_files_ == 0) {
                throw std::invalid_argument("FileSink: max_files doit etre superieur a zero");
            }

            if (max_file_size_ == 0) {
                throw std::invalid_argument("FileSink: max_file_size doit etre superieur a zero");
            }
        }
    }

    void open_existing_file() {
        std::error_code ec;

        if (std::filesystem::exists(path_, ec) && !ec) {
            if (!std::filesystem::is_regular_file(path_, ec) || ec) {
                throw std::runtime_error("FileSink: '" + path_.string() + "' n'est pas un fichier regulier");
            }

            current_size_ = std::filesystem::file_size(path_, ec);
            if (ec) {
                throw std::runtime_error("FileSink: impossible de determiner la taille de '" + path_.string() +
                                         "': " + ec.message());
            }
        } else if (ec) {
            throw std::runtime_error("FileSink: impossible d'acceder a '" + path_.string() + "': " + ec.message());
        } else {
            current_size_ = 0;
        }

        stream_.open(path_, std::ios::app);

        if (!stream_) {
            throw std::runtime_error("FileSink: impossible d'ouvrir '" + path_.string() + "'");
        }
    }

    std::filesystem::path archive_path(std::size_t index) const {
        return std::filesystem::path(path_.string() + "." + std::to_string(index));
    }

    void remove_if_exists(const std::filesystem::path &path) {
        std::error_code ec;
        std::filesystem::remove(path, ec);

        if (ec) {
            throw std::runtime_error("FileSink: impossible de supprimer '" + path.string() + "': " + ec.message());
        }
    }

    void rename_file(const std::filesystem::path &from, const std::filesystem::path &to) {
        std::error_code ec;
        std::filesystem::rename(from, to, ec);

        if (ec) {
            throw std::runtime_error("FileSink: impossible de renommer '" + from.string() + "' vers '" + to.string() +
                                     "': " + ec.message());
        }
    }

    void rotate() {
        // The stream must be closed before renaming the current file.
        stream_.close();

        if (!stream_) {
            throw std::runtime_error("FileSink: impossible de fermer '" + path_.string() + "' avant rotation");
        }

        if (max_files_ == 1) {
            // No archive is retained. Truncate the current file and reopen it.
            stream_.open(path_, std::ios::out | std::ios::trunc);

            if (!stream_) {
                throw std::runtime_error("FileSink: impossible de recreer '" + path_.string() + "' apres rotation");
            }

            current_size_ = 0;
            return;
        }

        // Drop the oldest archive first.
        remove_if_exists(archive_path(max_files_ - 1));

        // Shift:
        //
        //   .3 -> .4
        //   .2 -> .3
        //   .1 -> .2
        //
        // Existing destination files were removed just before each rename.
        for (std::size_t i = max_files_ - 1; i > 1; --i) {
            const auto from = archive_path(i - 1);
            const auto to = archive_path(i);

            std::error_code exists_ec;
            const bool exists = std::filesystem::exists(from, exists_ec);

            if (exists_ec) {
                throw std::runtime_error("FileSink: impossible de verifier '" + from.string() +
                                         "': " + exists_ec.message());
            }

            if (exists) {
                remove_if_exists(to);
                rename_file(from, to);
            }
        }

        // Current file -> .1
        {
            const auto first_archive = archive_path(1);

            std::error_code exists_ec;
            const bool exists = std::filesystem::exists(first_archive, exists_ec);

            if (exists_ec) {
                throw std::runtime_error("FileSink: impossible de verifier '" + first_archive.string() +
                                         "': " + exists_ec.message());
            }

            if (exists) {
                remove_if_exists(first_archive);
            }

            rename_file(path_, first_archive);
        }

        // Open a fresh current file.
        stream_.open(path_, std::ios::out | std::ios::trunc);

        if (!stream_) {
            throw std::runtime_error("FileSink: impossible d'ouvrir le nouveau fichier '" + path_.string() +
                                     "' apres rotation");
        }

        current_size_ = 0;
    }

    std::filesystem::path path_;

    Rotation rotation_;
    std::size_t max_files_;
    std::size_t max_file_size_;

    std::size_t current_size_ = 0;

    std::mutex mutex_;
    std::ofstream stream_;

    // The normalized path held in the global registry.
    std::string registry_key_;
};

} // namespace logchain
