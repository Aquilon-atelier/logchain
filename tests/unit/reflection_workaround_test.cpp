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

// Regression tests for the workaround of GCC PR 127002.
//
// The bug: the mangling of non-static data members of LOCAL classes does not
// include the enclosing function, so reflection metadata materialized with
// define_static_array (or used as a template argument) is conflated across
// distinct local classes that share the same name and layout.
//
// The workaround (logchain/detail/members.hpp): member reflections never
// escape a constant evaluation; they are spliced right where they are used.
//
// Every test below uses several LOCAL classes with the SAME name (LocalUser),
// because that is the only situation the bug affects. A regression shows up
// either as a compile error ("invalid use of non-static data member") or as a
// member value read from the wrong class.
//
// The scenarios come from the manual probes tests/manual_tests/type_id_<nn>.cpp:
//
//   type_id_9  / 10 / 12 : same name, one member, different values
//   type_id_11           : owner of a member reflection
//   type_id_13 / 16      : iteration over the members (template for)
//   type_id_14           : member count, same name and same layout
//   type_id_15           : member reflected by name (^^T::age)
//   type_id_17           : same name, different layouts
//   type_id_18           : same name, same layout, same values
//   type_id_19 / 20      : namespace scope (control) vs local classes
//   type_id_21           : the workaround itself
//
// Expected line numbers are not involved here: only rendered values are checked.

#include <gtest/gtest.h>

#include <logchain/detail/members.hpp>
#include <logchain/logger.hpp>

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace logchain;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct CaptureSink final : Sink {
    std::vector<std::string> lines;

    void write(std::string_view message) override { lines.emplace_back(message); }

    [[nodiscard]] const std::string &last() const { return lines.back(); }
};

bool contains(const std::string &s, std::string_view needle) { return s.find(needle) != std::string::npos; }

// Renders `value` as the context field "user" and returns the whole line.
template <typename T> std::string json_of(const T &value) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{}, sink}};

    lg.log(LogLevel::info, "m", named("user", value));

    return sink.last();
}

template <typename T> std::string console_of(const T &value) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    lg.log(LogLevel::info, "m", named("user", value));

    return sink.last();
}

// Direct access to the helpers of the workaround, the way the renderers use them.
template <typename T, std::size_t I> std::string member_name() {
    constexpr auto name = std::define_static_string(std::meta::identifier_of(detail::member_at<T, I>()));

    return std::string(name);
}

template <typename T, std::size_t I> const auto &value_at(const T &obj) { return obj.[:detail::member_at<T, I>():]; }

template <typename T, std::size_t I> consteval bool has_name(std::string_view expected) {
    return std::meta::identifier_of(detail::member_at<T, I>()) == expected;
}

template <typename T, std::size_t I> consteval bool owner_is_t() {
    return std::meta::parent_of(detail::member_at<T, I>()) == ^^T;
}

// Only a bool leaves the constant evaluation: no reflection is stored.
template <typename T, typename U, std::size_t I> consteval bool members_differ() {
    return detail::member_at<T, I>() != detail::member_at<U, I>();
}

// ---------------------------------------------------------------------------
// Local classes. Each factory declares its own `LocalUser` (or other name where
// the test says so), exactly like the reproducer of the bug report.
// ---------------------------------------------------------------------------

// Same name, same layout, different values (type_id_9/10/12/19/21).
auto make_a() {
    struct LocalUser {
        int age = 42;
        int score = 7;
    };
    return LocalUser{};
}

auto make_b() {
    struct LocalUser {
        int age = 84;
        int score = 11;
    };
    return LocalUser{};
}

// Same name, same layout, same values (type_id_18).
auto make_same_1() {
    struct LocalUser {
        int age = 42;
        int score = 7;
    };
    return LocalUser{};
}

auto make_same_2() {
    struct LocalUser {
        int age = 42;
        int score = 7;
    };
    return LocalUser{};
}

// Same name, different number of members (type_id_17).
auto make_two_members() {
    struct LocalUser {
        int age = 42;
        int score = 7;
    };
    return LocalUser{};
}

auto make_one_member() {
    struct LocalUser {
        int age = 84;
    };
    return LocalUser{};
}

// Same name, same member name, different member type.
auto make_int_value() {
    struct LocalUser {
        int value = 5;
    };
    return LocalUser{};
}

auto make_string_value() {
    struct LocalUser {
        std::string value = "five";
    };
    return LocalUser{};
}

