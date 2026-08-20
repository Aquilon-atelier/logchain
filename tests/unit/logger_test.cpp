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

#include "logger.hpp"

#include <gtest/gtest.h>

using namespace logchain;

TEST(console_sink, mem_check) {

    std::string user{"bob"};
    std::string request_id{"req-A"};
    int retry_count{3};

    // --- 1. Contrat de base : LOG capture les noms depuis le texte source -
    ConsoleSink console_sink;
    Logger logger(make_target(console_sink));
    LOG(logger, "Request handled", user, request_id, retry_count);
    // Sortie console attendue :
    // {"message":"Request handled","user":"bob","request_id":"req-A","retry_count":3}

    // --- 2. Preuve : immutabilité + partage mémoire (DAG), noms explicites -
    // L'API bas niveau (Context::bind) exige toujours un nom ; ici on le
    // fournit à la main puisqu'on n'appelle pas via la macro LOG.
    auto root = root_context();
    auto with_user = root.bind("user", user);

    auto request_a = with_user.bind("request_id", request_id).bind("retry_count", retry_count);
    auto request_b = with_user.bind("request_id", std::string{"req-B"}).bind("retry_count", 0);

    const void *user_node_via_a = request_a.underlying_node()->parent->parent.get();
    const void *user_node_via_b = request_b.underlying_node()->parent->parent.get();
    assert(user_node_via_a == user_node_via_b);
    std::cout << "Partage mémoire du nœud User confirmé (adresse " << user_node_via_a << ")\n";

    assert(with_user.underlying_node()->value == "bob"); // with_user inchangé
    std::cout << "with_user (inchangé) : " << with_user.underlying_node()->value << "\n";
}

TEST(file_sink, log) {
    namespace fs = std::filesystem;

    std::string user{"bob"};
    std::string request_id_a{"req-A"};
    std::string request_id_b{"req-B"};
    int retry_count{3};

    // --- 5.3 Sink réel : écriture fichier + relecture ----------------------
    const fs::path log_path = "demo_output.log";
    fs::remove(log_path);
    {
        FileSink file_sink(log_path);
        Logger file_logger(make_target(file_sink));
        LOG(file_logger, "requete traitee", user, request_id_a, retry_count);
        LOG(file_logger, "requete traitee", user, request_id_b, 0);
    }

    std::ifstream in(log_path);
    std::string line;
    std::size_t count = 0;
    while (std::getline(in, line)) {
        std::cout << "  ligne relue : " << line << "\n";
        ++count;
    }
    assert(count == 2);
    fs::remove(log_path);
}

TEST(multiple_sink, explicit_target_decl) {

    struct User {
        std::string firstname;
        std::string lastname;
        unsigned int age;
    };

    namespace fs = std::filesystem;
    const std::filesystem::path log_path{"/tmp/application.log"};

    ConsoleSink console_sink;
    FileSink file_sink{log_path};

    Logger logger{Target{ConsoleRenderer{}, console_sink}, Target{JsonRenderer{}, file_sink}};

    User albert_londre{"Albert", "Londre", 65};
    User romain_garry{"Romain", "Garry", 53};

    LOG(logger, "Request handled", albert_londre, romain_garry);
    fs::remove(log_path);
}
