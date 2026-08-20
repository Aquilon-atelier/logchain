#pragma once

#include "console_render.hpp"
#include "context.hpp"
#include "json_render.hpp"
#include "sink.hpp"

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

namespace logchain {

// ===========================================================================
// Default renderer associated with a Sink.
// ===========================================================================

template <typename SinkT> struct default_renderer_for;

template <typename SinkT>
using default_renderer_for_t = typename default_renderer_for<std::remove_cvref_t<SinkT>>::type;

// ===========================================================================
// Target
// ===========================================================================

template <typename RendererT> class Target {
  public:
    template <typename SinkT>
        requires std::derived_from<SinkT, Sink>
    Target(RendererT renderer, SinkT &sink) : renderer_(std::move(renderer)), sink_(&sink) {}

    template <typename Event, typename NodeT> void write(const Event &event, const Context<NodeT> &context) {

        const std::string rendered = renderer_.render(event, context);

        sink_->write(rendered);
    }

  private:
    RendererT renderer_;
    Sink *sink_;
};

template <typename RendererT, typename SinkT>
    requires std::derived_from<SinkT, Sink>
Target(RendererT, SinkT &) -> Target<RendererT>;

// ===========================================================================
// Default target
// ===========================================================================

template <typename SinkT> auto make_target(SinkT &sink) {
    using RendererT = default_renderer_for_t<SinkT>;

    return Target{RendererT{}, sink};
}

// ===========================================================================
// Built-in defaults
// ===========================================================================

template <> struct default_renderer_for<ConsoleSink> {
    using type = ConsoleRenderer;
};

template <> struct default_renderer_for<FileSink> {
    using type = JsonRenderer;
};

} // namespace logchain