// Same name, same members, different declaration order.
auto make_order_ab() {
    struct LocalUser {
        int a = 1;
        int b = 2;
    };
    return LocalUser{};
}

auto make_order_ba() {
    struct LocalUser {
        int b = 20;
        int a = 10;
    };
    return LocalUser{};
}

// Different names, same layout: the control case of the "name or layout?" probe.
auto make_alpha() {
    struct LocalAlpha {
        int age = 42;
        int score = 7;
    };
    return LocalAlpha{};
}

auto make_beta() {
    struct LocalBeta {
        int age = 99;
        int score = 3;
    };
    return LocalBeta{};
}

// Nested local aggregates with the same names (Inner and LocalUser).
auto make_nested_1() {
    struct Inner {
        int x = 1;
    };
    struct LocalUser {
        Inner inner;
        int y = 2;
    };
    return LocalUser{};
}

auto make_nested_2() {
    struct Inner {
        int x = 10;
        int z = 30;
    };
    struct LocalUser {
        Inner inner;
        int y = 20;
    };
    return LocalUser{};
}

// Empty local aggregates.
auto make_empty_1() {
    struct LocalUser {};
    return LocalUser{};
}

auto make_empty_2() {
    struct LocalUser {};
    return LocalUser{};
}

// Several members of mixed types, same name and layout.
auto make_wide_1() {
    struct LocalUser {
        std::string name = "bob";
        bool active = true;
        int age = 42;
        long id = 1001;
        int score = 7;
        int level = 3;
    };
    return LocalUser{};
}

auto make_wide_2() {
    struct LocalUser {
        std::string name = "alice";
        bool active = false;
        int age = 84;
        long id = 2002;
        int score = 11;
        int level = 9;
    };
    return LocalUser{};
}

// Local classes of two instantiations of the same function template: same
// name, same layout, and they come from the very same source line.
template <int N> auto make_templated() {
    struct LocalUser {
        int age = N;
        int score = N * 10;
    };
    return LocalUser{};
}

// Local classes declared in lambdas.
const auto make_lambda_1 = [] {
    struct LocalUser {
        int age = 1;
        int score = 2;
    };
    return LocalUser{};
};

const auto make_lambda_2 = [] {
    struct LocalUser {
        int age = 3;
        int score = 4;
    };
    return LocalUser{};
};

// Namespace-scope class and a local class with the same name. The namespace-scope
// one lives in a named namespace so that the local class does not shadow it
// (-Wshadow -Werror).
namespace scoped {
struct User {
    int age = 1;
    int score = 2;
};
} // namespace scoped

auto make_local_shadowing_user() {
    struct User {
        int age = 3;
        int score = 4;
    };
    return User{};
}

using TypeA = decltype(make_a());
using TypeB = decltype(make_b());

// ===========================================================================
// 1. The helpers of detail/members.hpp, used directly
// ===========================================================================

TEST(ReflectionWorkaround, LocalClassesWithTheSameNameAreDistinctTypes) {
    static_assert(!std::is_same_v<TypeA, TypeB>);
    static_assert(!std::is_same_v<decltype(make_same_1()), decltype(make_same_2())>);
    static_assert(!std::is_same_v<decltype(make_templated<1>()), decltype(make_templated<2>())>);

    SUCCEED();
}

// type_id_14 / 18
TEST(ReflectionWorkaround, MemberCountSameNameSameLayout) {
    constexpr std::size_t count_1 = detail::member_count<decltype(make_same_1())>();
    constexpr std::size_t count_2 = detail::member_count<decltype(make_same_2())>();

    EXPECT_EQ(count_1, 2u);
    EXPECT_EQ(count_2, 2u);
}

// type_id_17
TEST(ReflectionWorkaround, MemberCountSameNameDifferentLayout) {
    constexpr std::size_t two = detail::member_count<decltype(make_two_members())>();
    constexpr std::size_t one = detail::member_count<decltype(make_one_member())>();

    EXPECT_EQ(two, 2u);
    EXPECT_EQ(one, 1u);
}

TEST(ReflectionWorkaround, MemberCountOfEmptyLocalClasses) {
    EXPECT_EQ(detail::member_count<decltype(make_empty_1())>(), 0u);
    EXPECT_EQ(detail::member_count<decltype(make_empty_2())>(), 0u);
}

