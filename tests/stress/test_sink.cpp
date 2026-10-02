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

//
// Sink validation: actual writing + concurrency.
//

#include "logger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using namespace logchain;

int main() {
    namespace fs = std::filesystem;
    const fs::path log_path = "sink_test_output.log";
    fs::remove(log_path);

    // Functional: file writing + reading back, exact content -----
    {
        FileSink file_sink(log_path);
        Logger file_logger(target(file_sink));

        std::string user{"bob"};
        int retry_count{3};
        LOG(file_logger, LogLevel::info, "premiere ligne", user);
        LOG(file_logger, LogLevel::info, "deuxieme ligne", retry_count);
    }
    {
        std::ifstream in(log_path);
        std::string line1, line2;
        std::getline(in, line1);
        std::getline(in, line2);
        const std::regex line1_re(
            R"(\{"timestamp":"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}Z","level":"INFO","message":"premiere ligne","user":"bob"\})");

        const std::regex line2_re(
            R"(\{"timestamp":"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}Z","level":"INFO","message":"deuxieme ligne","retry_count":3\})");

        assert(std::regex_match(line1, line1_re));
        assert(std::regex_match(line2, line2_re));
        std::cout << "Fichier relu correctement :\n  " << line1 << "\n  " << line2 << "\n";
    }

    // Concurrency: N threads, each with M logs, a single shared
    // FileSink. We verify that no line is truncated or merged
    // and that each line is a well-formed JSON object for that
    // call site (the mutex resides in FileSink; see spec §9.2).
    {
        fs::remove(log_path);
        FileSink shared_sink(log_path);

        constexpr int kThreads = 8;
        constexpr int kLogsPerThread = 500;

        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&shared_sink, t] {
                Logger logger(target(shared_sink)); // one Logger per thread, a single Sink
                for (int i = 0; i < kLogsPerThread; ++i) {
                    LOG(logger, LogLevel::info, "thread-message", t, i);
                }
            });
        }
        for (auto &th : threads)
            th.join();
        const std::regex line_re(
            R"(\{"timestamp":"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}Z","level":"INFO","message":"thread-message","t":\d+\,"i":\d+\})");

        std::ifstream in(log_path);
        std::size_t line_count = 0;
        std::string line;

        while (std::getline(in, line)) {
            // Each line must constitute a complete JSON event.
            // The timestamp is structurally validated, but its value is dynamic.
            assert(std::regex_match(line, line_re));
            assert(!line.empty() && line.back() == '}');

            ++line_count;
        }

        assert(line_count == static_cast<std::size_t>(kThreads * kLogsPerThread));

        std::cout << "Concurrence : " << line_count << " lignes écrites par 8 threads, aucune corruption.\n";
    }

    fs::remove(log_path);
    std::cout << "OK — toutes les vérifications du sink sont passées.\n";
    return 0;
}
