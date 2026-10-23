[Guide index](../index.md) · Next step [Stack Traces](stack_traces.md)

# Advanced

> **This section is for readers who want to understand what happens under the hood, or who have production constraints to meet.**  
> It describes guarantees, costs, limits and design choices of `logchain`. The pages are technical and assume that you have read the corresponding page of the [Guide](../index.md).
>
> **You do not need it to use the library.** The Guide is self-sufficient: every feature described here has a short, task-oriented introduction there.

## Pages

| Page | Read it if... |
| :--- | :------------ |
| [Stack Traces](stack_traces.md) | you want to print where an exception was thrown, you need to know exactly when a trace is printed and what it costs, or you build without CMake. |
| [Evaluation Model and Level Filtering](evaluation_model.md) | you log from performance-sensitive code and need to know which APIs evaluate their arguments, or you have a macro name collision. |
| [FileSink: Guarantees and Edge Cases](file_sink.md) | you share a `FileSink` between loggers or threads, or you need to know how rotation and write errors behave. |

## How the pages are organized

Each page follows the same layout, so that you can jump to what you need:

1. **In brief**: what the page covers and who should read it.
2. **Requirements**, when there are any.
3. **Reference**: options, signatures and defaults.
4. **Behavior and guarantees**: what the library promises.
5. **Cost**: when it is paid, and by whom.
6. **Design rationale**: why it works this way.
7. **Pitfalls**: the mistakes worth knowing about in advance.

[Guide index](../index.md) · Next step [Stack Traces](stack_traces.md)