// type_id_20 (control): namespace scope has never been affected by the bug.
TEST(ReflectionWorkaround, MemberCountOfNamespaceScopeClass) {
    EXPECT_EQ(detail::member_count<scoped::User>(), 2u);
}

// type_id_9 / 10 / 12 / 21
TEST(ReflectionWorkaround, MemberNamesAreThoseOfEachLocalClass) {
    EXPECT_EQ((member_name<TypeA, 0>()), "age");
    EXPECT_EQ((member_name<TypeA, 1>()), "score");
    EXPECT_EQ((member_name<TypeB, 0>()), "age");
    EXPECT_EQ((member_name<TypeB, 1>()), "score");

    EXPECT_EQ((member_name<decltype(make_order_ab()), 0>()), "a");
    EXPECT_EQ((member_name<decltype(make_order_ab()), 1>()), "b");
    EXPECT_EQ((member_name<decltype(make_order_ba()), 0>()), "b");
    EXPECT_EQ((member_name<decltype(make_order_ba()), 1>()), "a");
}

// The core of the bug: the splice of member_at<T, I>() must read the member of
// T, not the one of another local class with the same name.
TEST(ReflectionWorkaround, SpliceReadsTheMemberOfTheRightClass) {
    const auto a = make_a();
    const auto b = make_b();

    EXPECT_EQ((value_at<TypeA, 0>(a)), 42);
    EXPECT_EQ((value_at<TypeA, 1>(a)), 7);
    EXPECT_EQ((value_at<TypeB, 0>(b)), 84);
    EXPECT_EQ((value_at<TypeB, 1>(b)), 11);
}

// Interleaved accesses: instantiating the helpers for B after A must not
// change what A reads (and the other way round).
TEST(ReflectionWorkaround, InterleavedAccessesDoNotInterfere) {
    const auto a = make_a();
    const auto b = make_b();

    EXPECT_EQ((value_at<TypeA, 1>(a)), 7);
    EXPECT_EQ((value_at<TypeB, 1>(b)), 11);
    EXPECT_EQ((value_at<TypeA, 1>(a)), 7);
    EXPECT_EQ((value_at<TypeB, 1>(b)), 11);
}

// type_id_11
TEST(ReflectionWorkaround, MemberReflectionIsOwnedByItsClass) {
    static_assert(owner_is_t<TypeA, 0>());
    static_assert(owner_is_t<TypeA, 1>());
    static_assert(owner_is_t<TypeB, 0>());
    static_assert(owner_is_t<TypeB, 1>());

    SUCCEED();
}

// Compile-time checks: the helpers are usable in constant expressions on
// local classes, which is what the renderers rely on (`if constexpr`).
TEST(ReflectionWorkaround, HelpersAreUsableInConstantExpressions) {
    static_assert(detail::member_count<TypeA>() == 2);
    static_assert(detail::member_count<decltype(make_one_member())>() == 1);

    static_assert(has_name<TypeA, 0>("age"));
    static_assert(has_name<TypeA, 1>("score"));
    static_assert(has_name<decltype(make_order_ba()), 0>("b"));

    SUCCEED();
}

// The invariant the workaround relies on: inside ONE constant evaluation, the
// member reflections of two distinct local classes are distinct. If this test
// ever fails, the premise of the workaround is wrong, not just its usage.
TEST(ReflectionWorkaround, MemberReflectionsOfDistinctLocalClassesDiffer) {
    static_assert(members_differ<TypeA, TypeB, 0>());
    static_assert(members_differ<TypeA, TypeB, 1>());

    SUCCEED();
}

// ===========================================================================
// 2. Through the renderers (what the workaround protects)
// ===========================================================================

// --- Same name, same layout, different values (type_id_9/10/12/19/21) ------

TEST(ReflectionWorkaround, JsonSameNameSameLayout) {
    EXPECT_TRUE(contains(json_of(make_a()), "\"user\":{\"age\":42,\"score\":7}"));
    EXPECT_TRUE(contains(json_of(make_b()), "\"user\":{\"age\":84,\"score\":11}"));
}

TEST(ReflectionWorkaround, ConsoleSameNameSameLayout) {
    EXPECT_TRUE(contains(console_of(make_a()), "  user={age=42 score=7}"));
    EXPECT_TRUE(contains(console_of(make_b()), "  user={age=84 score=11}"));
}

