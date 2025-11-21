// Copyright (c) Maia

#pragma once

#include <source_location>

#include <spdlog/spdlog.h>

namespace maia {

namespace detail {

inline spdlog::source_loc ToSpdlogLoc(const std::source_location& loc) {
  return spdlog::source_loc{
      loc.file_name(), static_cast<int>(loc.line()), loc.function_name()};
}

template <typename... Args>
struct WithSourceLoc {
  spdlog::format_string_t<Args...> str;
  std::source_location loc;

  // consteval constructor: Implicitly captures string and location.
  // It will FAIL TO COMPILE if 's' is not valid for 'Args'
  template <typename FormatStr>
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  consteval WithSourceLoc(
      const FormatStr& s,
      std::source_location loc = std::source_location::current())
      : str(s),
        loc(loc) {}
};

}  // namespace detail

/// \brief Configures the global spdlog formatting pattern.
///
/// Sets the output format to: [Time][Level][File:Line]: Message
///
/// Pattern Breakdown:
/// - [%H:%M:%S.%e] : Time (HH:MM:SS.ms)
/// - [%^%l%$]      : Log Level (Short name), wrapped in color codes (%^ start,
/// %$ end)
/// - [%s:%#]       : Source File Basename : Line Number
/// - %v            : The actual log message
///
/// \example [14:30:05.123][info][main.cpp:42]: Application started
inline void LogInstallFormat() {
  spdlog::set_pattern("[%H:%M:%S.%e][%^%l%$][%s:%#]: %v");
}

// Variadic Overloads (Format String).
//
// We use std::type_identity_t to stop the compiler from trying to guess Args
// from the first parameter. It forces the compiler to guess Args from the
// 'args' variables, then go back and build the struct.

template <typename... Args>
inline void LogTrace(
    detail::WithSourceLoc<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
  spdlog::default_logger_raw()->log(detail::ToSpdlogLoc(fmt_loc.loc),
                                    spdlog::level::trace,
                                    fmt_loc.str,
                                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogDebug(
    detail::WithSourceLoc<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
  spdlog::default_logger_raw()->log(detail::ToSpdlogLoc(fmt_loc.loc),
                                    spdlog::level::debug,
                                    fmt_loc.str,
                                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogInfo(
    detail::WithSourceLoc<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
  spdlog::default_logger_raw()->log(detail::ToSpdlogLoc(fmt_loc.loc),
                                    spdlog::level::info,
                                    fmt_loc.str,
                                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogWarning(
    detail::WithSourceLoc<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
  spdlog::default_logger_raw()->log(detail::ToSpdlogLoc(fmt_loc.loc),
                                    spdlog::level::warn,
                                    fmt_loc.str,
                                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogError(
    detail::WithSourceLoc<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
  spdlog::default_logger_raw()->log(detail::ToSpdlogLoc(fmt_loc.loc),
                                    spdlog::level::err,
                                    fmt_loc.str,
                                    std::forward<Args>(args)...);
}

// Single Argument Overloads (No formatting).

template <typename T>
inline void LogTrace(
    const T& msg, std::source_location loc = std::source_location::current()) {
  spdlog::default_logger_raw()->log(
      detail::ToSpdlogLoc(loc), spdlog::level::trace, "{}", msg);
}

template <typename T>
inline void LogDebug(
    const T& msg, std::source_location loc = std::source_location::current()) {
  spdlog::default_logger_raw()->log(
      detail::ToSpdlogLoc(loc), spdlog::level::debug, "{}", msg);
}

template <typename T>
inline void LogInfo(
    const T& msg, std::source_location loc = std::source_location::current()) {
  spdlog::default_logger_raw()->log(
      detail::ToSpdlogLoc(loc), spdlog::level::info, "{}", msg);
}

template <typename T>
inline void LogWarning(
    const T& msg, std::source_location loc = std::source_location::current()) {
  spdlog::default_logger_raw()->log(
      detail::ToSpdlogLoc(loc), spdlog::level::warn, "{}", msg);
}

template <typename T>
inline void LogError(
    const T& msg, std::source_location loc = std::source_location::current()) {
  spdlog::default_logger_raw()->log(
      detail::ToSpdlogLoc(loc), spdlog::level::err, "{}", msg);
}

}  // namespace maia
