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

#pragma once

#include <cstddef>
#include <meta>

// ===========================================================================
// Reflection helpers shared by the JSON and console renderers.
//
// Why not a static array of members (define_static_array)?
//
// GCC PR 127002: the mangling of non-static data members of LOCAL classes
// does not include the enclosing function, so `f1()::Local::x` and
// `f2()::Local::x` get the same mangled name. A reflection of such a member
// that is materialized (define_static_array) or used as a template argument
// is then conflated across the two classes, and splicing it fails with
// "invalid use of non-static data member".
//
// Workaround: never let a member reflection escape a constant evaluation.
// Both helpers below return plain values (a count) or are spliced right
// away at the point of use (member_at), so no reflection of a member is
// ever stored in a static object or passed as a template argument.
//
// Remove this workaround (and go back to a static array if desired) once
// the PR is fixed in the GCC versions we support.
// ===========================================================================

namespace logchain::detail {

template <typename T> consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
}

// Only meant to be used inside a splice: obj.[:member_at<T, I>():]
template <typename T, std::size_t I> consteval std::meta::info member_at() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())[I];
}

} // namespace logchain::detail