// Rendering A, B, A, B must give the same text for the same class each time.
TEST(ReflectionWorkaround, InterleavedRenderingIsStable) {
    const std::string a1 = json_of(make_a());
    const std::string b1 = json_of(make_b());
    const std::string a2 = json_of(make_a());
    const std::string b2 = json_of(make_b());

    EXPECT_TRUE(contains(a1, "\"age\":42"));
    EXPECT_TRUE(contains(b1, "\"age\":84"));
    EXPECT_TRUE(contains(a2, "\"age\":42"));
    EXPECT_TRUE(contains(b2, "\"age\":84"));
}

// --- Same name, same layout, same values (type_id_18) -----------------------

TEST(ReflectionWorkaround, JsonSameNameSameLayoutSameValues) {
    EXPECT_TRUE(contains(json_of(make_same_1()), "\"user\":{\"age\":42,\"score\":7}"));
    EXPECT_TRUE(contains(json_of(make_same_2()), "\"user\":{\"age\":42,\"score\":7}"));
}

// --- Same name, different layouts (type_id_17) ------------------------------

TEST(ReflectionWorkaround, JsonSameNameDifferentMemberCount) {
    EXPECT_TRUE(contains(json_of(make_two_members()), "\"user\":{\"age\":42,\"score\":7}"));
    EXPECT_TRUE(contains(json_of(make_one_member()), "\"user\":{\"age\":84}"));
}

TEST(ReflectionWorkaround, ConsoleSameNameDifferentMemberCount) {
    EXPECT_TRUE(contains(console_of(make_two_members()), "  user={age=42 score=7}"));
    EXPECT_TRUE(contains(console_of(make_one_member()), "  user={age=84}"));
}

// Same member name, different member type: both the value and its formatting
// (number vs string) must follow the right class.
TEST(ReflectionWorkaround, JsonSameMemberNameDifferentMemberType) {
    EXPECT_TRUE(contains(json_of(make_int_value()), "\"user\":{\"value\":5}"));
    EXPECT_TRUE(contains(json_of(make_string_value()), "\"user\":{\"value\":\"five\"}"));
}

TEST(ReflectionWorkaround, ConsoleSameMemberNameDifferentMemberType) {
    EXPECT_TRUE(contains(console_of(make_int_value()), "  user={value=5}"));
    EXPECT_TRUE(contains(console_of(make_string_value()), "  user={value=five}"));
}

// Declaration order is that of each class.
TEST(ReflectionWorkaround, JsonSameMembersDifferentOrder) {
    EXPECT_TRUE(contains(json_of(make_order_ab()), "\"user\":{\"a\":1,\"b\":2}"));
    EXPECT_TRUE(contains(json_of(make_order_ba()), "\"user\":{\"b\":20,\"a\":10}"));
}

TEST(ReflectionWorkaround, ConsoleSameMembersDifferentOrder) {
    EXPECT_TRUE(contains(console_of(make_order_ab()), "  user={a=1 b=2}"));
    EXPECT_TRUE(contains(console_of(make_order_ba()), "  user={b=20 a=10}"));
}

// --- Different names, same layout (control of the name/layout probe) --------

TEST(ReflectionWorkaround, DifferentNamesSameLayout) {
    EXPECT_TRUE(contains(json_of(make_alpha()), "\"user\":{\"age\":42,\"score\":7}"));
    EXPECT_TRUE(contains(json_of(make_beta()), "\"user\":{\"age\":99,\"score\":3}"));
}

// --- Nested and empty aggregates ---------------------------------------------

TEST(ReflectionWorkaround, JsonNestedLocalAggregates) {
    EXPECT_TRUE(contains(json_of(make_nested_1()), "\"user\":{\"inner\":{\"x\":1},\"y\":2}"));
    EXPECT_TRUE(contains(json_of(make_nested_2()), "\"user\":{\"inner\":{\"x\":10,\"z\":30},\"y\":20}"));
}

TEST(ReflectionWorkaround, ConsoleNestedLocalAggregates) {
    EXPECT_TRUE(contains(console_of(make_nested_1()), "  user={inner={x=1} y=2}"));
    EXPECT_TRUE(contains(console_of(make_nested_2()), "  user={inner={x=10 z=30} y=20}"));
}

TEST(ReflectionWorkaround, EmptyLocalAggregates) {
    EXPECT_TRUE(contains(json_of(make_empty_1()), "\"user\":{}"));
    EXPECT_TRUE(contains(json_of(make_empty_2()), "\"user\":{}"));
    EXPECT_TRUE(contains(console_of(make_empty_1()), "  user={}"));
    EXPECT_TRUE(contains(console_of(make_empty_2()), "  user={}"));
}

