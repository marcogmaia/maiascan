// Copyright (c) Maia

#include "maia/application/file_dialogs.h"

#include <nfd.h>

#include "maia/logging.h"

namespace maia::application {

bool FileDialogs::Init() {
  if (NFD_Init() != NFD_OKAY) {
    LogError("Failed to initialize NFD: {}", NFD_GetError());
    return false;
  }
  return true;
}

void FileDialogs::Quit() {
  NFD_Quit();
}

std::optional<std::filesystem::path> FileDialogs::ShowOpenDialog(
    std::span<const FileFilter> filters,
    const std::optional<std::filesystem::path>& default_path) {
  std::vector<nfdu8filteritem_t> nfd_filters;
  nfd_filters.reserve(filters.size());
  for (const auto& filter : filters) {
    nfd_filters.push_back({reinterpret_cast<const nfdu8char_t*>(filter.name),
                           reinterpret_cast<const nfdu8char_t*>(filter.spec)});
  }

  nfdu8char_t* out_path = nullptr;
  const nfdu8char_t* default_path_ptr = nullptr;
  std::u8string default_path_u8;

  if (default_path) {
    default_path_u8 = default_path->u8string();
    default_path_ptr =
        reinterpret_cast<const nfdu8char_t*>(default_path_u8.c_str());
  }

  nfdresult_t result =
      NFD_OpenDialogU8(&out_path,
                       nfd_filters.data(),
                       static_cast<nfdfiltersize_t>(nfd_filters.size()),
                       default_path_ptr);

  if (result == NFD_OKAY) {
    std::filesystem::path path =
        std::filesystem::path(reinterpret_cast<const char8_t*>(out_path));
    NFD_FreePathU8(out_path);
    return path;
  } else if (result == NFD_ERROR) {
    LogError("NFD Error: {}", NFD_GetError());
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> FileDialogs::ShowSaveDialog(
    std::span<const FileFilter> filters,
    const std::optional<std::filesystem::path>& default_path,
    const std::optional<std::string>& default_name) {
  std::vector<nfdu8filteritem_t> nfd_filters;
  nfd_filters.reserve(filters.size());
  for (const auto& filter : filters) {
    nfd_filters.push_back({reinterpret_cast<const nfdu8char_t*>(filter.name),
                           reinterpret_cast<const nfdu8char_t*>(filter.spec)});
  }

  nfdu8char_t* out_path = nullptr;
  const nfdu8char_t* default_path_ptr = nullptr;
  std::u8string default_path_u8;
  if (default_path) {
    default_path_u8 = default_path->u8string();
    default_path_ptr =
        reinterpret_cast<const nfdu8char_t*>(default_path_u8.c_str());
  }

  const nfdu8char_t* default_name_ptr = nullptr;
  std::u8string default_name_u8;
  if (default_name) {
    default_name_u8 =
        std::u8string(reinterpret_cast<const char8_t*>(default_name->c_str()));
    default_name_ptr =
        reinterpret_cast<const nfdu8char_t*>(default_name_u8.c_str());
  }

  nfdresult_t result =
      NFD_SaveDialogU8(&out_path,
                       nfd_filters.data(),
                       static_cast<nfdfiltersize_t>(nfd_filters.size()),
                       default_path_ptr,
                       default_name_ptr);

  if (result == NFD_OKAY) {
    std::filesystem::path path =
        std::filesystem::path(reinterpret_cast<const char8_t*>(out_path));
    NFD_FreePathU8(out_path);
    return path;
  } else if (result == NFD_ERROR) {
    LogError("NFD Error: {}", NFD_GetError());
  }

  return std::nullopt;
}

}  // namespace maia::application
