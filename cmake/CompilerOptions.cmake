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

# Target-scoped compiler features and optional sanitizer flags.
# Deliberately avoids global CMAKE_CXX_FLAGS and CMAKE_CXX_STANDARD.
# Settings attached to a target propagate to consumers only through
# PUBLIC or INTERFACE usage requirements.

function(apply_project_compiler_options target)

    target_compile_features(${target} INTERFACE cxx_std_26)

    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        if(${PROJECT_NAME_UPPER}_ENABLE_SANITIZERS)

            # AddressSanitizer + UndefinedBehaviorSanitizer
            target_compile_options(
                ${target}
                INTERFACE
                    -fsanitize=address,undefined
                    -fno-omit-frame-pointer
            )

            target_link_options(
                ${target}
                INTERFACE
                    -fsanitize=address,undefined
            )

        endif()


        if(${PROJECT_NAME_UPPER}_ENABLE_THREAD_SANITIZERS)

            # ThreadSanitizer
            target_compile_options(
                ${target}
                INTERFACE
                    -fsanitize=thread
                    -fno-omit-frame-pointer
            )

            target_link_options(
                ${target}
                INTERFACE
                    -fsanitize=thread
            )

        endif()

    else()

        if(${PROJECT_NAME_UPPER}_ENABLE_SANITIZERS
           OR ${PROJECT_NAME_UPPER}_ENABLE_THREAD_SANITIZER)

            message(WARNING
                "Sanitizers are enabled but compiler "
                "'${CMAKE_CXX_COMPILER_ID}' is not supported; ignoring."
            )

        endif()

    endif()

endfunction()
