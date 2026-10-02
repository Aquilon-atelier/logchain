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

function(apply_project_warnings target)
    set(MSVC_WARNINGS /W4 /permissive-)
    set(CLANG_GCC_WARNINGS
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wcast-align -Wunused -Woverloaded-virtual -Wnull-dereference
        -Wdouble-promotion -Wformat=2
    )

    if(${PROJECT_NAME_UPPER}_WARNINGS_AS_ERRORS)
        list(APPEND MSVC_WARNINGS /WX)
        list(APPEND CLANG_GCC_WARNINGS -Werror)
    endif()

    if(MSVC)
        target_compile_options(${target} INTERFACE ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} INTERFACE ${CLANG_GCC_WARNINGS})
    endif()
endfunction()
