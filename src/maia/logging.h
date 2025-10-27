// Copyright (c) Maia

#pragma once

#include <spdlog/spdlog.h>

namespace maia {

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
