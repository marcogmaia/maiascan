// Copyright (c) Maia

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace maia::application {

struct FileFilter {
  std::string_view name;
  std::string_view spec;
};

class FileDialogs {
 public:
  // Static class - prevent instantiation
  FileDialogs() = delete;

  [[nodiscard]] static std::optional<std::filesystem::path> ShowOpenDialog(
      std::span<const FileFilter> filters = {},
      const std::optional<std::filesystem::path>& default_path = std::nullopt);

  [[nodiscard]] static std::optional<std::filesystem::path> ShowSaveDialog(
      std::span<const FileFilter> filters = {},
      const std::optional<std::filesystem::path>& default_path = std::nullopt,
      const std::optional<std::string>& default_name = std::nullopt);
};

}  // namespace maia::application
