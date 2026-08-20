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

set(EXAMPLES
    basic_example
)

foreach(example IN LISTS EXAMPLES)
    add_executable(${PROJECT_NAME}_${example})

    target_sources(
        ${PROJECT_NAME}_${example}
        PRIVATE
            ${example}.cpp
    )

    target_link_libraries(
        ${PROJECT_NAME}_${example}
        PRIVATE
            ${PROJECT_NAMESPACE}::${PROJECT_NAME}
        pthread
    )

    apply_project_warnings(${PROJECT_NAME}_${example})
endforeach()

