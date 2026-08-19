#pragma once
#include "context.hpp"
#include "json_render.hpp"
#include "sink.hpp"

#include <concepts>
#include <vector>

namespace logchain {

// ===========================================================================
// 4. Logger : relie construction statique du contexte et sink runtime
// ===========================================================================

class Logger {
  public:
    template <typename... SinkT>
        requires(std::derived_from<SinkT, Sink> && ...)
    explicit Logger(SinkT &...sinks) : sinks_{&sinks...} {}

    template <typename... Args> void log(std::string_view message, Args &&...args) {
        auto ctx = make_context(std::forward<Args>(args)...);
        write(ctx, message);
    }

  private:
    template <typename NodeT> void write(const Context<NodeT> &ctx, std::string_view message) {
        for (Sink *sink : sinks_) {
            // Rendu JSON entièrement statique (récursion sur le DAG de types,
            // résolue à la compilation). Sink ne voit jamais que le résultat :
            // un std::string_view déjà formé. C'est le SEUL point de dispatch
            // runtime (virtual) de tout le pipeline log() -> écriture.
            sink->write(build_log_line(ctx, message));
        }
    }

    std::vector<Sink *> sinks_;
};

} // namespace logchain