// --- Wider classes, mixed member types ---------------------------------------

TEST(ReflectionWorkaround, JsonWideLocalAggregates) {
    EXPECT_TRUE(contains(json_of(make_wide_1()),
                         "\"user\":{\"name\":\"bob\",\"active\":true,\"age\":42,\"id\":1001,\"score\":7,\"level\":3}"));
    EXPECT_TRUE(contains(
        json_of(make_wide_2()),
        "\"user\":{\"name\":\"alice\",\"active\":false,\"age\":84,\"id\":2002,\"score\":11,\"level\":9}"));
}

TEST(ReflectionWorkaround, ConsoleWideLocalAggregates) {
    EXPECT_TRUE(contains(console_of(make_wide_1()), "  user={name=bob active=true age=42 id=1001 score=7 level=3}"));
    EXPECT_TRUE(
        contains(console_of(make_wide_2()), "  user={name=alice active=false age=84 id=2002 score=11 level=9}"));
}

// --- Other places where a local class can be declared -----------------------

// Two instantiations of one function template: the local classes have the same
// name, the same layout and the same source location.
TEST(ReflectionWorkaround, LocalClassesOfFunctionTemplateInstantiations) {
    EXPECT_TRUE(contains(json_of(make_templated<1>()), "\"user\":{\"age\":1,\"score\":10}"));
    EXPECT_TRUE(contains(json_of(make_templated<2>()), "\"user\":{\"age\":2,\"score\":20}"));
    EXPECT_TRUE(contains(console_of(make_templated<3>()), "  user={age=3 score=30}"));
}

TEST(ReflectionWorkaround, LocalClassesDeclaredInLambdas) {
    EXPECT_TRUE(contains(json_of(make_lambda_1()), "\"user\":{\"age\":1,\"score\":2}"));
    EXPECT_TRUE(contains(json_of(make_lambda_2()), "\"user\":{\"age\":3,\"score\":4}"));
}

// type_id_19 scenario A + B mixed: a namespace-scope class and a local class
// with the same name.
TEST(ReflectionWorkaround, NamespaceScopeClassAndLocalClassWithTheSameName) {
    EXPECT_TRUE(contains(json_of(scoped::User{}), "\"user\":{\"age\":1,\"score\":2}"));
    EXPECT_TRUE(contains(json_of(make_local_shadowing_user()), "\"user\":{\"age\":3,\"score\":4}"));
}

// Classes declared directly in the test bodies, the way users write them (and
// the way the manual probes did). The test bodies are distinct functions, yet
// the local classes below used to be conflated.
TEST(ReflectionWorkaround, LocalClassDeclaredInATestBodyFirst) {
    struct LocalUser {
        int age = 42;
        int score = 7;
    };

    EXPECT_TRUE(contains(json_of(LocalUser{}), "\"user\":{\"age\":42,\"score\":7}"));
    EXPECT_TRUE(contains(console_of(LocalUser{}), "  user={age=42 score=7}"));
}

TEST(ReflectionWorkaround, LocalClassDeclaredInATestBodySecond) {
    struct LocalUser {
        int age = 84;
        int score = 11;
    };

    EXPECT_TRUE(contains(json_of(LocalUser{}), "\"user\":{\"age\":84,\"score\":11}"));
    EXPECT_TRUE(contains(console_of(LocalUser{}), "  user={age=84 score=11}"));
}

// --- Through the logging macros ---------------------------------------------

TEST(ReflectionWorkaround, ExtraMacroWithLocalAggregates) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    const auto user = make_a();
    const auto other = make_b();

    LOG_INFO(lg, "macro", extra(user));
    EXPECT_TRUE(contains(sink.last(), "  user={age=42 score=7}"));

    LOG_INFO(lg, "macro", extra(other));
    EXPECT_TRUE(contains(sink.last(), "  other={age=84 score=11}"));
}

// Two local aggregates in the same log call.
TEST(ReflectionWorkaround, TwoLocalAggregatesInTheSameLogCall) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{}, sink}};

    lg.log(LogLevel::info, "both", named("first", make_a()), named("second", make_b()));

    EXPECT_TRUE(contains(sink.last(), "\"first\":{\"age\":42,\"score\":7},\"second\":{\"age\":84,\"score\":11}"));
}

} // namespace
