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

# Optional std::stacktrace support (see include/logchain/stacktrace.hpp).
#
#   LOGCHAIN_WITH_STACKTRACE=ON         enable it
#   LOGCHAIN_STACKTRACE_LIBRARY=<lib>   library providing the backend of
#                                       std::stacktrace (default: stdc++exp;
#                                       empty: none is needed)
#
# When enabled, a small program using std::stacktrace is compiled AND linked
# once. If that fails (standard library without std::stacktrace, libstdc++
# configured without --enable-libstdcxx-backtrace, wrong library name),
# configuration stops with an explicit error: the feature was asked for, so it
# is never silently disabled.

include_guard(GLOBAL)

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

function(logchain_configure_stacktrace target)

    if(NOT ${PROJECT_NAME_UPPER}_WITH_STACKTRACE)
        return()
    endif()

    set(_library "${${PROJECT_NAME_UPPER}_STACKTRACE_LIBRARY}")

    # One cached result per library name, so that changing the library after a
    # failed configuration re-runs the check instead of reusing the failure.
    string(MAKE_C_IDENTIFIER "${PROJECT_NAME_UPPER}_STACKTRACE_CHECK_${_library}" _result_var)

    cmake_push_check_state(RESET)

    # std::stacktrace is C++23: the check does not need the project's C++26.
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_REQUIRED_LIBRARIES ${_library})

    check_cxx_source_compiles([=[
#include <stacktrace>
#ifndef __cpp_lib_stacktrace
#error std::stacktrace is not available
#endif
int main() {
    const auto trace = std::stacktrace::current();
    return trace.empty() || trace[0].description().empty() ? 1 : 0;
}
]=] ${_result_var})

    cmake_pop_check_state()

    if(NOT ${_result_var})
        message(FATAL_ERROR
            "${PROJECT_NAME_UPPER}_WITH_STACKTRACE is ON, but a program using std::stacktrace "
            "does not compile and link with ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
            "(library: '${_library}').\n"
            "  - The standard library must provide std::stacktrace: libstdc++ must have been "
            "configured with --enable-libstdcxx-backtrace.\n"
            "  - libstdc++ needs a backend library: stdc++exp on recent GCC, stdc++_libbacktrace "
            "on older ones. Set ${PROJECT_NAME_UPPER}_STACKTRACE_LIBRARY accordingly "
            "(empty if no library is needed).\n"
            "The compiler output is in CMakeFiles/CMakeError.log."
        )
    endif()

    target_compile_definitions(${target} INTERFACE LOGCHAIN_WITH_STACKTRACE)

    if(_library)
        target_link_libraries(${target} INTERFACE ${_library})
    endif()

    message(STATUS "${PROJECT_NAME}: std::stacktrace enabled (library: '${_library}')")

endfunction()
