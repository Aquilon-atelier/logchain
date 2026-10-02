#
# Copyright (c) 2026 Sylvain Ladoux
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 4.0.0)

project(logchain
    VERSION 1.0.0
    DESCRIPTION "Stack event logger"
    LANGUAGES CXX
)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
    message(WARNING
        "logchain requires GCC >= 16.1 with experimental "
        "reflection support (P2996). Detected compiler: "
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Use a separate build directory, "
        "e.g.: cmake --preset debug"
    )
endif()

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
set(PROJECT_NAMESPACE ${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

# Options.cmake must run before include(CTest): it pre-sets BUILD_TESTING,
# and CMake's option() is a no-op once a cache variable already exists, so
# our default (OFF) wins over CTest's own default (ON).
include(cmake/Options.cmake)
include(CTest)

include(cmake/CompilerOptions.cmake)
include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

# --- Library target ----------------------------------------------------------
add_library(logchain INTERFACE)
add_library(logchain::logchain ALIAS logchain)

target_include_directories(logchain INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(logchain INTERFACE
    -std=c++26
    -freflection
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME ${PROJECT_NAME}
)

apply_project_compiler_options(${PROJECT_NAME})
apply_project_warnings(${PROJECT_NAME})

# --- Subdirectories ------------------------------------------------------------
if(${PROJECT_NAME_UPPER}_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    include(cmake/Testing.cmake)
endif()

if(${PROJECT_NAME_UPPER}_BUILD_DOCS)
    add_subdirectory(docs)
endif()

include(cmake/Install.cmake)
include(cmake/Packaging.cmake)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
