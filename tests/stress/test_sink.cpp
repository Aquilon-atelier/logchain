// test_sink.cpp
//
// Validation du Sink : écriture réelle + concurrence. Reste séparé de
// context_binding_demo.cpp (délibérément) pour garder la démo lisible.
//
// Compilation normale :
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread -Iinclude -o test_sink src/test_sink.cpp
// Compilation TSan :
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -fsanitize=thread -g -O1 -pthread -Iinclude -o test_sink_tsan
//   src/test_sink.cpp

#include "logger.hpp"
#include "sink.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace logchain;

int main() {
    namespace fs = std::filesystem;
    const fs::path log_path = "sink_test_output.log";
    fs::remove(log_path);

    // --- 1. Fonctionnel : écriture fichier + relecture, contenu exact -----
    {
        FileSink file_sink(log_path);
        Logger file_logger(file_sink);

        std::string user{"bob"};
        int retry_count{3};
        file_logger.log("premiere ligne", user);
        file_logger.log("deuxieme ligne", retry_count);
    }
    {
        std::ifstream in(log_path);
        std::string line1, line2;
        std::getline(in, line1);
        std::getline(in, line2);
        assert(line1 == R"({"message":"premiere ligne","fields":["bob"]})");
        assert(line2 == R"({"message":"deuxieme ligne","fields":[3]})");
        std::cout << "Fichier relu correctement :\n  " << line1 << "\n  " << line2 << "\n";
    }

    // --- 2. Concurrence : N threads, chacun M logs, un seul FileSink ------
    //        partagé. On vérifie qu'aucune ligne n'est tronquée / fusionnée
    //        et que chaque ligne est un objet JSON bien formé pour ce site
    //        d'appel (le mutex vit dans FileSink, cf. spec §9.2).
    {
        fs::remove(log_path);
        FileSink shared_sink(log_path);

        constexpr int kThreads = 8;
        constexpr int kLogsPerThread = 500;

        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&shared_sink, t] {
                Logger logger(shared_sink); // un Logger par thread, un seul Sink
                for (int i = 0; i < kLogsPerThread; ++i) {
                    logger.log("thread-message", t, i);
                }
            });
        }
        for (auto &th : threads)
            th.join();

        const std::string prefix = R"({"message":"thread-message","fields":[)";

        std::ifstream in(log_path);
        std::size_t line_count = 0;
        std::string line;
        while (std::getline(in, line)) {
            // Ligne bien formée : pas de parsing JSON complet nécessaire ici,
            // juste la preuve qu'aucune écriture concurrente n'a coupé ou
            // entrelacé une ligne avec une autre.
            assert(line.rfind(prefix, 0) == 0);
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
