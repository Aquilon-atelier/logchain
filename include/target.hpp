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

template <typename SinkT> auto target(SinkT &sink) {
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