// Copyright (c) Maia

#pragma once

#include <spdlog/spdlog.h>

namespace maia {

inline void LogInstallFormat() {
  // spdlog pattern: [Time][Level][File:Line]: Message
  // - [%H:%M:%S.%e]: HH:MM:SS.ms
  // - [%^%l%$]:      Colored log level
  // - [%s:%#]:       Source location (e.g., main.cpp:17)
  // - %v:            The message
  //
  // NOTE: [File:Line] requires using the SPDLOG_... macros!
  // "[%H:%M:%S.%e][%^%l%$][%s:%#]: %v"
  spdlog::set_pattern("[%H:%M:%S.%e][%^%l%$]: %v");
}

template <typename... Args>
inline void LogTrace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogDebug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogInfo(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogWarning(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogError(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->error(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogCritical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::default_logger_raw()->critical(fmt, std::forward<Args>(args)...);
}

template <typename T>
inline void LogTrace(const T& msg) {
  spdlog::default_logger_raw()->trace(msg);
}

template <typename T>
inline void LogDebug(const T& msg) {
  spdlog::default_logger_raw()->debug(msg);
}

template <typename T>
inline void LogInfo(const T& msg) {
  spdlog::default_logger_raw()->info(msg);
}

template <typename T>
inline void LogWarn(const T& msg) {
  spdlog::default_logger_raw()->warn(msg);
}

template <typename T>
inline void LogError(const T& msg) {
  spdlog::default_logger_raw()->error(msg);
}

template <typename T>
inline void LogCritical(const T& msg) {
  spdlog::default_logger_raw()->critical(msg);
}

}  // namespace maia
