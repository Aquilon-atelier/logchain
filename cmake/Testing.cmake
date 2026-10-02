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

enable_testing()

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    if(${PROJECT_NAME_UPPER}_FETCH_GTEST)
        message(STATUS
            "GTest not found via find_package(); fetching via FetchContent "
            "(${PROJECT_NAME_UPPER}_FETCH_GTEST=ON)."
        )
        include(FetchContent)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(googletest)
    else()
        message(FATAL_ERROR
            "GTest not found via find_package(). Install it or re-configure "
            "with -D${PROJECT_NAME_UPPER}_FETCH_GTEST=ON to allow a "
            "FetchContent fallback."
        )
    endif()
endif()

include(GoogleTest)

add_subdirectory(tests)
