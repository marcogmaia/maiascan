// Copyright (c) Maia

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace maia::application {

struct FileFilter {
  const char* name;
  const char* spec;
};

class FileDialogs {
 public:
  FileDialogs();
  ~FileDialogs();

  FileDialogs(const FileDialogs&) = delete;
  FileDialogs& operator=(const FileDialogs&) = delete;

  [[nodiscard]] bool IsValid() const {
    return initialized_;
  }

  static std::optional<std::filesystem::path> ShowOpenDialog(
      std::span<const FileFilter> filters = {},
      const std::optional<std::filesystem::path>& default_path = std::nullopt);

  static std::optional<std::filesystem::path> ShowSaveDialog(
      std::span<const FileFilter> filters = {},
      const std::optional<std::filesystem::path>& default_path = std::nullopt,
      const std::optional<std::string>& default_name = std::nullopt);

 private:
  bool initialized_ = false;
};

}  // namespace maia::application
