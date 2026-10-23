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

option(${PROJECT_NAME_UPPER}_BUILD_EXAMPLES "Build ${PROJECT_NAME} examples" ON)

# User-facing option controlling whether unit tests are built.
option(LOGCHAIN_BUILD_TESTS "Build tests" OFF)
option(LOGCHAIN_BUILD_STRESS_TESTS "Build stress tests" OFF)

# Initialize CTest's standard BUILD_TESTING variable before include(CTest).
set(BUILD_TESTING "${LOGCHAIN_BUILD_TESTS}" CACHE BOOL "Build tests")

option(${PROJECT_NAME_UPPER}_FETCH_GTEST
    "Fetch GoogleTest via FetchContent if find_package(GTest) fails" OFF)

option(${PROJECT_NAME_UPPER}_BUILD_DOCS "Build Doxygen documentation" OFF)

option(${PROJECT_NAME_UPPER}_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

option(${PROJECT_NAME_UPPER}_ENABLE_SANITIZERS
    "Enable Address/UndefinedBehavior sanitizers (GCC/Clang only)" OFF)

option(${PROJECT_NAME_UPPER}_ENABLE_THREAD_SANITIZER
    "Enable ThreadSanitizer (GCC/Clang only)" OFF)

option(${PROJECT_NAME_UPPER}_INSTALL "Generate install and packaging targets" ON)

# Stack traces of explicit exceptions (std::stacktrace), see cmake/Stacktrace.cmake.
option(${PROJECT_NAME_UPPER}_WITH_STACKTRACE
    "Enable stack traces for exceptions thrown with throw_traced() (std::stacktrace)" OFF)

set(${PROJECT_NAME_UPPER}_STACKTRACE_LIBRARY "stdc++exp" CACHE STRING
    "Library providing the backend of std::stacktrace: stdc++exp (recent GCC), stdc++_libbacktrace (older GCC), or empty")
