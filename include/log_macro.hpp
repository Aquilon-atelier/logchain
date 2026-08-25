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

#include "context.hpp"

namespace logchain {

#define SCL_PARENS ()

#define SCL_EXPAND(...) SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(__VA_ARGS__))))
#define SCL_EXPAND4(...) SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(__VA_ARGS__))))
#define SCL_EXPAND3(...) SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(__VA_ARGS__))))
#define SCL_EXPAND2(...) SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(__VA_ARGS__))))
#define SCL_EXPAND1(...) __VA_ARGS__

#define SCL_FOR_EACH(macro, ...) __VA_OPT__(SCL_EXPAND(SCL_FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define SCL_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, SCL_FOR_EACH_AGAIN SCL_PARENS(macro, __VA_ARGS__))

#define SCL_FOR_EACH_AGAIN() SCL_FOR_EACH_HELPER

#define SCL_NAMED_ARG(x) logchain::detail::make_named(#x, (x))

// extra(...) : pure syntactic sugar, expands BEFORE LOG_xxx.
// extra(user, request_id) -> make_extra(make_named("user", user), make_named("request_id", request_id))
#define extra(...) ::logchain::detail::make_extra(SCL_FOR_EACH(SCL_NAMED_ARG, __VA_ARGS__))

// LOG / LOG_TRACE / LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR / LOG_FATAL
// used to be defined here as thin, unconditional wrappers around
// `(logger).log(...)`. They now live in log_level_filtering.hpp instead
// (LOGCHAIN_LOG* and their short aliases), which checks is_enabled()
// *before* the variadic message/args are ever evaluated — a property a
// plain macro-to-member-function forward like the old LOG(...) here
// could never have, since C++ always evaluates function-call arguments
// eagerly regardless of what the callee does internally. Keeping both
// definitions around is not just redundant: they use the SAME macro
// names with DIFFERENT parameter lists, which is an outright macro
// redefinition the moment both headers are included together — not a
// style choice to leave open.

} // namespace logchain
