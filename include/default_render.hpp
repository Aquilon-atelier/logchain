#pragma once

#include <type_traits>

namespace logchain {

template <typename SinkT> struct default_renderer_for;

// Convenience alias.
template <typename SinkT>
using default_renderer_for_t = typename default_renderer_for<std::remove_cvref_t<SinkT>>::type;

} // namespace logchain
